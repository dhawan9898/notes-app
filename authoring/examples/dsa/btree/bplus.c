#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

/* In-memory B+ tree: internal nodes route, leaves hold all keys/values and are chained for range scans.
 * `M` (max keys per node) is a runtime parameter so we can compare node sizes; arrays are sized for the largest M. */
#define MAXM 512
typedef struct Node {
    int leaf, n;                                   /* n = number of keys */
    struct Node *next;                             /* leaf chain */
    uint32_t k[];                                  /* keys[kcap] followed by slots[kcap] (uint64: a child pointer or a value) */
} Node;
typedef struct { Node *root; int M, kcap; size_t nodes, leaves; int height; } BPT;

static inline uint64_t *slots(const BPT *t, const Node *x) { return (uint64_t *)(x->k + t->kcap); }
static Node *new_node(BPT *t, int leaf) {                      /* sized for this tree's M: header + (M+2) keys + (M+2) slots */
    Node *n = calloc(1, sizeof *n + (size_t)t->kcap * (sizeof(uint32_t) + sizeof(uint64_t))); n->leaf = leaf; t->nodes++; t->leaves += leaf; return n;
}
static void bpt_init(BPT *t, int M) { t->M = M; t->kcap = (M + 3) & ~1; t->nodes = t->leaves = 0; t->root = new_node(t, 1); t->height = 1; }

static int lower(const uint32_t *k, int n, uint32_t key) { int lo = 0, hi = n; while (lo < hi) { int m = (lo + hi) / 2; if (k[m] < key) lo = m + 1; else hi = m; } return lo; }
static int upper(const uint32_t *k, int n, uint32_t key) { int lo = 0, hi = n; while (lo < hi) { int m = (lo + hi) / 2; if (k[m] <= key) lo = m + 1; else hi = m; } return lo; }

static const Node *find_leaf(const BPT *t, uint32_t key) {
    const Node *x = t->root;
    while (!x->leaf) x = (const Node *)slots(t, x)[upper(x->k, x->n, key)];       /* separator k[i] is the smallest key of child i+1 */
    return x;
}
static int bpt_get(const BPT *t, uint32_t key, uint32_t *val) {
    const Node *l = find_leaf(t, key); int i = lower(l->k, l->n, key);
    if (i < l->n && l->k[i] == key) { *val = (uint32_t)slots(t, l)[i]; return 1; }
    return 0;
}
/* insert into subtree; if the node splits, returns the new right sibling and its separator key */
static Node *insert_rec(BPT *t, Node *x, uint32_t key, uint32_t val, uint32_t *sep, int *added) {
    int M = t->M; uint64_t *sl = slots(t, x);
    if (x->leaf) {
        int i = lower(x->k, x->n, key);
        if (i < x->n && x->k[i] == key) { sl[i] = val; return NULL; }
        memmove(x->k + i + 1, x->k + i, (size_t)(x->n - i) * sizeof(uint32_t));
        memmove(sl + i + 1, sl + i, (size_t)(x->n - i) * sizeof(uint64_t));
        x->k[i] = key; sl[i] = val; x->n++; *added = 1;
        if (x->n <= M) return NULL;
        Node *r = new_node(t, 1); int h = x->n / 2;                    /* split leaf: right gets the upper half; separator is COPIED up */
        r->n = x->n - h; memcpy(r->k, x->k + h, (size_t)r->n * sizeof(uint32_t)); memcpy(slots(t, r), sl + h, (size_t)r->n * sizeof(uint64_t));
        x->n = h; r->next = x->next; x->next = r; *sep = r->k[0]; return r;
    }
    int i = upper(x->k, x->n, key);
    uint32_t csep; Node *r = insert_rec(t, (Node *)sl[i], key, val, &csep, added);
    if (!r) return NULL;
    memmove(x->k + i + 1, x->k + i, (size_t)(x->n - i) * sizeof(uint32_t));
    memmove(sl + i + 2, sl + i + 1, (size_t)(x->n - i) * sizeof(uint64_t));
    x->k[i] = csep; sl[i + 1] = (uint64_t)(uintptr_t)r; x->n++;
    if (x->n <= M) return NULL;
    Node *s = new_node(t, 0); int h = x->n / 2;                        /* split internal: middle key MOVES up */
    *sep = x->k[h]; s->n = x->n - h - 1;
    memcpy(s->k, x->k + h + 1, (size_t)s->n * sizeof(uint32_t)); memcpy(slots(t, s), sl + h + 1, (size_t)(s->n + 1) * sizeof(uint64_t));
    x->n = h; return s;
}
static void bpt_put(BPT *t, uint32_t key, uint32_t val, size_t *count) {
    uint32_t sep; int added = 0; Node *r = insert_rec(t, t->root, key, val, &sep, &added);
    if (added) (*count)++;
    if (r) { Node *nr = new_node(t, 0); nr->n = 1; nr->k[0] = sep; slots(t, nr)[0] = (uint64_t)(uintptr_t)t->root; slots(t, nr)[1] = (uint64_t)(uintptr_t)r; t->root = nr; t->height++; }
}
static void bpt_free_rec(const BPT *t, Node *x) { if (!x->leaf) for (int i = 0; i <= x->n; i++) bpt_free_rec(t, (Node *)slots(t, x)[i]); free(x); }
static void bpt_free(BPT *t) { bpt_free_rec(t, t->root); }

