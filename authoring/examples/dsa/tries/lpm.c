#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

typedef struct { uint32_t addr; uint8_t len; uint8_t nh; } Route;   /* nh 0 is reserved for "no route" */

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }
static inline uint32_t mask_of(int len) { return len == 0 ? 0 : ~0u << (32 - len); }

/* ================= 1. linear scan: the specification of longest-prefix match ================= */
static int lookup_linear(const Route *r, size_t n, uint32_t ip) {
    int best_len = -1, nh = 0;
    for (size_t i = 0; i < n; i++) if ((ip & mask_of(r[i].len)) == r[i].addr && r[i].len > best_len) { best_len = r[i].len; nh = r[i].nh; }
    return nh;
}

/* ================= 2. binary trie: one bit per level, up to 32 dependent loads ================= */
typedef struct { uint32_t child[2]; uint8_t nh; } BNode;           /* children are indexes into one pool (0 = none) */
typedef struct { BNode *pool; uint32_t n; } BTrie;
static void bt_init(BTrie *t, size_t cap) { t->pool = calloc(cap, sizeof(BNode)); t->n = 1; }
static void bt_insert(BTrie *t, const Route *r) {
    uint32_t cur = 0;
    for (int i = 0; i < r->len; i++) {
        int b = (r->addr >> (31 - i)) & 1;
        if (!t->pool[cur].child[b]) t->pool[cur].child[b] = t->n++;
        cur = t->pool[cur].child[b];
    }
    t->pool[cur].nh = r->nh;
}
static int bt_lookup(const BTrie *t, uint32_t ip) {
    uint32_t cur = 0; int nh = t->pool[0].nh;
    for (int i = 0; i < 32; i++) {
        cur = t->pool[cur].child[(ip >> (31 - i)) & 1];
        if (!cur) break;
        if (t->pool[cur].nh) nh = t->pool[cur].nh;               /* remember the longest match seen so far */
    }
    return nh;
}

/* ================= 3. multibit trie, stride 8: at most 4 loads; prefixes expanded to byte boundaries ================= */
typedef struct MNode { struct MNode *child[256]; uint8_t nh[256]; } MNode;
static MNode *mt_node(size_t *count) { (*count)++; return calloc(1, sizeof(MNode)); }
/* routes must be inserted in ascending prefix length so a longer prefix overwrites the expansion of a shorter one */
static void mt_insert(MNode *root, size_t *count, const Route *r) {
    if (r->len == 0) { for (int e = 0; e < 256; e++) root->nh[e] = r->nh; return; }   /* default route: handled as /0 in the root */
    MNode *n = root; int level = (r->len - 1) / 8;
    for (int l = 0; l < level; l++) {
        int byte = (r->addr >> (24 - 8 * l)) & 255;
        if (!n->child[byte]) n->child[byte] = mt_node(count);
        n = n->child[byte];
    }
    int bits = r->len - 8 * level;                                /* 1..8 significant bits inside this node's byte */
    int first = (r->addr >> (24 - 8 * level)) & 255 & (0xFF << (8 - bits));
    for (int e = first; e < first + (1 << (8 - bits)); e++) n->nh[e] = r->nh;
}
static void mt_free(MNode *n) { if (!n) return; for (int e = 0; e < 256; e++) mt_free(n->child[e]); free(n); }
static int mt_lookup(const MNode *root, uint32_t ip) {
    int nh = 0; const MNode *n = root;
    for (int l = 0; l < 4 && n; l++) {
        int byte = (ip >> (24 - 8 * l)) & 255;
        if (n->nh[byte]) nh = n->nh[byte];
        n = n->child[byte];
    }
    return nh;
}

/* ================= 4. DIR-24-8: one table of 2^24 entries, plus 256-entry second-level groups for /25../32 ================= */
#define EXT 0x8000u                                               /* entry flag: low 15 bits are a tbl8 group index */
typedef struct { uint16_t *t24; uint16_t *t8; uint32_t groups, cap; } Dir;
static void dir_init(Dir *d) { d->t24 = calloc(1u << 24, sizeof(uint16_t)); d->groups = 0; d->cap = 20000; d->t8 = calloc((size_t)d->cap * 256, sizeof(uint16_t)); }
static void dir_insert(Dir *d, const Route *r) {                  /* ascending prefix length */
    if (r->len <= 24) {
        uint32_t first = r->addr >> 8, cnt = 1u << (24 - r->len);
        for (uint32_t i = first; i < first + cnt; i++) {
            if (d->t24[i] & EXT) { uint16_t *g = d->t8 + (size_t)(d->t24[i] & ~EXT) * 256; for (int k = 0; k < 256; k++) g[k] = r->nh; }
            else d->t24[i] = r->nh;
        }
    } else {
        uint32_t i = r->addr >> 8;
        if (!(d->t24[i] & EXT)) {                                 /* split: allocate a group pre-filled with the covering next hop */
            assert(d->groups < d->cap);
            uint16_t *g = d->t8 + (size_t)d->groups * 256; for (int k = 0; k < 256; k++) g[k] = d->t24[i];
            d->t24[i] = (uint16_t)(EXT | d->groups++);
        }
        uint16_t *g = d->t8 + (size_t)(d->t24[i] & ~EXT) * 256;
        uint32_t first = r->addr & 255, cnt = 1u << (32 - r->len);
        for (uint32_t k = first; k < first + cnt; k++) g[k] = r->nh;
    }
}
static inline int dir_lookup(const Dir *d, uint32_t ip) {
    uint16_t e = d->t24[ip >> 8];
    return e & EXT ? d->t8[(size_t)(e & ~EXT) * 256 + (ip & 255)] : e;
}

