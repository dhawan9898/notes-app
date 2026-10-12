#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

static uint64_t rng = 88172645463325252ULL;
static uint64_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }
static void swap_i(int *a, int *b) { int t = *a; *a = *b; *b = t; }
static int cmp_int(const void *x, const void *y) { int a = *(const int *)x, b = *(const int *)y; return (a > b) - (a < b); }

/* 3-way partition around a pivot value; returns the [lt, gt] range equal to the pivot. */
static void partition3(int *a, int lo, int hi, int pivot, int *plt, int *pgt) {
    int lt = lo, i = lo, gt = hi;
    while (i <= gt) { if (a[i] < pivot) swap_i(&a[lt++], &a[i++]); else if (a[i] > pivot) swap_i(&a[i], &a[gt--]); else i++; }
    *plt = lt; *pgt = gt;
}

/* Quickselect with a RANDOM pivot: expected O(n), worst case O(n^2) (astronomically unlikely). Returns the k-th smallest (0-based). */
static int quickselect(int *a, int n, int k) {
    int lo = 0, hi = n - 1;
    while (lo < hi) {
        int pivot = a[lo + (int)(rnd() % (uint64_t)(hi - lo + 1))], lt, gt;
        partition3(a, lo, hi, pivot, &lt, &gt);
        if (k < lt) hi = lt - 1; else if (k > gt) lo = gt + 1; else return pivot;
    }
    return a[lo];
}

/* Median of medians (BFPRT): deterministic worst-case O(n).  Pivot = median of the medians of groups of five. */
static int select_mom(int *a, int lo, int hi, int k);
static int median_of_five(int *a, int lo, int hi) {                 /* sort <= 5 elements by insertion; return the index of the median */
    for (int i = lo + 1; i <= hi; i++) { int key = a[i], j = i - 1; while (j >= lo && a[j] > key) { a[j + 1] = a[j]; j--; } a[j + 1] = key; }
    return lo + (hi - lo) / 2;
}
static int select_mom(int *a, int lo, int hi, int k) {              /* k is an absolute index into a[] */
    while (lo < hi) {
        int n = hi - lo + 1, m = 0;
        for (int i = lo; i <= hi; i += 5) {                          /* medians of groups of five, gathered at the front */
            int end = i + 4 < hi ? i + 4 : hi;
            swap_i(&a[lo + m++], &a[median_of_five(a, i, end)]);
        }
        int pivot = n <= 5 ? a[lo + (m - 1) / 2] : select_mom(a, lo, lo + m - 1, lo + (m - 1) / 2);   /* median of the medians (recursive) */
        int lt, gt;
        partition3(a, lo, hi, pivot, &lt, &gt);
        if (k < lt) hi = lt - 1; else if (k > gt) lo = gt + 1; else return pivot;
    }
    return a[lo];
}

/* Top-k smallest with a bounded max-heap: O(n log k) time, O(k) space, works on a stream. */
static void heap_sift_down(int *h, int n, int i) { for (;;) { int c = 2 * i + 1; if (c >= n) break; if (c + 1 < n && h[c + 1] > h[c]) c++; if (h[i] >= h[c]) break; swap_i(&h[i], &h[c]); i = c; } }
static void topk_smallest(const int *a, int n, int k, int *out) {
    int size = 0;
    for (int i = 0; i < n; i++) {
        if (size < k) { out[size++] = a[i]; if (size == k) for (int j = k / 2 - 1; j >= 0; j--) heap_sift_down(out, k, j); }
        else if (a[i] < out[0]) { out[0] = a[i]; heap_sift_down(out, k, 0); }          /* replace the current k-th smallest */
    }
}

int main(void) {
    /* correctness */
    for (int t = 0; t < 500; t++) {
        int n = 1 + (int)(rnd() % 300), a[300], b[300], c[300];
        for (int i = 0; i < n; i++) a[i] = (int)(rnd() % 50);
        memcpy(b, a, n * sizeof *a); memcpy(c, a, n * sizeof *a);
        int k = (int)(rnd() % (uint64_t)n);
        int ref[300]; memcpy(ref, a, n * sizeof *a); qsort(ref, n, sizeof *ref, cmp_int);
        assert(quickselect(b, n, k) == ref[k]);
        assert(select_mom(c, 0, n - 1, k) == ref[k]);
        int kk = 1 + (int)(rnd() % (uint64_t)n), out[300];
        topk_smallest(a, n, kk, out); qsort(out, kk, sizeof *out, cmp_int);
        assert(!memcmp(out, ref, kk * sizeof *out));
    }
    printf("quickselect, median-of-medians and heap top-k agree with sorting on 500 random arrays\n\n");

    size_t n = 5000000; int *base = malloc(n * sizeof *base), *a = malloc(n * sizeof *a);
    for (size_t i = 0; i < n; i++) base[i] = (int)(rnd() >> 33);
    int k = (int)(n / 2);
    memcpy(a, base, n * sizeof *a); double t0 = now(); qsort(a, n, sizeof *a, cmp_int); int m1 = a[k]; double t1 = now();
    memcpy(a, base, n * sizeof *a); double t2 = now(); int m2 = quickselect(a, (int)n, k); double t3 = now();
    memcpy(a, base, n * sizeof *a); double t4 = now(); int m3 = select_mom(a, 0, (int)n - 1, k); double t5 = now();
    printf("median of %zu ints:  sort+index %7.1f ms | quickselect %6.1f ms | median-of-medians %6.1f ms   (all equal: %s)\n", n, (t1 - t0) * 1e3, (t3 - t2) * 1e3, (t5 - t4) * 1e3, m1 == m2 && m2 == m3 ? "yes" : "NO");
    int top[100];
    double t6 = now(); topk_smallest(base, (int)n, 100, top); double t7 = now();
    printf("100 smallest of %zu: heap top-k %6.1f ms (one pass, O(k) memory) vs full sort %7.1f ms\n", n, (t7 - t6) * 1e3, (t1 - t0) * 1e3);
    free(base); free(a);
    return 0;
}
