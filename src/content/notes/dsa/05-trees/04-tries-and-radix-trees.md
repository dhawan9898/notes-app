---
title: Tries, Radix Trees and Longest-Prefix Match
track: dsa
module: Trees
moduleOrder: 5
order: 4
level: advanced
summary: "Prefix trees from the 26-way trie to compressed radix trees, then the structure that justifies them in networking — IPv4 longest-prefix match with binary, multibit and DIR-24-8 tables, checked against a linear scan and measured."
sources:
  - label: "Fredkin — Trie Memory (CACM 3(9), 1960)"
  - label: "Morrison — PATRICIA, Practical Algorithm To Retrieve Information Coded in Alphanumeric (J. ACM 15(4), 1968)"
  - label: "Gupta, Lin, McKeown — Routing Lookups in Hardware at Memory Access Speeds (IEEE INFOCOM 1998)"
  - label: "Nilsson & Karlsson — IP-Address Lookup Using LC-Tries (IEEE JSAC 17(6), 1999)"
  - label: "Eatherton, Varghese, Dittia — Tree Bitmap, Hardware/Software IP Lookups with Incremental Updates (ACM SIGCOMM CCR 34(2), 2004)"
  - label: "Leis, Kemper, Neumann — The Adaptive Radix Tree, ARTful Indexing for Main-Memory Databases (ICDE 2013)"
  - label: "RFC 4632 — Classless Inter-domain Routing (CIDR), the Internet Address Assignment and Aggregation Plan"
    url: https://www.rfc-editor.org/rfc/rfc4632
  - label: "Linux kernel documentation — XArray"
    url: https://www.kernel.org/doc/html/latest/core-api/xarray.html
  - label: "DPDK Programmer's Guide — LPM Library"
    url: https://doc.dpdk.org/guides/prog_guide/lpm_lib.html
---

A **trie** (from re*trie*val, usually pronounced "try") indexes keys by their *symbols* instead of by comparing whole keys: the path from the root spells the key, so lookup cost is `O(k)` in the key length `k`, independent of how many keys are stored. That makes tries the natural structure whenever the question is about **prefixes** — autocomplete, dictionary matching, routing tables, URL routers, IP access lists — and whenever the keys are bit strings, as in IP addresses.

## The design space

Every trie answers one question: how do you store the children of a node?

| Node representation | Child lookup | Memory per node | Notes |
|---|---|---|---|
| Array of `σ` pointers (σ = alphabet size) | O(1) | `8σ` bytes | Fast; wasteful for sparse nodes (26 letters → 208 B, bytes → 2 KB) |
| Linked list of children (first-child / next-sibling) | O(σ) | ~24 B | Compact; slow for wide nodes |
| Sorted array + binary search | O(log σ) | proportional to children | Good for medium fan-out |
| Bitmap + packed array (HAMT, Tree Bitmap, poptrie) | O(1) with popcount | 8σ bits + 8 per child | Dense-fast and sparse-compact |
| Hash table per node | O(1) expected | high constant | Rarely worthwhile |
| Adaptive node types (ART: 4, 16, 48, 256 children) | O(1) | adapts to fan-out | Best general in-memory design |

And one orthogonal choice: **path compression**. A chain of nodes with a single child wastes a node per symbol; a **radix tree** (Patricia trie) stores a whole substring on each edge, so a tree with `n` keys has at most `2n−1` nodes no matter how long the keys are. Compression complicates insertion (an edge may need to be *split*) but is what makes tries practical for long, sparse keys.

## Words: four ways to answer "complete this prefix"

The program builds three trie variants and the obvious alternative, a sorted array searched with binary search, on a deterministic synthetic vocabulary (syllable combinations, so these are *not* English words), inserts the words in scrambled order, and checks that for 5000 random prefixes all four structures return the same first ten completions in alphabetical order.

