#include <limits.h>
#include "mathutil.h"

int clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Returns 0 and stores a+b, or -1 on overflow. */
int safe_add(int a, int b, int *out) {
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) return -1;
    *out = a + b;
    return 0;
}

/* Integer square root by binary search; untested branch on purpose. */
int isqrt(int n) {
    if (n < 0) return -1;
    if (n < 2) return n;
    int lo = 1, hi = n < 46341 ? n : 46341;
    while (lo < hi) {
        int mid = lo + (hi - lo + 1) / 2;
        if ((long long)mid * mid <= n) lo = mid; else hi = mid - 1;
    }
    return lo;
}
