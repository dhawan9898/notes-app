// Exercise 2: is a sentence a palindrome if you ignore case and non-letters?
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
int is_palindrome(const char *s) {
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
    check_int("is_palindrome(\"A man, a plan, a canal: Panama\")", is_palindrome("A man, a plan, a canal: Panama"), 1);
    check_int("is_palindrome(\"hello\")", is_palindrome("hello"), 0);
    check_int("is_palindrome(\"Racecar\") (case)", is_palindrome("Racecar"), 1);
    check_int("is_palindrome(\"\") (empty)", is_palindrome(""), 1);
    check_int("is_palindrome(\"!!\") (no letters)", is_palindrome("!!"), 1);
    check_int("is_palindrome(\"ab, cb\")", is_palindrome("ab, cb"), 0);
    return summary();
}
