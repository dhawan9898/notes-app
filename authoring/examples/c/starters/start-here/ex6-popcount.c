// Exercise 6: count the 1 bits of a 32-bit number.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
int popcount(uint32_t x) {
    (void)x;
    return 0;   /* look at the lowest bit with x & 1, then shift */
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
    check_int("popcount(0)", popcount(0), 0);
    check_int("popcount(1)", popcount(1), 1);
    check_int("popcount(255)", popcount(255), 8);
    check_int("popcount(0x80000001)", popcount(0x80000001u), 2);
    check_int("popcount(0xFFFFFFFF)", popcount(0xFFFFFFFFu), 32);
    return summary();
}
