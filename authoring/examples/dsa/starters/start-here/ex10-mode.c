// Exercise 10: the most frequent value (values 0 to 99), with a counting array.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* On a tie, return the smallest of the most frequent values. n > 0. */
int mode_of(const int *a, int n) {
    (void)a; (void)n;
    return -1;
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
    int data[] = {4, 7, 4, 2, 7, 7, 9};
    check_int("mode_of({4,7,4,2,7,7,9})", mode_of(data, 7), 7);
    int tie[] = {3, 1, 3, 1};
    check_int("mode_of({3,1,3,1}) (tie: smallest)", mode_of(tie, 4), 1);
    int one[] = {99};
    check_int("mode_of({99})", mode_of(one, 1), 99);
    int zeros[] = {0, 0, 5};
    check_int("mode_of({0,0,5})", mode_of(zeros, 3), 0);
    return summary();
}
