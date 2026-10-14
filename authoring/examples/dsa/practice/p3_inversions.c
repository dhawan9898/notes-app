#include <stdio.h>
#include <string.h>

/* Merge sort that also counts inversions: pairs (i < j) with a[i] > a[j]. An inversion is found whenever an element
   from the right half is placed before elements still waiting in the left half. */
static long sort_count(int *a, int *tmp, int lo, int hi) {
    if (hi - lo < 2) return 0;
    int mid = lo + (hi - lo) / 2;
    long inv = sort_count(a, tmp, lo, mid) + sort_count(a, tmp, mid, hi);
    int i = lo, j = mid, k = lo;
    while (i < mid && j < hi) {
        if (a[i] <= a[j]) tmp[k++] = a[i++];
        else { tmp[k++] = a[j++]; inv += mid - i; }              /* a[j] is smaller than ALL of a[i..mid) */
    }
    while (i < mid) tmp[k++] = a[i++];
    while (j < hi) tmp[k++] = a[j++];
    memcpy(a + lo, tmp + lo, (size_t)(hi - lo) * sizeof *a);
    return inv;
}

static long brute(const int *a, int n) { long c = 0; for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (a[i] > a[j]) c++; return c; }

int main(void) {
    int a[] = {8, 4, 2, 1, 7, 3, 9, 5}, copy[8], tmp[8];
    memcpy(copy, a, sizeof a);
    long fast = sort_count(a, tmp, 0, 8);
    printf("inversions by merge sort: %ld, by brute force: %ld\n", fast, brute(copy, 8));
    printf("sorted:"); for (int i = 0; i < 8; i++) printf(" %d", a[i]); printf("\n");
    return 0;
}
