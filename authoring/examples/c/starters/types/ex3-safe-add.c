// Exercise 3: overflow-safe addition. Never perform an addition that overflows.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Store a + b in *out and return true if the true sum fits in an int; otherwise return false (leave *out alone). */
bool safe_add(int a, int b, int *out) {
    (void)a; (void)b; (void)out;
    return false;
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
    int r = 0;
    check_int("safe_add(2000000000, 2000000000) fits?", safe_add(2000000000, 2000000000, &r), 0);
    r = 0;
    check_int("safe_add(2000000000, -5) fits?", safe_add(2000000000, -5, &r), 1);
    check_int("  and stores", r, 1999999995);
    r = 0;
    check_int("safe_add(INT_MAX, 0) fits?", safe_add(INT_MAX, 0, &r), 1);
    check_int("  and stores", r, INT_MAX);
    check_int("safe_add(INT_MAX, 1) fits?", safe_add(INT_MAX, 1, &r), 0);
    check_int("safe_add(INT_MIN, -1) fits?", safe_add(INT_MIN, -1, &r), 0);
    r = 0;
    check_int("safe_add(INT_MIN, INT_MAX) fits?", safe_add(INT_MIN, INT_MAX, &r), 1);
    check_int("  and stores", r, -1);
    return summary();
}
