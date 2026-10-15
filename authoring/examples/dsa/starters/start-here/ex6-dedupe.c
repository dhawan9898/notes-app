// Exercise 6: remove duplicates from a sorted array in place; return the new length.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
int dedupe(int *a, int n) {
    (void)a;
    return n;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static void print_arr(const int *v, int n) {
    printf("{");
    for (int i = 0; i < n; i++) printf(i ? ", %d" : "%d", v[i]);
    printf("}");
}

static void check_arr(const char *what, const int *got, int got_n, const int *want, int want_n) {
    int same = got_n == want_n;
    for (int i = 0; same && i < want_n; i++) same = got[i] == want[i];
    checks++;
    if (!same) failed++;
    printf("%s %s", same ? "PASS" : "FAIL", what);
    if (same) { printf(" = "); print_arr(got, got_n); }
    else { printf(": expected "); print_arr(want, want_n); printf(", got "); print_arr(got, got_n); }
    printf("\n");
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    int d[] = {1, 1, 2, 2, 2, 3, 5, 5};
    int n = dedupe(d, 8);
    const int want[] = {1, 2, 3, 5};
    check_arr("dedupe({1,1,2,2,2,3,5,5})", d, n, want, 4);
    int same[] = {7, 7, 7};
    n = dedupe(same, 3);
    const int want2[] = {7};
    check_arr("dedupe({7,7,7})", same, n, want2, 1);
    int distinct[] = {1, 2, 3};
    n = dedupe(distinct, 3);
    check_arr("dedupe({1,2,3}) (nothing to remove)", distinct, n, distinct, 3);
    check_int("dedupe(empty)", dedupe(d, 0), 0);
    return summary();
}
