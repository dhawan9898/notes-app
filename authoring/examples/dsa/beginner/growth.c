#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Count the work (comparisons) three ways of finding a value among n numbers need. */
static long linear_steps(const int *a, int n, int key) { long steps = 0; for (int i = 0; i < n; i++) { steps++; if (a[i] == key) break; } return steps; }
static long binary_steps(const int *a, int n, int key) { long steps = 0; int lo = 0, hi = n - 1; while (lo <= hi) { steps++; int mid = lo + (hi - lo) / 2; if (a[mid] == key) break; if (a[mid] < key) lo = mid + 1; else hi = mid - 1; } return steps; }
static long pairs_steps(int n) { long steps = 0; for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) steps++; return steps; }   /* compare every pair */

int main(void) {
    printf("%10s | %14s %14s %16s\n", "n", "linear search", "binary search", "all pairs (n^2)");
    for (int n = 10; n <= 10000000; n *= 10) {
        int *a = malloc((size_t)n * sizeof *a); for (int i = 0; i < n; i++) a[i] = 2 * i;       /* sorted */
        long lin = linear_steps(a, n, 2 * (n - 1)), bin = binary_steps(a, n, 2 * (n - 1));        /* worst case for linear: the key is last */
        if (n <= 10000) printf("%10d | %14ld %14ld %16ld\n", n, lin, bin, pairs_steps(n));
        else printf("%10d | %14ld %14ld %16s\n", n, lin, bin, "(too slow to run)");
        free(a);
    }
    return 0;
}
