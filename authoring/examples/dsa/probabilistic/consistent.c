#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "hash.h"

/* Hash ring: each node owns `vn` points; a key belongs to the first point clockwise from hash(key). */
typedef struct { uint64_t pos; int node; } Pt;
typedef struct { Pt *p; size_t n; } Ring;

static int cmp_pt(const void *a, const void *b) { uint64_t x = ((const Pt *)a)->pos, y = ((const Pt *)b)->pos; return x < y ? -1 : x > y; }
static Ring ring_build(int nodes, int vn) {
    Ring r = { malloc((size_t)nodes * (size_t)vn * sizeof(Pt)), 0 };
    for (int nd = 0; nd < nodes; nd++)
        for (int v = 0; v < vn; v++) r.p[r.n++] = (Pt){ hash64((uint64_t)nd * 1000003u + (uint64_t)v, 7), nd };
    qsort(r.p, r.n, sizeof(Pt), cmp_pt);
    return r;
}
static int ring_lookup(const Ring *r, uint64_t key) {      /* binary search: first point >= h, wrapping to 0 */
    uint64_t h = hash64(key, 3); size_t lo = 0, hi = r->n;
    while (lo < hi) { size_t m = (lo + hi) / 2; if (r->p[m].pos < h) lo = m + 1; else hi = m; }
    return r->p[lo == r->n ? 0 : lo].node;
}

int main(void) {
    enum { NODES = 10, KEYS = 1000000 };
    printf("Load balance: %d keys over %d nodes (ideal %d per node)\n", KEYS, NODES, KEYS / NODES);
    printf("%14s %12s %12s %12s\n", "points/node", "min load", "max load", "max/ideal");
    int vns[] = { 1, 10, 100, 1000 };
    for (int c = 0; c < 4; c++) {
        Ring r = ring_build(NODES, vns[c]); long load[NODES] = {0};
        for (uint64_t k = 0; k < KEYS; k++) load[ring_lookup(&r, k)]++;
        long mn = load[0], mx = load[0]; for (int i = 1; i < NODES; i++) { if (load[i] < mn) mn = load[i]; if (load[i] > mx) mx = load[i]; }
        printf("%14d %12ld %12ld %11.2fx\n", vns[c], mn, mx, (double)mx / (KEYS / NODES));
        free(r.p);
    }
    printf("\nAdding an 11th node: how many keys change owner?\n");
    long moved_mod = 0, moved_ring = 0;
    Ring a = ring_build(10, 200), b = ring_build(11, 200);
    for (uint64_t k = 0; k < KEYS; k++) {
        if (hash64(k, 3) % 10 != hash64(k, 3) % 11) moved_mod++;
        if (ring_lookup(&a, k) != ring_lookup(&b, k)) moved_ring++;
    }
    printf("  hash %% N      : %6.2f%% of keys move\n", 100.0 * (double)moved_mod / KEYS);
    printf("  hash ring     : %6.2f%% of keys move (ideal 1/11 = %.2f%%)\n", 100.0 * (double)moved_ring / KEYS, 100.0 / 11);
    free(a.p); free(b.p);
    return 0;
}
