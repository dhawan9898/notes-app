#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

/* 1. classic binary search with a branch */
static size_t bs_branchy(const int *a, size_t n, int key) {
    size_t lo = 0, hi = n;
    while (lo < hi) { size_t mid = lo + (hi - lo) / 2; if (a[mid] < key) lo = mid + 1; else hi = mid; }
    return lo;
}
/* 2. branchless: the compiler emits a conditional move instead of an unpredictable branch */
static size_t bs_branchless(const int *a, size_t n, int key) {
    const int *base = a; size_t len = n;
    while (len > 1) { size_t half = len / 2; base = (base[half - 1] < key) ? base + half : base; len -= half; }
    return (size_t)(base - a) + (len && *base < key);
}
/* 3. Eytzinger (BFS) layout: node k has children 2k and 2k+1, so the first levels of the implicit tree share cache lines */
static size_t eyt_build(const int *sorted, int *e, size_t n, size_t i, size_t k) {
    if (k <= n) { i = eyt_build(sorted, e, n, i, 2 * k); e[k] = sorted[i++]; i = eyt_build(sorted, e, n, i, 2 * k + 1); }
    return i;
}
static size_t eyt_lower_bound(const int *e, size_t n, int key) {           /* returns Eytzinger index of the lower bound, 0 if none */
    size_t k = 1;
    while (k <= n) { __builtin_prefetch(&e[k * 16]); k = 2 * k + (e[k] < key); }
    k >>= __builtin_ffsll((long long)~k);
    return k;
}

int main(void) {
    for (int log = 12; log <= 24; log += 6) {
        size_t n = (size_t)1 << log, q = 3000000;
        int *a = malloc(n * sizeof *a), *e = calloc(n + 1, sizeof *e), *keys = malloc(q * sizeof *keys);
        for (size_t i = 0; i < n; i++) a[i] = (int)(2 * i);
        eyt_build(a, e, n, 0, 1);
        uint64_t r = 88172645463325252ULL;
        for (size_t i = 0; i < q; i++) { r ^= r << 13; r ^= r >> 7; r ^= r << 17; keys[i] = (int)(r % (2 * n)); }
        size_t s1 = 0, s2 = 0, s3 = 0; double t0, t1;
        t0 = now(); for (size_t i = 0; i < q; i++) s1 += bs_branchy(a, n, keys[i]); t1 = now(); double d1 = (t1 - t0) / q * 1e9;
        t0 = now(); for (size_t i = 0; i < q; i++) s2 += bs_branchless(a, n, keys[i]); t1 = now(); double d2 = (t1 - t0) / q * 1e9;
        t0 = now(); for (size_t i = 0; i < q; i++) { size_t k = eyt_lower_bound(e, n, keys[i]); s3 += k ? (size_t)e[k] : (size_t)-1; } t1 = now(); double d3 = (t1 - t0) / q * 1e9;
        DO_NOT_OPTIMIZE(s1); DO_NOT_OPTIMIZE(s2); DO_NOT_OPTIMIZE(s3);
        /* sanity: lower-bound values agree */
        int ok = 1; for (size_t i = 0; i < 1000; i++) { size_t p = bs_branchy(a, n, keys[i]); size_t k = eyt_lower_bound(e, n, keys[i]); if ((p == n) != (k == 0) || (p < n && a[p] != e[k])) ok = 0; if (bs_branchless(a, n, keys[i]) != p) ok = 0; }
        printf("n = 2^%-2d (%6.1f MiB): branchy %6.1f ns/lookup | branchless %6.1f ns | Eytzinger+prefetch %6.1f ns   results agree: %s\n", log, n * 4 / 1048576.0, d1, d2, d3, ok ? "yes" : "NO");
        free(a); free(e); free(keys);
    }
    return 0;
}