```c
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

/* ===== A. 26-way array trie: O(len) lookup, but 26 pointers per node ===== */
typedef struct T26 { struct T26 *c[26]; uint8_t term; } T26;
static size_t t26_nodes = 1;
static void t26_insert(T26 *r, const char *w) {
    for (; *w; w++) { int i = *w - 'a'; if (!r->c[i]) { r->c[i] = calloc(1, sizeof(T26)); t26_nodes++; } r = r->c[i]; }
    r->term = 1;
}
static void t26_collect(const T26 *n, char *buf, int len, char (*out)[32], int *cnt, int max) {
    if (*cnt >= max) return;
    if (n->term) { memcpy(out[*cnt], buf, (size_t)len); out[*cnt][len] = 0; (*cnt)++; }
    for (int i = 0; i < 26; i++) if (n->c[i]) { buf[len] = (char)('a' + i); t26_collect(n->c[i], buf, len + 1, out, cnt, max); }
}
static int t26_complete(const T26 *r, const char *p, char (*out)[32], int max) {
    char buf[32]; int len = 0, cnt = 0;
    for (; *p; p++) { r = r->c[*p - 'a']; if (!r) return 0; buf[len++] = *p; }
    t26_collect(r, buf, len, out, &cnt, max); return cnt;
}
static void t26_free(T26 *n) { for (int i = 0; i < 26; i++) if (n->c[i]) t26_free(n->c[i]); free(n); }

/* ===== B. first-child / next-sibling trie: 24 bytes per node, children scanned linearly (kept sorted) ===== */
typedef struct TS { struct TS *first, *next; char ch; uint8_t term; } TS;
static size_t ts_nodes = 1;
static void ts_insert(TS *r, const char *w) {
    for (; *w; w++) {
        TS **link = &r->first;
        while (*link && (*link)->ch < *w) link = &(*link)->next;
        if (!*link || (*link)->ch != *w) { TS *n = calloc(1, sizeof *n); n->ch = *w; n->next = *link; *link = n; ts_nodes++; }
        r = *link;
    }
    r->term = 1;
}
static void ts_collect(const TS *n, char *buf, int len, char (*out)[32], int *cnt, int max) {
    if (*cnt >= max) return;
    if (n->term) { memcpy(out[*cnt], buf, (size_t)len); out[*cnt][len] = 0; (*cnt)++; }
    for (const TS *c = n->first; c; c = c->next) { buf[len] = c->ch; ts_collect(c, buf, len + 1, out, cnt, max); }
}
static int ts_complete(const TS *r, const char *p, char (*out)[32], int max) {
    char buf[32]; int len = 0, cnt = 0;
    for (; *p; p++) { const TS *c = r->first; while (c && c->ch < *p) c = c->next; if (!c || c->ch != *p) return 0; r = c; buf[len++] = *p; }
    ts_collect(r, buf, len, out, &cnt, max); return cnt;
}
static void ts_free(TS *n) { while (n) { TS *nx = n->next; ts_free(n->first); free(n); n = nx; } }

/* ===== C. compressed radix tree (Patricia-style): edges carry whole substrings, so unary chains collapse ===== */
typedef struct RX { char *lab; int len; uint8_t term; struct RX *first, *next; } RX;
static size_t rx_nodes = 1, rx_label_bytes = 0;
static RX *rx_new(const char *lab, int len) { RX *n = calloc(1, sizeof *n); n->lab = malloc((size_t)len); memcpy(n->lab, lab, (size_t)len); n->len = len; rx_nodes++; rx_label_bytes += (size_t)len; return n; }
static void rx_insert(RX *r, const char *w) {
    for (;;) {
        if (!*w) { r->term = 1; return; }
        RX **link = &r->first;
        while (*link && (*link)->lab[0] < *w) link = &(*link)->next;
        RX *c = *link;
        if (!c || c->lab[0] != *w) { RX *n = rx_new(w, (int)strlen(w)); n->term = 1; n->next = c; *link = n; return; }
        int k = 0; while (k < c->len && w[k] && c->lab[k] == w[k]) k++;
        if (k < c->len) {                                          /* split the edge at k */
            RX *tail = rx_new(c->lab + k, c->len - k); tail->term = c->term; tail->first = c->first; tail->next = NULL;
            c->len = k; c->term = 0; c->first = tail;
        }
        r = c; w += k;
    }
}
static void rx_collect(const RX *n, char *buf, int len, char (*out)[32], int *cnt, int max) {
    if (*cnt >= max) return;
    if (n->term) { memcpy(out[*cnt], buf, (size_t)len); out[*cnt][len] = 0; (*cnt)++; }
    for (const RX *c = n->first; c; c = c->next) { memcpy(buf + len, c->lab, (size_t)c->len); rx_collect(c, buf, len + c->len, out, cnt, max); }
}
static int rx_complete(const RX *r, const char *p, char (*out)[32], int max) {
    char buf[32]; int len = 0, cnt = 0;
    while (*p) {
        const RX *c = r->first; while (c && c->lab[0] < *p) c = c->next;
        if (!c || c->lab[0] != *p) return 0;
        int k = 0; while (k < c->len && p[k] && c->lab[k] == p[k]) k++;
        if (p[k] && k < c->len) return 0;                          /* mismatch inside the edge */
        memcpy(buf + len, c->lab, (size_t)c->len); len += c->len;  /* prefix may end in the middle of an edge: take the whole edge */
        p += k; r = c;
        if (!*p) break;
    }
    rx_collect(r, buf, len, out, &cnt, max); return cnt;
}
static void rx_free(RX *n) { while (n) { RX *nx = n->next; rx_free(n->first); free(n->lab); free(n); n = nx; } }

/* ===== D. sorted array of strings + binary search for the first word >= prefix ===== */
static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static int arr_complete(char **w, size_t n, const char *p, char (*out)[32], int max) {
    size_t lo = 0, hi = n, pl = strlen(p);
    while (lo < hi) { size_t m = (lo + hi) / 2; if (strcmp(w[m], p) < 0) lo = m + 1; else hi = m; }
    int cnt = 0; for (; lo < n && cnt < max && strncmp(w[lo], p, pl) == 0; lo++) strcpy(out[cnt++], w[lo]);
    return cnt;
}

int main(void) {
    /* synthetic vocabulary: pronounceable-ish words built from syllables, deduplicated */
    static const char *syl[] = { "ka","lo","mi","ra","te","su","no","vi","an","el","or","ing","tion","er","st","pr","con","de","re","un","ta","ne","po","ly" };
    enum { WANT = 200000 };
    char **raw = malloc(WANT * sizeof *raw); size_t n = 0;
    while (n < WANT) { char w[32] = ""; int parts = 2 + (int)(rnd() % 3); for (int i = 0; i < parts; i++) strcat(w, syl[rnd() % 24]); if (strlen(w) < 30) raw[n++] = strdup(w); }
    qsort(raw, n, sizeof *raw, cmp_str);
    size_t m = 0; for (size_t i = 0; i < n; i++) { if (m && !strcmp(raw[m - 1], raw[i])) free(raw[i]); else raw[m++] = raw[i]; }
    n = m;
    size_t chars = 0; for (size_t i = 0; i < n; i++) chars += strlen(raw[i]);

    T26 *t26 = calloc(1, sizeof *t26); TS *ts = calloc(1, sizeof *ts); RX *rx = calloc(1, sizeof *rx);
    for (size_t i = 0; i < n; i++) { char *w = raw[(i * 7919) % n]; t26_insert(t26, w); ts_insert(ts, w); rx_insert(rx, w); }   /* scrambled insertion order */

    /* correctness: first 10 completions of 5000 random prefixes must be identical in all four structures */
    char (*o1)[32] = malloc(10 * 32), (*o2)[32] = malloc(10 * 32), (*o3)[32] = malloc(10 * 32), (*o4)[32] = malloc(10 * 32);
    int nonempty = 0;
    for (int q = 0; q < 5000; q++) {
        const char *w = raw[rnd() % n]; char p[32]; int pl = 1 + (int)(rnd() % (uint32_t)strlen(w)); memcpy(p, w, (size_t)pl); p[pl] = 0;
        int a = t26_complete(t26, p, o1, 10), b = ts_complete(ts, p, o2, 10), c = rx_complete(rx, p, o3, 10), d = arr_complete(raw, n, p, o4, 10);
        assert(a == b && b == c && c == d); nonempty += a > 0;
        for (int i = 0; i < a; i++) assert(!strcmp(o1[i], o2[i]) && !strcmp(o2[i], o3[i]) && !strcmp(o3[i], o4[i]));
    }
    printf("%zu distinct words (%.1f MB of characters); first-10 completions of 5000 prefixes identical in all structures (%d non-empty)\n\n", n, chars / 1048576.0, nonempty);

    enum { Q = 200000 }; char (*pre)[16] = malloc((size_t)Q * 16);
    for (int i = 0; i < Q; i++) { const char *w = raw[rnd() % n]; int pl = 2 + (int)(rnd() % 4); if (pl > (int)strlen(w)) pl = (int)strlen(w); memcpy(pre[i], w, (size_t)pl); pre[i][pl] = 0; }
    long sink = 0; double t0;
    printf("%-34s %12s %14s %16s\n", "structure", "nodes", "memory", "ns / complete(10)");
    t0 = now(); for (int i = 0; i < Q; i++) sink += t26_complete(t26, pre[i], o1, 10); double a = (now() - t0) / Q * 1e9;
    printf("%-34s %12zu %11.1f MB %16.0f\n", "26-way array trie", t26_nodes, t26_nodes * sizeof(T26) / 1048576.0, a);
    t0 = now(); for (int i = 0; i < Q; i++) sink += ts_complete(ts, pre[i], o2, 10); double b = (now() - t0) / Q * 1e9;
    printf("%-34s %12zu %11.1f MB %16.0f\n", "first-child/next-sibling trie", ts_nodes, ts_nodes * sizeof(TS) / 1048576.0, b);
    t0 = now(); for (int i = 0; i < Q; i++) sink += rx_complete(rx, pre[i], o3, 10); double c = (now() - t0) / Q * 1e9;
    printf("%-34s %12zu %11.1f MB %16.0f\n", "compressed radix tree", rx_nodes, (rx_nodes * sizeof(RX) + rx_nodes * 16 + rx_label_bytes) / 1048576.0, c);
    t0 = now(); for (int i = 0; i < Q; i++) sink += arr_complete(raw, n, pre[i], o4, 10); double d = (now() - t0) / Q * 1e9;
    printf("%-34s %12s %11.1f MB %16.0f\n", "sorted array + binary search", "-", (n * sizeof(char *) + chars + n) / 1048576.0, d);
    printf("\n(radix memory is an estimate: node struct + label bytes + about 16 B of allocator overhead for each of the two allocations per node)\n");
    if (sink == 42) puts("");
    t26_free(t26); ts_free(ts); rx_free(rx); for (size_t i = 0; i < n; i++) free(raw[i]); free(raw); free(o1); free(o2); free(o3); free(o4); free(pre);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined words.c -o words_asan && ./words_asan | head -3 && gcc -std=gnu11 -O2 -Wall -Wextra words.c -o words && ./words
74871 distinct words (0.6 MB of characters); first-10 completions of 5000 prefixes identical in all structures (5000 non-empty)

structure                                 nodes         memory ns / complete(10)
74871 distinct words (0.6 MB of characters); first-10 completions of 5000 prefixes identical in all structures (5000 non-empty)

structure                                 nodes         memory ns / complete(10)
26-way array trie                        153394        31.6 MB             2394
first-child/next-sibling trie            153394         3.5 MB             1332
compressed radix tree                     83281         4.0 MB             1602
sorted array + binary search                  -         1.2 MB              782

(radix memory is an estimate: node struct + label bytes + about 16 B of allocator overhead for each of the two allocations per node)
```

