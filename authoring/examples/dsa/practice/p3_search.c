#include <stdio.h>

/* First index i with a[i] >= x (n if none): the "lower bound". Invariant: everything left of lo is < x, everything from hi on is >= x. */
static int lower_bound(const int *a, int n, int x) {
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (a[mid] < x) lo = mid + 1; else hi = mid;
    }
    return lo;
}
static int upper_bound(const int *a, int n, int x) {           /* first index with a[i] > x */
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (a[mid] <= x) lo = mid + 1; else hi = mid;
    }
    return lo;
}

int main(void) {
    int a[] = {1, 3, 3, 3, 5, 8, 8, 13};
    int n = (int)(sizeof a / sizeof a[0]);
    int queries[] = {3, 8, 4, 0, 20};
    for (int i = 0; i < 5; i++) {
        int x = queries[i], lo = lower_bound(a, n, x), hi = upper_bound(a, n, x);
        printf("x = %2d: lower_bound %d, upper_bound %d, occurrences %d%s\n", x, lo, hi, hi - lo, hi == lo ? "  (absent: lower_bound is where it would be inserted)" : "");
    }
    return 0;
}
