// Exercise 3: find every occurrence of a pattern with a rolling hash (Rabin-Karp).
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Store the starting index of each occurrence of pat in text (overlaps included) in pos[], at most max of them,
   and return how many there are. Update the window hash in O(1) per step; confirm candidates character by character. */
int find_all(const char *text, const char *pat, int pos[], int max) {
    (void)text; (void)pat; (void)pos; (void)max;
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
    int pos[16] = {0};
    int n = find_all("the cat sat on the mat with the other cat", "the", pos, 16);
    const int want[] = {0, 15, 28, 33};
    check_arr("positions of \"the\"", pos, n, want, 4);
    n = find_all("aaaa", "aa", pos, 16);
    const int overlap[] = {0, 1, 2};
    check_arr("positions of \"aa\" in \"aaaa\" (overlapping)", pos, n, overlap, 3);
    check_int("find_all(\"abc\", \"abcd\") (pattern longer than text)", find_all("abc", "abcd", pos, 16), 0);
    check_int("find_all(\"abc\", \"x\")", find_all("abc", "x", pos, 16), 0);
    n = find_all("abc", "c", pos, 16);
    const int last[] = {2};
    check_arr("positions of \"c\" in \"abc\" (last window)", pos, n, last, 1);
    return summary();
}
