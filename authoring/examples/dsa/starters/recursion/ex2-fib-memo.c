// Exercise 2: Fibonacci with memoisation. Same answers, far fewer calls.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long long calls;      /* the tests count calls: keep the calls++ line */
long long memo[91];         /* memo[n] == 0 means "not computed yet" */

/* ===== Your code: edit below this line ===== */
long long fib_memo(int n) {
    calls++;
    if (n < 2) return n;
    return fib_memo(n - 1) + fib_memo(n - 2);      /* naive: recomputes the same values again and again */
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
    calls = 0;
    check_int("fib_memo(30)", fib_memo(30), 832040);
    printf("     fib_memo(30) made %lld calls\n", calls);
    check_int("fib_memo(30) made at most 2 * 30 calls", calls <= 60, 1);
    if (calls <= 60) {
        check_int("fib_memo(90)", fib_memo(90), 2880067194370816120LL);
    } else {
        checks++; failed++;
        printf("FAIL fib_memo(90): skipped, it would take years without memoisation\n");
    }
    return summary();
}