static int cmp_route(const void *a, const void *b) {
    const Route *x = a, *y = b;
    if (x->len != y->len) return x->len < y->len ? -1 : 1;
    return x->addr < y->addr ? -1 : x->addr > y->addr;
}

int main(void) {
    /* A synthetic table with a BGP-like length distribution (about 55 % /24s). Not real routes. */
    static const struct { int len, pct; } dist[] = { {8,0}, {12,1}, {16,6}, {17,2}, {18,3}, {19,4}, {20,5}, {21,4}, {22,8}, {23,6}, {24,55}, {25,1}, {27,1}, {29,1}, {32,3} };
    size_t want = 200000, n = 0; Route *tab = malloc((want + 8) * sizeof *tab);
    for (size_t di = 0; di < sizeof dist / sizeof *dist; di++)
        for (size_t k = 0; k < want * (size_t)dist[di].pct / 100; k++) {
            int len = dist[di].len; uint32_t a = rnd() ^ (rnd() << 16);
            tab[n++] = (Route){ a & mask_of(len), (uint8_t)len, (uint8_t)(1 + rnd() % 200) };
        }
    tab[n++] = (Route){ 0, 0, 250 };                              /* default route */
    qsort(tab, n, sizeof *tab, cmp_route);
    size_t m = 0; for (size_t i = 0; i < n; i++) if (i == 0 || tab[i].addr != tab[m - 1].addr || tab[i].len != tab[m - 1].len) tab[m++] = tab[i];
    n = m;

    BTrie bt; bt_init(&bt, n * 32 + 2); size_t mcount = 0; MNode *mt = mt_node(&mcount); Dir dir; dir_init(&dir);
    for (size_t i = 0; i < n; i++) { bt_insert(&bt, &tab[i]); mt_insert(mt, &mcount, &tab[i]); dir_insert(&dir, &tab[i]); }

    /* addresses: 80 % inside some route (random host bits), 20 % uniformly random */
    enum { Q = 4000000 }; uint32_t *ip = malloc((size_t)Q * sizeof *ip);
    for (int i = 0; i < Q; i++) { if (rnd() % 5) { const Route *r = &tab[rnd() % n]; ip[i] = r->addr | (rnd() & ~mask_of(r->len)); } else ip[i] = rnd() ^ (rnd() << 16); }

    /* correctness: all three structures against the linear scan */
    int checked = 20000;
    for (int i = 0; i < checked; i++) {
        int want_nh = lookup_linear(tab, n, ip[i]);
        assert(bt_lookup(&bt, ip[i]) == want_nh); assert(mt_lookup(mt, ip[i]) == want_nh); assert(dir_lookup(&dir, ip[i]) == want_nh);
    }
    printf("%zu routes (unique prefixes, incl. a default route); %d random destinations: binary trie, stride-8 trie and DIR-24-8 agree with linear scan\n\n", n, checked);

    long sink = 0; double t0;
    t0 = now(); for (int i = 0; i < 2000; i++) sink += lookup_linear(tab, n, ip[i]); double lin = (now() - t0) / 2000 * 1e9;
    t0 = now(); for (int i = 0; i < Q; i++) sink += bt_lookup(&bt, ip[i]); double tb = (now() - t0) / Q * 1e9;
    t0 = now(); for (int i = 0; i < Q; i++) sink += mt_lookup(mt, ip[i]); double tm = (now() - t0) / Q * 1e9;
    t0 = now(); for (int i = 0; i < Q; i++) sink += dir_lookup(&dir, ip[i]); double td = (now() - t0) / Q * 1e9;
    printf("%-28s %12s %14s\n", "structure", "ns / lookup", "memory");
    printf("%-28s %12.0f %11.1f KB\n", "linear scan", lin, n * sizeof(Route) / 1024.0);
    printf("%-28s %12.0f %11.1f MB   (%u nodes)\n", "binary trie (1 bit/level)", tb, bt.n * sizeof(BNode) / 1048576.0, bt.n);
    printf("%-28s %12.0f %11.1f MB   (%zu nodes)\n", "multibit trie (stride 8)", tm, mcount * sizeof(MNode) / 1048576.0, mcount);
    printf("%-28s %12.0f %11.1f MB   (%u tbl8 groups)\n", "DIR-24-8 (2^24 table)", td, ((1u << 24) * 2 + (double)dir.groups * 512) / 1048576.0, dir.groups);
    if (sink == 42) puts("");
    free(tab); free(ip); free(bt.pool); mt_free(mt); free(dir.t24); free(dir.t8);
    return 0;
}
