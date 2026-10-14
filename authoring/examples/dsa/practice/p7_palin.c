#include <stdio.h>
#include <string.h>

/* Longest palindromic substring: try every centre (a letter, or the gap between two letters) and expand outwards. */
static void longest_palindrome(const char *s, int *best_start, int *best_len) {
    int n = (int)strlen(s);
    *best_start = 0; *best_len = n ? 1 : 0;
    for (int centre = 0; centre < 2 * n - 1; centre++) {
        int l = centre / 2, r = l + centre % 2;                  /* odd length: l == r, even length: r = l + 1 */
        while (l >= 0 && r < n && s[l] == s[r]) { l--; r++; }
        int len = r - l - 1;
        if (len > *best_len) { *best_len = len; *best_start = l + 1; }
    }
}

int main(void) {
    const char *tests[] = {"babad", "cbbd", "forgeeksskeegfor", "a", "abc"};
    for (int i = 0; i < 5; i++) {
        int start, len;
        longest_palindrome(tests[i], &start, &len);
        printf("%-18s -> \"%.*s\" (length %d)\n", tests[i], len, tests[i] + start, len);
    }
    return 0;
}