(The first run is under AddressSanitizer/UBSan to validate memory handling; the second is the timed build.) How to read the table:

- The **26-way array trie** needs a 208-byte node for every character position: tens of megabytes for a vocabulary that fits in about a megabyte. This is the standard textbook trie and the standard warning: it is the *least* memory-efficient option.
- The **first-child/next-sibling** version stores each node in 24 bytes and is about an order of magnitude smaller, at the cost of scanning siblings.
- **Path compression** roughly halves the node count here (the vocabulary was built from shared syllables, so the saving is moderate — for sparse keys like URLs or UUID strings it is far larger), though each node now also owns a small label allocation.
- The **sorted array with binary search** is the honest competitor, and in this experiment it is the smallest *and* about as fast or faster. A prefix is a contiguous range in sorted order, so `lower_bound` plus a linear scan gives completions with no pointer chasing. This is a recurring theme: **a trie wins when keys are updated dynamically, when you need O(k) independent of `n`, or when the key is a bit string with a fixed structure — not automatically for text lookup.** Production autocomplete systems mostly use sorted arrays, FSTs (finite-state transducers, which are tries with shared suffixes too) or compressed tries, never the 26-array trie.

## IP routing: longest-prefix match

IP forwarding needs, for a destination address, the route with the **longest matching prefix** (CIDR, RFC 4632): a table can hold both `10.0.0.0/8` and `10.1.2.0/24`, and the more specific one wins. See the *Forwarding: Routing Table, Longest-Prefix Match, Policy Routing and ECMP* note in the IPv4 module for how the kernel and routers use it. At line rate the lookup has a hard budget: a 100 Gb/s link carries up to ~150 million minimum-size packets per second, about 7 ns per packet per port. The full IPv4 Internet table is on the order of a million prefixes. This is the textbook case for a trie, with a bit string for a key.

