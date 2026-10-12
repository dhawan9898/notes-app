#ifndef SORTS_H
#define SORTS_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Every comparison goes through LT so a counting build can measure comparisons exactly. */
extern unsigned long long g_cmps;
#ifdef COUNT_CMPS
#  define LT(a, b) (g_cmps++, (a) < (b))
#else
#  define LT(a, b) ((a) < (b))
#endif

typedef struct { int key; int seq; } Rec;       /* for stability tests: sort by key only, seq records the original order */

/* ---- insertion sort: O(n^2) worst, O(n) on nearly sorted input, stable, in place ---- */
static inline void insertion_sort(int *a, size_t n) {
    for (size_t i = 1; i < n; i++) {
        int key = a[i]; size_t j = i;
        while (j > 0 && LT(key, a[j - 1])) { a[j] = a[j - 1]; j--; }
        a[j] = key;
    }
}

/* ---- merge sort (top-down, one auxiliary buffer): Theta(n log n) always, stable ---- */
static void msort_rec(int *a, int *tmp, size_t n) {
    if (n <= 16) { insertion_sort(a, n); return; }              /* cutoff: small runs by insertion sort */
    size_t m = n / 2;
    msort_rec(a, tmp, m); msort_rec(a + m, tmp, n - m);
    if (!LT(a[m], a[m - 1])) return;                            /* already ordered: skip the merge (helps sorted input) */
    memcpy(tmp, a, m * sizeof *a);                              /* copy only the left half; merge back into a */
    size_t i = 0, j = m, k = 0;
    while (i < m && j < n) a[k++] = LT(a[j], tmp[i]) ? a[j++] : tmp[i++];     /* take left on ties: stability */
    while (i < m) a[k++] = tmp[i++];                            /* leftover right elements are already in place */
}
static inline int merge_sort(int *a, size_t n) {
    int *tmp = malloc((n / 2 + 1) * sizeof *tmp);
    if (!tmp) return -1;
    msort_rec(a, tmp, n);
    free(tmp);
    return 0;
}

/* ---- quicksort: median-of-three pivot, Hoare-style partition, recurse on the smaller side, loop on the larger ---- */
static inline void swap_int(int *x, int *y) { int t = *x; *x = *y; *y = t; }
static void quick_rec(int *a, size_t n) {
    while (n > 16) {
        size_t mid = n / 2;
        if (LT(a[mid], a[0])) swap_int(&a[mid], &a[0]);          /* order a[0], a[mid], a[n-1] */
        if (LT(a[n - 1], a[0])) swap_int(&a[n - 1], &a[0]);
        if (LT(a[n - 1], a[mid])) swap_int(&a[n - 1], &a[mid]);
        int pivot = a[mid];
        size_t i = 0, j = n - 1;
        for (;;) {                                               /* Hoare partition: both scans stop on equal keys, which balances duplicates */
            while (LT(a[i], pivot)) i++;
            while (LT(pivot, a[j])) j--;
            if (i >= j) break;
            swap_int(&a[i], &a[j]); i++; j--;
        }
        size_t left = j + 1;                                     /* [0, left) <= pivot <= [left, n) */
        if (left < n - left) { quick_rec(a, left); a += left; n -= left; }         /* recurse into the smaller part ... */
        else                 { quick_rec(a + left, n - left); n = left; }          /* ... iterate on the larger: stack depth O(log n) */
    }
    insertion_sort(a, n);
}
static inline void quick_sort(int *a, size_t n) { quick_rec(a, n); }

/* ---- 3-way (Dutch national flag) quicksort: O(n) when there are few distinct keys.
        The pivot is RANDOM: a fixed choice (first, last, middle) has an adversarial input that makes it O(n^2).
        (An earlier draft used the middle element and went quadratic on "organ pipe" input, where the middle is always the maximum.) ---- */
static uint64_t g_rng = 88172645463325252ULL;
static inline uint64_t rng_next(void) { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 7; g_rng ^= g_rng << 17; return g_rng; }
static void quick3_rec(int *a, long lo, long hi) {
    while (lo < hi) {
        int pivot = a[lo + (long)(rng_next() % (uint64_t)(hi - lo + 1))];
        long lt = lo, i = lo, gt = hi;                           /* invariant: a[lo..lt) < pivot, a[lt..i) == pivot, a(gt..hi] > pivot */
        while (i <= gt) {
            if (LT(a[i], pivot)) swap_int(&a[lt++], &a[i++]);
            else if (LT(pivot, a[i])) swap_int(&a[i], &a[gt--]);
            else i++;
        }
        if (lt - lo < hi - gt) { quick3_rec(a, lo, lt - 1); lo = gt + 1; }
        else                   { quick3_rec(a, gt + 1, hi); hi = lt - 1; }
    }
}
static inline void quick3_sort(int *a, size_t n) { if (n > 1) quick3_rec(a, 0, (long)n - 1); }

/* ---- heapsort: O(n log n) worst case, in place, not stable, poor cache behaviour ---- */
static void sift_down(int *a, size_t root, size_t n) {
    int v = a[root];
    for (;;) {
        size_t child = 2 * root + 1;
        if (child >= n) break;
        if (child + 1 < n && LT(a[child], a[child + 1])) child++;
        if (!LT(v, a[child])) break;
        a[root] = a[child]; root = child;
    }
    a[root] = v;
}
static inline void heap_sort(int *a, size_t n) {
    if (n < 2) return;
    for (size_t i = n / 2; i-- > 0; ) sift_down(a, i, n);        /* build a max-heap in O(n) */
    for (size_t end = n - 1; end > 0; end--) { swap_int(&a[0], &a[end]); sift_down(a, 0, end); }
}

/* ---- introsort: quicksort that falls back to heapsort when recursion gets too deep => O(n log n) worst case ---- */
static void intro_rec(int *a, size_t n, int depth) {
    while (n > 16) {
        if (depth-- == 0) { heap_sort(a, n); return; }
        size_t mid = n / 2;
        if (LT(a[mid], a[0])) swap_int(&a[mid], &a[0]);
        if (LT(a[n - 1], a[0])) swap_int(&a[n - 1], &a[0]);
        if (LT(a[n - 1], a[mid])) swap_int(&a[n - 1], &a[mid]);
        int pivot = a[mid]; size_t i = 0, j = n - 1;
        for (;;) { while (LT(a[i], pivot)) i++; while (LT(pivot, a[j])) j--; if (i >= j) break; swap_int(&a[i], &a[j]); i++; j--; }
        size_t left = j + 1;
        if (left < n - left) { intro_rec(a, left, depth); a += left; n -= left; } else { intro_rec(a + left, n - left, depth); n = left; }
    }
    insertion_sort(a, n);
}
static inline void intro_sort(int *a, size_t n) { int d = 0; for (size_t m = n; m; m >>= 1) d += 2; intro_rec(a, n, d); }
#endif
