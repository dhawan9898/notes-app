// Exercise 4: are two words made of the same letters (same counts)?
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Words contain only the letters a-z, in either case. */
int is_anagram(const char *a, const char *b) {
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
    check_int("is_anagram(\"listen\", \"silent\")", is_anagram("listen", "silent"), 1);
    check_int("is_anagram(\"apple\", \"pale\")", is_anagram("apple", "pale"), 0);
    check_int("is_anagram(\"aab\", \"abb\") (same letters, different counts)", is_anagram("aab", "abb"), 0);
    check_int("is_anagram(\"Dusty\", \"study\") (case)", is_anagram("Dusty", "study"), 1);
    check_int("is_anagram(\"\", \"\")", is_anagram("", ""), 1);
    return summary();
}
