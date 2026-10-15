// Exercise 4: count the ways to place n non-attacking queens on an n x n board (backtracking).
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
long long count_queens(int n) {
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
    const int ns[] = {1, 2, 3, 4, 5, 6, 8, 10};
    const long long want[] = {1, 0, 0, 2, 10, 4, 92, 724};
    for (int i = 0; i < 8; i++) {
        char what[32];
        snprintf(what, sizeof what, "count_queens(%d)", ns[i]);
        check_int(what, count_queens(ns[i]), want[i]);
    }
    return summary();
}
