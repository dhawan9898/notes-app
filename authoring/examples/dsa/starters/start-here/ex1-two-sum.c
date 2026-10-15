// Exercise 1: two-sum on a sorted array, faster than checking every pair.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* If two elements at different indexes add up to target, store the indexes (i < j) and return 1; otherwise return 0. */
int two_sum_sorted(const int *a, int n, int target, int *i_out, int *j_out) {
    (void)a; (void)n; (void)target; (void)i_out; (void)j_out;
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
    int a[] = {1, 3, 4, 7, 9, 12};
    int i = -1, j = -1;
    check_int("two_sum_sorted(a, 16) found", two_sum_sorted(a, 6, 16, &i, &j), 1);
    check_int("  a[i] + a[j]", (i >= 0 && j >= 0) ? a[i] + a[j] : -1, 16);
    check_int("  i < j", i < j, 1);
    check_int("two_sum_sorted(a, 13) (first and last)", two_sum_sorted(a, 6, 13, &i, &j), 1);
    check_int("two_sum_sorted(a, 100)", two_sum_sorted(a, 6, 100, &i, &j), 0);
    check_int("two_sum_sorted(a, 2) (an element cannot pair with itself)", two_sum_sorted(a, 6, 2, &i, &j), 0);
    check_int("two_sum_sorted(empty array, 0)", two_sum_sorted(a, 0, 0, &i, &j), 0);
    return summary();
}
