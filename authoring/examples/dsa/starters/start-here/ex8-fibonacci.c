// Exercise 8: Fibonacci, recursive and iterative; they must agree.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
long long fib_rec(int n) {
    (void)n;
    return 0;
}

long long fib_iter(int n) {
    (void)n;
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
    check_int("fib_iter(0)", fib_iter(0), 0);
    check_int("fib_iter(1)", fib_iter(1), 1);
    check_int("fib_iter(10)", fib_iter(10), 55);
    check_int("fib_iter(50)", fib_iter(50), 12586269025LL);
    int agree = 1;
    for (int n = 0; n <= 25; n++) agree &= fib_rec(n) == fib_iter(n);
    check_int("fib_rec and fib_iter agree for 0..25", agree, 1);
    check_int("fib_rec(25)", fib_rec(25), 75025);
    return summary();
}
