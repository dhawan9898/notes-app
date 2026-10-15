// Exercise 3: make swap_ints exchange the caller's two variables.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* The tests call swap_ints(&a, &b). This version makes the same mistake as the by-value one in the exercise:
   it swaps its own copies (here, copies of the two pointers) and leaves the caller's ints alone. */
void swap_ints(int *a, int *b) {
    int *t = a;
    a = b;
    b = t;
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
    int a = 1, b = 2;
    swap_ints(&a, &b);
    check_int("a after swap_ints(&a, &b)", a, 2);
    check_int("b after swap_ints(&a, &b)", b, 1);
    int x = -7, y = 7;
    swap_ints(&x, &y);
    check_int("x after swap_ints(&x, &y)", x, 7);
    check_int("y after swap_ints(&x, &y)", y, -7);
    return summary();
}
