void longest_palindrome(const char *s, int *start, int *len) {
    int n = (int)strlen(s);
    *start = 0;
    *len = n > 0;
    for (int centre = 0; centre < 2 * n - 1; centre++) {    /* even centres sit between two characters */
        int lo = centre / 2, hi = lo + centre % 2;
        while (lo >= 0 && hi < n && s[lo] == s[hi]) { lo--; hi++; }
        if (hi - lo - 1 > *len) { *len = hi - lo - 1; *start = lo + 1; }
    }
}
