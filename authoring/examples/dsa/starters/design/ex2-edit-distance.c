// Exercise 2: edit distance (insertions, deletions and substitutions).
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
int edit_distance(const char *a, const char *b) {
    (void)a; (void)b;
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
    check_int("edit_distance(\"kitten\", \"sitting\")", edit_distance("kitten", "sitting"), 3);
    check_int("edit_distance(\"intention\", \"execution\")", edit_distance("intention", "execution"), 5);
    check_int("edit_distance(\"flaw\", \"lawn\")", edit_distance("flaw", "lawn"), 2);
    check_int("edit_distance(\"\", \"abc\")", edit_distance("", "abc"), 3);
    check_int("edit_distance(\"same\", \"same\")", edit_distance("same", "same"), 0);
    return summary();
}
