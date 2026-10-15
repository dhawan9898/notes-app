// Exercise 7: balanced brackets with a stack.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Return 1 if every (, [ and { is closed by the matching bracket in the right order. Other characters are ignored. */
int balanced(const char *s) {
    (void)s;
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
    check_int("balanced(\"{[()]}\")", balanced("{[()]}"), 1);
    check_int("balanced(\"([)]\") (counting brackets is not enough)", balanced("([)]"), 0);
    check_int("balanced(\"((\")", balanced("(("), 0);
    check_int("balanced(\")(\")", balanced(")("), 0);
    check_int("balanced(\"\")", balanced(""), 1);
    check_int("balanced(\"f(a[i], {x})\")", balanced("f(a[i], {x})"), 1);
    return summary();
}
