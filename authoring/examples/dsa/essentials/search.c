#include <stdio.h>

/* Linear search: look at every item until found. Works on any array. */
static int linear_search(const int *a, int n, int target, int *steps) {
    for (int i = 0; i < n; i++) {
        (*steps)++;
        if (a[i] == target) return i;
    }
    return -1;
}

/* Binary search: needs a SORTED array; each step throws away half of what is left. */
static int binary_search(const int *a, int n, int target, int *steps) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;          /* not (lo + hi) / 2: that can overflow for huge arrays */
        (*steps)++;
        if (a[mid] == target) return mid;
        if (a[mid] < target) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

int main(void) {
    int a[1000];
    for (int i = 0; i < 1000; i++) a[i] = i * 3;          /* sorted: 0, 3, 6, ... 2997 */

    int targets[] = {2997, 1500, 7};                       /* last item, middle item, absent */
    for (int t = 0; t < 3; t++) {
        int s1 = 0, s2 = 0;
        int i1 = linear_search(a, 1000, targets[t], &s1);
        int i2 = binary_search(a, 1000, targets[t], &s2);
        printf("target %4d: linear -> index %4d in %4d steps | binary -> index %4d in %2d steps\n",
               targets[t], i1, s1, i2, s2);
    }
    return 0;
}
