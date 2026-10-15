// Exercise 8: find and fix the bug in sum_first_n.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
int sum_first_n(const int *v, int n) {
    int total = 0;
    for (int i = 0; i <= n; i++) total += v[i];
    return total;
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
    int v[] = {1, 2, 3, 4, 99};     /* the 99 is not part of the first 4 elements */
    check_int("sum_first_n(v, 4)", sum_first_n(v, 4), 10);
    check_int("sum_first_n(v, 1)", sum_first_n(v, 1), 1);
    check_int("sum_first_n(v, 0)", sum_first_n(v, 0), 0);
    check_int("sum_first_n(v, 5)", sum_first_n(v, 5), 109);
    return summary();
}