/* structural check: sorted keys, separators bound the children, equal depth */
static int check(const BPT *t, const Node *x, uint32_t lo, uint32_t hi, int depth, int *leaf_depth, size_t *keys) {
    for (int i = 1; i < x->n; i++) if (x->k[i - 1] >= x->k[i]) return 0;
    for (int i = 0; i < x->n; i++) if (x->k[i] < lo || x->k[i] >= hi) return 0;
    if (x->leaf) { if (*leaf_depth < 0) *leaf_depth = depth; *keys += (size_t)x->n; return *leaf_depth == depth; }
    for (int i = 0; i <= x->n; i++)
        if (!check(t, (const Node *)slots(t, x)[i], i ? x->k[i - 1] : lo, i < x->n ? x->k[i] : hi, depth + 1, leaf_depth, keys)) return 0;
    return 1;
}

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }
static int cmp_u32(const void *a, const void *b) { return (*(const uint32_t *)a > *(const uint32_t *)b) - (*(const uint32_t *)a < *(const uint32_t *)b); }

int main(void) {
    /* correctness: 300k random puts (with overwrites) vs a hash-free reference: sorted array rebuilt at the end */
    { BPT t; bpt_init(&t, 8); size_t cnt = 0; enum { N = 300000, RANGE = 100000 }; static uint32_t ref[RANGE]; static char has[RANGE];
      for (int i = 0; i < N; i++) { uint32_t k = rnd() % RANGE, v = rnd(); bpt_put(&t, k, v, &cnt); ref[k] = v; has[k] = 1; }
      size_t distinct = 0; for (int i = 0; i < RANGE; i++) distinct += has[i];
      int ld = -1; size_t keys = 0; assert(check(&t, t.root, 0, UINT32_MAX, 1, &ld, &keys)); assert(keys == distinct && cnt == distinct && ld == t.height);
      for (int k = 0; k < RANGE; k++) { uint32_t v; assert(bpt_get(&t, (uint32_t)k, &v) == has[k]); if (has[k]) assert(v == ref[k]); }
      size_t scanned = 0; uint32_t prev = 0; int first = 1;
      for (const Node *l = find_leaf(&t, 0); l; l = l->next) for (int i = 0; i < l->n; i++) { assert(first || l->k[i] > prev); prev = l->k[i]; first = 0; scanned++; }
      assert(scanned == distinct);
      printf("M = 8 B+ tree: %d puts, %zu distinct keys, structure/lookup/leaf-chain checks passed (height %d)\n\n", N, distinct, t.height);
      bpt_free(&t); }

    /* shape and speed vs node size, n = 2,000,000 random distinct-ish keys */
    size_t n = 2000000; uint32_t *keys = malloc(n * sizeof *keys), *probe = malloc(n * sizeof *probe);
    for (size_t i = 0; i < n; i++) keys[i] = rnd();
    for (size_t i = 0; i < n; i++) probe[i] = keys[rnd() % n];
    printf("%-12s %6s %8s %12s %12s %10s %10s %10s\n", "structure", "M", "height", "nodes", "leaf fill", "insert ns", "lookup ns", "scan ns/key");
    int Ms[] = { 4, 16, 64, 256, 510 };
    for (int mi = 0; mi < 5; mi++) {
        BPT t; bpt_init(&t, Ms[mi]); size_t cnt = 0; double t0 = now();
        for (size_t i = 0; i < n; i++) bpt_put(&t, keys[i], (uint32_t)i, &cnt);
        double ti = (now() - t0) / (double)n * 1e9; uint32_t v; long sink = 0;
        t0 = now(); for (size_t i = 0; i < n; i++) sink += bpt_get(&t, probe[i], &v) ? v : 0; double tl = (now() - t0) / (double)n * 1e9;
        t0 = now(); size_t sc = 0; for (const Node *l = find_leaf(&t, 0); l; l = l->next) for (int i = 0; i < l->n; i++) sink += (long)slots(&t, l)[i], sc++; double tsc = (now() - t0) / (double)sc * 1e9;
        printf("%-12s %6d %8d %12zu %11.0f%% %10.0f %10.0f %10.1f\n", "B+ tree", Ms[mi], t.height, t.nodes, 100.0 * (double)cnt / ((double)t.leaves * Ms[mi]), ti, tl, tsc);
        if (sink == 42) puts("");
        bpt_free(&t);
    }
    qsort(keys, n, sizeof *keys, cmp_u32);
    long sink = 0; double t0 = now();
    for (size_t i = 0; i < n; i++) { size_t lo = 0, hi = n; while (lo < hi) { size_t m = (lo + hi) / 2; if (keys[m] < probe[i]) lo = m + 1; else hi = m; } sink += keys[lo % n]; }
    printf("%-12s %6s %8s %12s %12s %10s %10.0f %10s\n", "sorted array", "-", "-", "-", "100%", "(static)", (now() - t0) / (double)n * 1e9, "-");
    if (sink == 42) puts("");
    free(keys); free(probe);
    return 0;
}
