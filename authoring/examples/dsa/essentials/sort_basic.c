#include <stdio.h>
#include <stdlib.h>

static void print(const char *label, const int *a, int n) {
    printf("%-10s", label);
    for (int i = 0; i < n; i++) printf(" %d", a[i]);
    printf("\n");
}

/* Selection sort: repeatedly pick the smallest remaining item and put it next. */
static long selection_sort(int *a, int n) {
    long compares = 0;
    for (int i = 0; i < n - 1; i++) {
        int min = i;
        for (int j = i + 1; j < n; j++) { compares++; if (a[j] < a[min]) min = j; }
        int tmp = a[i]; a[i] = a[min]; a[min] = tmp;
    }
    return compares;
}

/* Insertion sort: grow a sorted prefix by sliding each new item left into place. */
static long insertion_sort(int *a, int n) {
    long compares = 0;
    for (int i = 1; i < n; i++) {
        int key = a[i], j = i - 1;
        while (j >= 0) { compares++; if (a[j] <= key) break; a[j + 1] = a[j]; j--; }
        a[j + 1] = key;
    }
    return compares;
}

static int cmp_int(const void *x, const void *y) {      /* the comparator qsort needs */
    int a = *(const int *)x, b = *(const int *)y;
    return (a > b) - (a < b);                            /* never "a - b": that can overflow */
}

int main(void) {
    int small[] = {5, 2, 9, 1, 7, 3};
    int copy1[6], copy2[6];
    for (int i = 0; i < 6; i++) copy1[i] = copy2[i] = small[i];
    print("input", small, 6);
    selection_sort(copy1, 6); print("selection", copy1, 6);
    insertion_sort(copy2, 6); print("insertion", copy2, 6);

    enum { N = 2000 };
    static int big1[N], big2[N], big3[N], sorted[N];
    unsigned seed = 12345;                               /* fixed seed: same numbers every run */
    for (int i = 0; i < N; i++) { seed = seed * 1103515245u + 12345u; big1[i] = big2[i] = big3[i] = (int)((seed >> 8) % 100000); }
    for (int i = 0; i < N; i++) sorted[i] = i;           /* already sorted input */

    printf("\ncomparisons for %d items:\n", N);
    printf("  selection sort, random input : %ld\n", selection_sort(big1, N));
    printf("  insertion sort, random input : %ld\n", insertion_sort(big2, N));
    printf("  insertion sort, sorted input : %ld\n", insertion_sort(sorted, N));
    qsort(big3, N, sizeof big3[0], cmp_int);
    int ok = 1; for (int i = 1; i < N; i++) if (big3[i - 1] > big3[i]) ok = 0;
    printf("  qsort from the C library     : sorted correctly = %s\n", ok ? "yes" : "no");
    return 0;
}
