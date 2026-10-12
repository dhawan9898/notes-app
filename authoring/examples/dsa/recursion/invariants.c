#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Insertion sort annotated with its loop invariants as runtime assertions. */
static int is_sorted(const int *a, int n) { for (int i = 1; i < n; i++) if (a[i - 1] > a[i]) return 0; return 1; }

static void insertion_sort(int *a, int n) {
    for (int i = 1; i < n; i++) {
        assert(is_sorted(a, i));                       /* INVARIANT: a[0..i) is sorted at the start of every iteration */
        int key = a[i], j = i - 1;
        while (j >= 0 && a[j] > key) { a[j + 1] = a[j]; j--; }
        a[j + 1] = key;
        assert(is_sorted(a, i + 1));                   /* after the body the invariant holds for i+1 (maintenance) */
    }
    assert(is_sorted(a, n));                           /* termination: i == n, so the whole array is sorted */
}

/* Binary search lower_bound with the invariant: every index < lo has a[idx] < key; every index >= hi has a[idx] >= key. */
static int lower_bound(const int *a, int n, int key) {
    int lo = 0, hi = n;
    while (lo < hi) {
        for (int i = 0; i < lo; i++) assert(a[i] < key);
        for (int i = hi; i < n; i++) assert(a[i] >= key);
        int mid = lo + (hi - lo) / 2;
        if (a[mid] < key) lo = mid + 1; else hi = mid;
    }
    return lo;                                         /* lo == hi: first index with a[idx] >= key */
}

int main(void) {
    srand(7);
    long trials = 0;
    for (int t = 0; t < 2000; t++) {
        int n = rand() % 40, a[40];
        for (int i = 0; i < n; i++) a[i] = rand() % 20;
        insertion_sort(a, n);
        int key = rand() % 22, pos = lower_bound(a, n, key);
        assert(pos == 0 || a[pos - 1] < key);
        assert(pos == n || a[pos] >= key);
        trials++;
    }
    printf("%ld random arrays: all invariants and postconditions held\n", trials);
    int broken[] = {3, 1, 2};
    int ok = is_sorted(broken, 3);
    printf("is_sorted({3,1,2}) = %d  (an assertion on this would have fired)\n", ok);
    return 0;
}
