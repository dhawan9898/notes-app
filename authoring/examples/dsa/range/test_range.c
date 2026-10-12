#include <assert.h>
#include <stdio.h>
#include "structs.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    /* Randomized differential tests of every structure against a naive array, on sizes that are not powers of two */
    for (size_t n = 1; n <= 300; n += (n < 40 ? 1 : 37)) {
        int *a = malloc(n * sizeof *a), *ref = malloc(n * sizeof *ref);
        for (size_t i = 0; i < n; i++) ref[i] = a[i] = (int)(rnd() % 2001) - 1000;
        ll *p = prefix_build(a, n); Fenwick f = fw_build(a, n); Seg g = seg_new(a, n); Sparse sp = sp_build(a, n); Sqrt q = sq_new(a, n);
        Fenwick f2 = fw_new(n); for (size_t i = 0; i < n; i++) fw_add(&f2, i, a[i]);        /* built by n updates must equal the O(n) build */
        for (size_t i = 0; i < n; i++) assert(f.t[i + 1] == f2.t[i + 1]);
        for (int op = 0; op < 3000; op++) {
            size_t l = rnd() % n, r = l + 1 + rnd() % (n - l); ll want = 0; int mn = ref[l];
            for (size_t i = l; i < r; i++) { want += ref[i]; if (ref[i] < mn) mn = ref[i]; }
            int kind = (int)(rnd() % 4);
            if (kind == 0) {                                                      /* point update on the dynamic structures */
                size_t i = rnd() % n; int d = (int)(rnd() % 201) - 100; ref[i] += d; fw_add(&f, i, d); sq_add(&q, i, d); seg_add(&g, i, i + 1, d);
                free(p); p = prefix_build(ref, n); sp_free(&sp); sp = sp_build(ref, n);
            } else if (kind == 1) {                                               /* range add (lazy segment tree only) */
                int d = (int)(rnd() % 21) - 10; for (size_t i = l; i < r; i++) ref[i] += d;
                seg_add(&g, l, r, d); for (size_t i = l; i < r; i++) { fw_add(&f, i, d); sq_add(&q, i, d); }
                free(p); p = prefix_build(ref, n); sp_free(&sp); sp = sp_build(ref, n);
            } else {
                assert(prefix_sum(p, l, r) == want); assert(fw_range(&f, l, r) == want); assert(seg_sum(&g, l, r) == want);
                assert(sq_sum(&q, l, r) == want); assert(sp_min(&sp, l, r) == mn);
            }
        }
        free(a); free(ref); free(p); free(f.t); free(f2.t); free(g.sum); free(g.lazy); sp_free(&sp); free(q.a); free(q.blk);
    }
    printf("prefix sums, Fenwick, lazy segment tree, sparse table and sqrt decomposition agree with a naive scan on random sizes, updates and queries\n");
    return 0;
}
