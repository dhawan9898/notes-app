void longest_palindrome(const char *s, int *start, int *len) {
    int n = (int)strlen(s);
    *start = 0;
    *len = n > 0;
    for (int c = 0; c < n; c++) {                       /* only odd-length palindromes: misses "bb" */
        int lo = c, hi = c;
        while (lo >= 0 && hi < n && s[lo] == s[hi]) { lo--; hi++; }
        if (hi - lo - 1 > *len) { *len = hi - lo - 1; *start = lo + 1; }
    }
}