Four implementations, all verified against each other:

1. **Linear scan** — the specification: check every route, keep the longest match. Used as the oracle in the test.
2. **Binary trie** — one bit per level, up to 32 *dependent* memory loads per lookup. Updates are trivial.
3. **Multibit trie, stride 8** — consume a byte per level, so at most 4 loads. A prefix whose length is not a multiple of 8 is **expanded**: `10.0.0.0/10` becomes 64 entries at the second level. Insert in ascending length so a longer prefix overwrites the expansion of a shorter one.
4. **DIR-24-8** (Gupta, Lin & McKeown 1998) — a flat table of 2²⁴ entries indexed directly by the top 24 bits of the address, plus 256-entry second-level groups for the rare prefixes longer than /24. One memory access for the lookups that match a /24 or shorter prefix (the large majority), a second for the rest. It is the structure behind DPDK's `rte_lpm`, which software routers and virtual switches use.

```c
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
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra lpm.c -o lpm && ./lpm
197914 routes (unique prefixes, incl. a default route); 20000 random destinations: binary trie, stride-8 trie and DIR-24-8 agree with linear scan

structure                     ns / lookup         memory
linear scan                        131475      1546.2 KB
binary trie (1 bit/level)             539        14.7 MB   (1282702 nodes)
multibit trie (stride 8)              106       162.2 MB   (73836 nodes)
DIR-24-8 (2^24 table)                  21        37.9 MB   (11997 tbl8 groups)
```

