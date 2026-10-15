// Exercise 4: count inversions (i < j with a[i] > a[j]) in O(n log n) by extending merge sort.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* May reorder a. tmp has room for n ints. */
long long count_inversions(int *a, int *tmp, int n) {
    (void)a; (void)tmp; (void)n;
    return 0;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    int a[] = {8, 4, 2, 1, 7, 3, 9, 5}, tmp[2000];
    check_int("count_inversions({8,4,2,1,7,3,9,5})", count_inversions(a, tmp, 8), 13);
    int sorted[] = {1, 2, 3, 4};
    check_int("count_inversions(sorted)", count_inversions(sorted, tmp, 4), 0);
    int dup[] = {2, 2, 1};
    check_int("count_inversions({2,2,1}) (equal keys are not inversions)", count_inversions(dup, tmp, 3), 2);
    static int big[2000];
    for (int i = 0; i < 2000; i++) big[i] = 2000 - i;
    check_int("count_inversions(2000 descending)", count_inversions(big, tmp, 2000), 1999000);
    return summary();
}
