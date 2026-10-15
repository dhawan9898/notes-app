// Exercise 4: write max_of, the largest element of an array (count > 0).
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
int max_of(const int *v, int count) {
    (void)v; (void)count;
    return 0;   /* replace with your loop */
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
    int a[] = {4, 9, -2, 7};
    int negatives[] = {-5, -1, -7};
    int one[] = {42};
    int last[] = {1, 2, 3, 100};
    check_int("max_of({4, 9, -2, 7})", max_of(a, 4), 9);
    check_int("max_of({-5, -1, -7}) (all negative)", max_of(negatives, 3), -1);
    check_int("max_of({42})", max_of(one, 1), 42);
    check_int("max_of({1, 2, 3, 100}) (largest is last)", max_of(last, 4), 100);
    return summary();
}
