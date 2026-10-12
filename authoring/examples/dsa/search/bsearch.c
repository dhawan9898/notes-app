#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* lower_bound: first index i in [0, n] with a[i] >= key.  Invariant: all a[0..lo) < key, all a[hi..n) >= key. */
static size_t lower_bound(const int *a, size_t n, int key) {
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;                    /* never overflows */
        if (a[mid] < key) lo = mid + 1; else hi = mid;
    }
    return lo;
}
/* upper_bound: first index with a[i] > key.  [lower, upper) is the range equal to key. */
static size_t upper_bound(const int *a, size_t n, int key) {
    size_t lo = 0, hi = n;
    while (lo < hi) { size_t mid = lo + (hi - lo) / 2; if (a[mid] <= key) lo = mid + 1; else hi = mid; }
    return lo;
}

/* Search in a sorted array that was rotated at an unknown pivot (no duplicates): one of the two halves is always sorted. */
static long search_rotated(const int *a, long n, int key) {
    long lo = 0, hi = n - 1;
    while (lo <= hi) {
        long mid = lo + (hi - lo) / 2;
        if (a[mid] == key) return mid;
        if (a[lo] <= a[mid]) { if (a[lo] <= key && key < a[mid]) hi = mid - 1; else lo = mid + 1; }      /* left half sorted */
        else                 { if (a[mid] < key && key <= a[hi]) lo = mid + 1; else hi = mid - 1; }      /* right half sorted */
    }
    return -1;
}

/* Search on the ANSWER: smallest ship capacity that moves all packages in at most `days` days (order preserved).
   feasible(cap) is monotone: if cap works, any larger cap works.  Binary search the smallest feasible value. */
static int days_needed(const int *w, int n, long cap) { int days = 1; long load = 0; for (int i = 0; i < n; i++) { if (load + w[i] > cap) { days++; load = 0; } load += w[i]; } return days; }
static long min_capacity(const int *w, int n, int days) {
    long lo = 0, hi = 0;
    for (int i = 0; i < n; i++) { if (w[i] > lo) lo = w[i]; hi += w[i]; }       /* answer lies in [max weight, total weight] */
    while (lo < hi) { long mid = lo + (hi - lo) / 2; if (days_needed(w, n, mid) <= days) hi = mid; else lo = mid + 1; }
    return lo;
}

/* Bisection on a real function: sqrt(x) without sqrt().  Stop by iteration count, not by "lo == hi" (floating point may never get there). */
static double bisect_sqrt(double x, int *iters) {
    double lo = 0, hi = x < 1 ? 1 : x;
    int i;
    for (i = 0; i < 200 && hi - lo > 1e-12 * hi; i++) { double mid = lo + (hi - lo) / 2; if (mid * mid < x) lo = mid; else hi = mid; }
    *iters = i;
    return lo + (hi - lo) / 2;
}

static int cmp_int(const void *x, const void *y) { int a = *(const int *)x, b = *(const int *)y; return (a > b) - (a < b); }

int main(void) {
    srand(2024);
    long checks = 0;
    for (int t = 0; t < 5000; t++) {                           /* fuzz lower/upper bound against a linear scan */
        size_t n = (size_t)rand() % 30; int a[30];
        for (size_t i = 0; i < n; i++) a[i] = rand() % 12;
        qsort(a, n, sizeof *a, cmp_int);
        int key = rand() % 14 - 1;
        size_t lb = 0, ub = 0;
        while (lb < n && a[lb] < key) lb++;
        while (ub < n && a[ub] <= key) ub++;
        assert(lower_bound(a, n, key) == lb && upper_bound(a, n, key) == ub); checks++;
    }
    printf("lower_bound/upper_bound: %ld random cases match a linear scan (including empty arrays and absent keys)\n", checks);

    int dups[] = {1, 2, 2, 2, 2, 3, 5};
    size_t lb = lower_bound(dups, 7, 2), ub = upper_bound(dups, 7, 2);
    int key = 2; int *found = bsearch(&key, dups, 7, sizeof *dups, cmp_int);
    printf("array {1,2,2,2,2,3,5}: key 2 occupies [%zu, %zu) -> count %zu; insertion point of 4 = %zu; libc bsearch returned index %ld (any match, unspecified which)\n", lb, ub, ub - lb, lower_bound(dups, 7, 4), (long)(found - dups));

    int rot[] = {15, 18, 19, 22, 1, 3, 6, 9, 12};
    printf("rotated search: 6 at index %ld, 1 at index %ld, 7 at index %ld (absent)\n", search_rotated(rot, 9, 6), search_rotated(rot, 9, 1), search_rotated(rot, 9, 7));

    int w[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    printf("ship packages {1..10} in 5 days: minimum capacity = %ld; in 1 day: %ld; in 10 days: %ld\n", min_capacity(w, 10, 5), min_capacity(w, 10, 1), min_capacity(w, 10, 10));

    int it; double r = bisect_sqrt(2.0, &it);
    printf("bisect_sqrt(2) = %.12f after %d halvings (libm: %.12f)\n", r, it, sqrt(2.0));

    /* the classic overflow, shown with 32-bit indices near INT_MAX */
    int lo = 1500000000, hi = 2000000000;
    long long exact = ((long long)lo + hi) / 2;
    int safe = lo + (hi - lo) / 2;
    int wrapped = (int)((unsigned)lo + (unsigned)hi);        /* what (lo+hi) holds after wraparound (the signed version is UB) */
    printf("lo=%d hi=%d: correct mid=%lld, lo+(hi-lo)/2=%d, (lo+hi)/2 would be %d/2 = %d\n", lo, hi, exact, safe, wrapped, wrapped / 2);
    return 0;
}
