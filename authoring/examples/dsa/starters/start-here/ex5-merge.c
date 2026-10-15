// Exercise 5: merge two sorted arrays into one sorted array in one pass.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* out has room for na + nb elements. */
void merge(const int *a, int na, const int *b, int nb, int *out) {
    (void)a; (void)na; (void)b; (void)nb; (void)out;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

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
    int a[] = {1, 3, 4, 7, 9, 12}, b[] = {2, 3, 10}, out[16] = {0};
    merge(a, 6, b, 3, out);
    const int want1[] = {1, 2, 3, 3, 4, 7, 9, 10, 12};
    check_arr("merge({1,3,4,7,9,12}, {2,3,10})", out, 9, want1, 9);
    int c[] = {20, 30};
    merge(a, 6, c, 2, out);
    const int want2[] = {1, 3, 4, 7, 9, 12, 20, 30};
    check_arr("merge(a, {20, 30}) (a runs out first)", out, 8, want2, 8);
    merge(c, 2, a, 0, out);
    const int want3[] = {20, 30};
    check_arr("merge({20, 30}, empty)", out, 2, want3, 2);
    return summary();
}
