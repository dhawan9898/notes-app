// Exercise 2: the longest palindromic substring, by expanding around each of the 2n - 1 centres.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Store where the longest palindrome starts and its length. If several have the maximum length, any of them is fine. */
void longest_palindrome(const char *s, int *start, int *len) {
    (void)s;
    *start = 0;
    *len = 0;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

static void check_palindrome(const char *s, int want_len) {
    int start = -1, len = -1;
    longest_palindrome(s, &start, &len);
    int ok = len == want_len && start >= 0 && start + len <= (int)strlen(s);
    for (int i = 0; ok && i < len / 2; i++) ok = s[start + i] == s[start + len - 1 - i];
    checks++;
    if (!ok) failed++;
    printf("%s longest palindrome in \"%s\": expected length %d, got \"%.*s\" (length %d)\n",
           ok ? "PASS" : "FAIL", s, want_len, len > 0 && start >= 0 ? len : 0, s + (start > 0 ? start : 0), len);
}

int main(void) {
    check_palindrome("babad", 3);
    check_palindrome("cbbd", 2);
    check_palindrome("forgeeksskeegfor", 10);
    check_palindrome("a", 1);
    check_palindrome("abc", 1);
    check_palindrome("", 0);
    return summary();
}
