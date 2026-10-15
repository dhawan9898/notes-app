// Exercise 4: b^e with O(log e) multiplications instead of e.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Multiply only through mul(): the tests count the multiplications. */
static long long mults;
static long long mul(long long a, long long b) { mults++; return a * b; }

/* ===== Your code: edit below this line ===== */
long long power_fast(long long b, int e) {
    long long r = 1;
    for (int i = 0; i < e; i++) r = mul(r, b);     /* e multiplications: make it O(log e) */
    return r;
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
    check_int("power_fast(3, 13)", power_fast(3, 13), 1594323);
    check_int("power_fast(2, 62)", power_fast(2, 62), 4611686018427387904LL);
    check_int("power_fast(7, 0)", power_fast(7, 0), 1);
    check_int("power_fast(-2, 5)", power_fast(-2, 5), -32);
    mults = 0;
    check_int("power_fast(1, 1000)", power_fast(1, 1000), 1);
    printf("     power_fast(1, 1000) used %lld multiplications\n", mults);
    check_int("power_fast(1, 1000) used at most 20 multiplications", mults <= 20, 1);
    return summary();
}
