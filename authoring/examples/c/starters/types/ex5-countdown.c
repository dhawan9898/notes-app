// starter-warnings: expected (the compiler warning is part of the exercise)
// Exercise 5: visit indexes n-1 down to 0 with a size_t counter.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Record every index visited, from n-1 down to 0, in out[] and return how many were visited.
   The loop below never ends on its own: steps < 5 is only a safety cap. */
int countdown(size_t n, size_t out[]) {
    int steps = 0;
    for (size_t i = n - 1; i >= 0 && steps < 5; i--) out[steps++] = i;
    return steps;
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
    size_t seen[8] = {0};
    check_int("countdown(3) visits", countdown(3, seen), 3);
    check_int("  first index", (long long)seen[0], 2);
    check_int("  last index", (long long)seen[2], 0);
    check_int("countdown(1) visits", countdown(1, seen), 1);
    check_int("countdown(0) visits", countdown(0, seen), 0);
    return summary();
}
