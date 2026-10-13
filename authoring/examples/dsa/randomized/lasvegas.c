// Las Vegas algorithms: always correct, random running time. Randomized quicksort and quickselect with comparison counts.
#include "rnd.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static long comps;
static void swap(int *a, int *b) { int t = *a; *a = *b; *b = t; }
// Lomuto partition around the LAST element: returns the pivot's final position. Distinct keys assumed (equal keys need a 3-way partition).
static int lomuto(int *a, int lo, int hi) {
    int p = a[hi], i = lo;
    for (int j = lo; j < hi; j++) { comps++; if (a[j] < p) swap(&a[i++], &a[j]); }
    swap(&a[i], &a[hi]);
    return i;
}
static void qs_last(int *a, int lo, int hi) {              // deterministic: pivot = last element, quadratic on sorted input
    while (lo < hi) {
        int m = lomuto(a, lo, hi);
        if (m - lo < hi - m) { qs_last(a, lo, m - 1); lo = m + 1; } else { qs_last(a, m + 1, hi); hi = m - 1; }    // recurse on the smaller side
    }
}
static void qs_random(int *a, int lo, int hi) {            // same algorithm, uniformly random pivot
    while (lo < hi) {
        swap(&a[hi], &a[lo + rnd_below(hi - lo + 1)]);
        int m = lomuto(a, lo, hi);
        if (m - lo < hi - m) { qs_random(a, lo, m - 1); lo = m + 1; } else { qs_random(a, m + 1, hi); hi = m - 1; }
    }
}
// quickselect for the k-th smallest (0-based), random pivot: recurse into one side only
static int quickselect(int *a, int n, int k) {
    int lo = 0, hi = n - 1;
    while (lo < hi) {
        swap(&a[hi], &a[lo + rnd_below(hi - lo + 1)]);
        int m = lomuto(a, lo, hi);
        if (k == m) return a[m]; else if (k < m) hi = m - 1; else lo = m + 1;
    }
    return a[lo];
}

int main(void) {
    int n = 20000; int *a = malloc(n * sizeof *a);
    printf("sorting n = %d integers; comparisons made (expected for random pivots: about 2 n ln n = %.0f)\n", n, 2.0 * n * log(n));
    printf("%-34s %14s\n", "input / pivot rule", "comparisons");
    for (int i = 0; i < n; i++) a[i] = i;
    comps = 0; qs_last(a, 0, n - 1);   printf("%-34s %14ld\n", "sorted input, last-element pivot", comps);
    for (int i = 0; i < n; i++) a[i] = i;
    comps = 0; qs_random(a, 0, n - 1); printf("%-34s %14ld\n", "sorted input, random pivot", comps);
    for (int i = 0; i < n; i++) a[i] = n - i;
    comps = 0; qs_random(a, 0, n - 1); printf("%-34s %14ld\n", "reversed input, random pivot", comps);
    long total = 0, worst = 0, best = 1L << 60;
    for (int run = 0; run < 200; run++) { for (int i = 0; i < n; i++) a[i] = i; comps = 0; qs_random(a, 0, n - 1); total += comps; if (comps > worst) worst = comps; if (comps < best) best = comps; }
    printf("200 runs on sorted input, random pivot: mean %ld, min %ld, max %ld  (the running time is random, the answer is always sorted)\n", total / 200, best, worst);
    long tot = 0; int ok = 1;
    for (int run = 0; run < 200; run++) { for (int i = 0; i < n; i++) a[i] = (int)rnd_below(1000000); comps = 0; int k = (int)rnd_below(n);
        int *b = malloc(n * sizeof *b); for (int i = 0; i < n; i++) b[i] = a[i]; qs_random(b, 0, n - 1); long c1 = comps; comps = 0;
        int got = quickselect(a, n, k); if (got != b[k]) ok = 0; tot += comps; (void)c1; free(b); }
    printf("quickselect: 200 random ranks all correct: %s; average %.2f comparisons per element (expected at most 3.4 n for a random rank)\n", ok ? "yes" : "NO", (double)tot / 200 / n);
    free(a); return 0;
}
