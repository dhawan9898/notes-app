// Exercise 5: reverse a string in place.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
void reverse(char *s) {
    (void)s;    /* swap pairs of characters from both ends */
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_str(const char *what, const char *got, const char *want) {
    checks++;
    if (got && strcmp(got, want) == 0) printf("PASS %s = \"%s\"\n", what, got);
    else { failed++; printf("FAIL %s: expected \"%s\", got %s%s%s\n", what, want, got ? "\"" : "", got ? got : "NULL", got ? "\"" : ""); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    char s1[] = "stressed", s2[] = "ab", s3[] = "a", s4[] = "", s5[] = "racecar", s6[] = "hello world";
    reverse(s1); check_str("reverse(\"stressed\")", s1, "desserts");
    reverse(s2); check_str("reverse(\"ab\")", s2, "ba");
    reverse(s3); check_str("reverse(\"a\")", s3, "a");
    reverse(s4); check_str("reverse(\"\")", s4, "");
    reverse(s5); check_str("reverse(\"racecar\")", s5, "racecar");
    reverse(s6); check_str("reverse(\"hello world\")", s6, "dlrow olleh");
    return summary();
}
