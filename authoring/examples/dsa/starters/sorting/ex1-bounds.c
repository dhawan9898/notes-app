// Exercise 1: lower_bound and upper_bound by binary search.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* First index i with a[i] >= x, or n if there is none. */
int lower_bound(const int *a, int n, int x) {
    (void)a; (void)n; (void)x;
    return 0;
}

/* First index i with a[i] > x, or n if there is none. */
int upper_bound(const int *a, int n, int x) {
    (void)a; (void)n; (void)x;
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
    int a[] = {1, 3, 3, 3, 5, 8, 8, 13};
    const int xs[] = {3, 8, 4, 0, 20, 1, 13};
    const int lo_want[] = {1, 5, 4, 0, 8, 0, 7}, hi_want[] = {4, 7, 4, 0, 8, 1, 8};
    for (int i = 0; i < 7; i++) {
        char what[40];
        snprintf(what, sizeof what, "lower_bound(a, %d)", xs[i]);
        check_int(what, lower_bound(a, 8, xs[i]), lo_want[i]);
        snprintf(what, sizeof what, "upper_bound(a, %d)", xs[i]);
        check_int(what, upper_bound(a, 8, xs[i]), hi_want[i]);
    }
    check_int("lower_bound(empty, 5)", lower_bound(a, 0, 5), 0);
    return summary();
}
