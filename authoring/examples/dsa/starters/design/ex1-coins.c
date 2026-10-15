// Exercise 1: fewest coins for a target (dynamic programming), and the number of ways to make it.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Fewest coins (unlimited supply of each) adding up to target, or -1 if impossible. */
int min_coins(const int *coins, int nc, int target) {
    (void)coins; (void)nc; (void)target;
    return -1;
}

/* Number of different multisets of coins adding up to target (order does not matter). */
long long count_ways(const int *coins, int nc, int target) {
    (void)coins; (void)nc; (void)target;
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
    const int us[] = {25, 10, 5, 1}, odd[] = {4, 3, 1}, big[] = {5, 10}, small[] = {1, 5, 10};
    check_int("min_coins({25,10,5,1}, 63)", min_coins(us, 4, 63), 6);
    check_int("min_coins({4,3,1}, 6) (greedy would use 3)", min_coins(odd, 3, 6), 2);
    check_int("min_coins({5,10}, 3) (impossible)", min_coins(big, 2, 3), -1);
    check_int("min_coins(any, 0)", min_coins(us, 4, 0), 0);
    check_int("count_ways({1,5,10}, 10)", count_ways(small, 3, 10), 4);
    check_int("count_ways({25,10,5,1}, 100)", count_ways(us, 4, 100), 242);
    return summary();
}
