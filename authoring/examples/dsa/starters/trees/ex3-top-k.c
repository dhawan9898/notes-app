// Exercise 3: the k largest values of a stream with O(k) memory (a min-heap of size k).
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Write the k largest values of data[0..n) into out[0..k), smallest first. k <= n. */
void top_k(const int *data, int n, int k, int *out) {
    (void)data; (void)n; (void)k; (void)out;
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
    int data[] = {42, 7, 99, 15, 3, 88, 61, 27, 54, 70, 12, 95};
    int out[12] = {0};
    top_k(data, 12, 4, out);
    const int want4[] = {70, 88, 95, 99};
    check_arr("top_k(data, 4)", out, 4, want4, 4);
    top_k(data, 12, 1, out);
    const int want1[] = {99};
    check_arr("top_k(data, 1)", out, 1, want1, 1);
    int dup[] = {5, 9, 9, 1, 9};
    top_k(dup, 5, 3, out);
    const int want_dup[] = {9, 9, 9};
    check_arr("top_k({5,9,9,1,9}, 3)", out, 3, want_dup, 3);
    int asc[] = {1, 2, 3, 4, 5, 6};
    top_k(asc, 6, 6, out);
    check_arr("top_k(1..6, 6) (k == n)", out, 6, asc, 6);
    return summary();
}