The synthetic table has about 200,000 unique prefixes with a length distribution resembling the real Internet (more than half are /24; a few are longer than /24; one default route). It is *not* real BGP data. The key results:

- **All three structures agree with the linear scan** on 20,000 destinations, 80 % of which fall inside a route and 20 % of which are uniformly random.
- The **linear scan costs about 130 µs per lookup** — it examines every prefix. This is also what a naive routing table costs, and why tables were tries from the start.
- The **binary trie is a few hundred nanoseconds**, bounded by up to 32 dependent loads.
- The **stride-8 trie is several times faster** (4 loads) but its 256-pointer nodes make it by far the **largest** structure — a 256× fan-out for nodes that usually have a handful of children.
- **DIR-24-8 is the fastest, around 20 ns per lookup, using a flat 32 MB table** (16-bit entries) plus the second-level groups. The cost is memory and update time: a /8 route rewrites 65,536 entries. That is acceptable for a table that changes a few times per second, and why such tables are rebuilt or patched in the control plane while the data plane reads them lock-free.

### What real implementations do

- **Linux IPv4 FIB** uses a **LC-trie** (level- and path-compressed trie, Nilsson & Karlsson) in `net/ipv4/fib_trie.c`: path compression plus *level compression* that replaces a dense subtree of height `h` with one node of `2^h` children, resizing dynamically. Lookups are protected by RCU, so forwarding does not take locks. (The routing cache that preceded it was removed in 3.6.)
- **DPDK `rte_lpm`** implements DIR-24-8 as above. For IPv6 the address is 128 bits, a direct table is impossible, and `rte_lpm6` uses a chain of 8-bit-stride tables.
- **Hardware routers** store compressed multibit tries (Tree Bitmap) or use TCAM, which matches all prefixes in parallel. **Poptrie** is a modern software design that use bitmaps and popcount to get both compactness and a few memory accesses.
- The **Linux `radix_tree` / XArray** (the kernel's page-cache index, an array indexed by file offset with 64-way nodes, 6 bits per level) and **ART** in main-memory databases follow the same idea for integer keys.

## Other prefix-structure uses

- **IP access lists and firewalls**: each rule is a prefix or a range; ranges convert into a handful of prefixes, and multi-field classification is built from several tries (cross-producting, HiCuts, bit-vector schemes — or, in Linux, nftables sets implemented as interval tree or `rbtree`/`pipapo` sets).
- **Aho–Corasick** (strings note): a trie with failure links, matching many patterns in one pass.
- **URL and HTTP routers**: a compressed trie over path segments, often with parameter nodes.
- **Spell-check, T9, autocomplete**: tries or FSTs over the dictionary.
- **XOR tries (Kademlia DHT)**: peers are organized by shared bit prefix of their IDs.
- **Bitwise tries for integer sets**: van Emde Boas trees and y-fast tries answer predecessor queries in `O(log log U)`.

## Pitfalls

- **Pointer fan-out waste.** Choose the node representation from the alphabet and the expected density; do not default to `T *child[256]`.
- **Memory is the real cost.** Count bytes per key, including allocator overhead — small mallocs round up and carry headers. A pool or arena allocator, and 32-bit indices instead of 64-bit pointers (as the binary trie above uses), often halve the footprint.
- **Insertion order for expanded prefixes.** In expansion schemes (multibit, DIR-24-8), insert short before long — or track prefix lengths per entry — or a short prefix will overwrite a longer one. Deletion needs per-entry prefix lengths (or a rebuild) to restore the next-longest match.
- **Unicode.** A trie over bytes of UTF-8 works and sorts correctly in code point order; a trie over "characters" needs a decision about normalization.
- **Concurrency.** Readers walking a trie while a writer splits an edge need RCU, atomic pointer publication, or copy-on-write; Morris-style in-place tricks are unsafe.
- **Worst-case depth.** The depth equals the key length; adversarial or very long keys (URLs, paths) need a compressed trie and length limits.

## Key takeaways

1. A trie's cost depends on the key length, not on the number of keys, and it makes prefix operations natural.
2. The child representation and path compression decide whether it is practical: the 26-way array trie is simple but memory-hungry; radix trees, bitmaps and adaptive nodes fix that.
3. A sorted array with binary search is a strong baseline for static string sets; tries win when keys change, for bit strings, and for prefix-keyed lookups such as routing.
4. Longest-prefix match is the canonical use: linear scan ~130 µs, binary trie hundreds of ns, a flat DIR-24-8 table ~20 ns — memory and update cost are the price.
5. Linux uses an LC-trie for the IPv4 FIB and radix-tree-style XArrays for the page cache; DPDK uses DIR-24-8.
6. Insert prefixes short-to-long in expansion schemes, and verify every fast structure against a slow linear oracle.
