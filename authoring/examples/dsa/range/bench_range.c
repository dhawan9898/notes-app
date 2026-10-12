#define _GNU_SOURCE
#include <stdio.h>
#include "structs.h"
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    printf("dynamic range sum: 50%% point updates, 50%% random range sums; ns per operation\n");
    printf("%9s | %10s %10s %10s %10s | %12s\n", "n", "naive", "Fenwick", "segtree", "sqrt dec.", "memory (F/S)");
    for (size_t n = 10000; n <= 4000000; n *= 20) {
        int *a = malloc(n * sizeof *a); for (size_t i = 0; i < n; i++) a[i] = (int)(rnd() % 1000);
        enum { Q = 400000 }; size_t *L = malloc(Q * sizeof *L), *R = malloc(Q * sizeof *R); for (int i = 0; i < Q; i++) { L[i] = rnd() % n; R[i] = L[i] + 1 + rnd() % (n - L[i]); }
        long sink = 0; double t0;
        int naive_q = (int)(2000000000ull / n / 4); if (naive_q > Q) naive_q = Q; if (naive_q < 200) naive_q = 200;
        int *c = malloc(n * sizeof *c); memcpy(c, a, n * sizeof *c);
        t0 = now(); for (int i = 0; i < naive_q; i++) { if (i & 1) c[L[i]] += 1; else { ll t = 0; for (size_t j = L[i]; j < R[i]; j++) t += c[j]; sink += t; } } double tn = (now() - t0) / naive_q * 1e9;
        Fenwick f = fw_build(a, n); t0 = now(); for (int i = 0; i < Q; i++) { if (i & 1) fw_add(&f, L[i], 1); else sink += fw_range(&f, L[i], R[i]); } double tf = (now() - t0) / Q * 1e9;
        Seg g = seg_new(a, n); t0 = now(); for (int i = 0; i < Q; i++) { if (i & 1) seg_add(&g, L[i], L[i] + 1, 1); else sink += seg_sum(&g, L[i], R[i]); } double ts = (now() - t0) / Q * 1e9;
        Sqrt q = sq_new(a, n); t0 = now(); for (int i = 0; i < Q; i++) { if (i & 1) sq_add(&q, L[i], 1); else sink += sq_sum(&q, L[i], R[i]); } double tq = (now() - t0) / Q * 1e9;
        printf("%9zu | %10.0f %10.0f %10.0f %10.0f | %5.1f/%4.1f MB\n", n, tn, tf, ts, tq, (double)(n + 1) * 8 / 1e6, (double)n * 64 / 1e6);
        if (sink == 42) puts("");
        free(a); free(c); free(L); free(R); free(f.t); free(g.sum); free(g.lazy); free(q.a); free(q.blk);
    }

    printf("\nstatic range minimum: n = 2,000,000, 4,000,000 random queries\n");
    size_t n = 2000000; int *a = malloc(n * sizeof *a); for (size_t i = 0; i < n; i++) a[i] = (int)rnd();
    enum { Q = 4000000 }; long sink = 0; double t0;
    t0 = now(); Sparse sp = sp_build(a, n); double build = now() - t0;
    size_t *L = malloc(Q * sizeof *L), *R = malloc(Q * sizeof *R); for (int i = 0; i < Q; i++) { L[i] = rnd() % n; R[i] = L[i] + 1 + rnd() % (n - L[i]); }
    t0 = now(); for (int i = 0; i < Q; i++) sink += sp_min(&sp, L[i], R[i]); double tq = (now() - t0) / Q * 1e9;
    int nq = 2000; t0 = now(); for (int i = 0; i < nq; i++) { int m = a[L[i]]; for (size_t j = L[i]; j < R[i]; j++) if (a[j] < m) m = a[j]; sink += m; } double tn = (now() - t0) / nq * 1e9;
    printf("sparse table: build %.0f ms, %d levels, %.0f MB; query %.1f ns.   naive scan: %.0f ns per query (%.0fx slower)\n",
           build * 1e3, sp.levels, (double)n * (double)sp.levels * 4 / 1e6, tq, tn, tn / tq);
    if (sink == 42) puts("");
    return 0;
}
