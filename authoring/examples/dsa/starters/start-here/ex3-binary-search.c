// Exercise 3: binary search. Return the index of key in a sorted array, or -1.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
int bsearch_idx(const int *a, int n, int key) {
    (void)a; (void)n; (void)key;
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
    int a[] = {1, 3, 4, 7, 9, 12};
    check_int("bsearch_idx(a, 9)", bsearch_idx(a, 6, 9), 4);
    check_int("bsearch_idx(a, 1) (first)", bsearch_idx(a, 6, 1), 0);
    check_int("bsearch_idx(a, 12) (last)", bsearch_idx(a, 6, 12), 5);
    check_int("bsearch_idx(a, 8) (missing)", bsearch_idx(a, 6, 8), -1);
    check_int("bsearch_idx(a, 0) (below all)", bsearch_idx(a, 6, 0), -1);
    check_int("bsearch_idx(a, 99) (above all)", bsearch_idx(a, 6, 99), -1);
    int one[] = {5};
    check_int("bsearch_idx({5}, 5)", bsearch_idx(one, 1, 5), 0);
    check_int("bsearch_idx(empty, 5)", bsearch_idx(one, 0, 5), -1);
    return summary();
}
