#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long ops;                                /* global operation counter: deterministic, unlike timing */

/* O(n): linear search, worst case (key absent) */
static long linear_search(const int *a, int n, int key) { for (int i = 0; i < n; i++) { ops++; if (a[i] == key) return i; } return -1; }

/* O(log n): binary search */
static long binary_search(const int *a, int n, int key) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) { ops++; int mid = lo + (hi - lo) / 2; if (a[mid] == key) return mid; if (a[mid] < key) lo = mid + 1; else hi = mid - 1; }
    return -1;
}

/* O(n^2): bubble sort comparisons (worst case: reversed input) */
static void bubble_sort(int *a, int n) {
    for (int i = 0; i < n - 1; i++) for (int j = 0; j < n - 1 - i; j++) { ops++; if (a[j] > a[j + 1]) { int t = a[j]; a[j] = a[j + 1]; a[j + 1] = t; } }
}

/* O(n log n): merge sort comparisons */
static void merge_sort(int *a, int *tmp, int n) {
    if (n < 2) return;
    int m = n / 2;
    merge_sort(a, tmp, m); merge_sort(a + m, tmp, n - m);
    int i = 0, j = m, k = 0;
    while (i < m && j < n) { ops++; tmp[k++] = a[i] <= a[j] ? a[i++] : a[j++]; }
    while (i < m) tmp[k++] = a[i++];
    while (j < n) tmp[k++] = a[j++];
    memcpy(a, tmp, (size_t)n * sizeof *a);
}

/* O(2^n) vs O(n): Fibonacci */
static unsigned long fib_calls;
static uint64_t fib_naive(int n) { fib_calls++; return n < 2 ? (uint64_t)n : fib_naive(n - 1) + fib_naive(n - 2); }
static uint64_t fib_memo(int n, uint64_t *memo) { fib_calls++; if (n < 2) return (uint64_t)n; if (memo[n]) return memo[n]; return memo[n] = fib_memo(n - 1, memo) + fib_memo(n - 2, memo); }

int main(void) {
    printf("%8s %10s %10s %12s %12s %14s\n", "n", "linear", "binary", "bubble", "mergesort", "n*log2(n)");
    for (int n = 1000; n <= 16000; n *= 2) {
        int *a = malloc((size_t)n * sizeof *a), *b = malloc((size_t)n * sizeof *b), *tmp = malloc((size_t)n * sizeof *tmp);
        for (int i = 0; i < n; i++) { a[i] = 2 * i; b[i] = n - i; }               /* a sorted (even numbers), b reversed */
        ops = 0; linear_search(a, n, 1);  unsigned long lin = ops;               /* odd key: absent */
        ops = 0; binary_search(a, n, 1);  unsigned long bin = ops;
        ops = 0; bubble_sort(b, n);       unsigned long bub = ops;
        for (int i = 0; i < n; i++) b[i] = n - i;
        ops = 0; merge_sort(b, tmp, n);   unsigned long mer = ops;
        printf("%8d %10lu %10lu %12lu %12lu %14.0f\n", n, lin, bin, bub, mer, n * log2((double)n));
        free(a); free(b); free(tmp);
    }
    printf("\nFibonacci calls:   n   naive     memoized\n");
    for (int n = 10; n <= 35; n += 5) {
        uint64_t memo[64] = {0};
        fib_calls = 0; uint64_t r1 = fib_naive(n); unsigned long c1 = fib_calls;
        fib_calls = 0; uint64_t r2 = fib_memo(n, memo); unsigned long c2 = fib_calls;
        printf("                  %2d %9lu %8lu   (fib=%llu%s)\n", n, c1, c2, (unsigned long long)r1, r1 == r2 ? "" : " MISMATCH");
    }
    return 0;
}
