int find_all(const char *text, const char *pat, int pos[], int max) {
    const unsigned long long B = 257, M = 1000000007ULL;
    int n = (int)strlen(text), m = (int)strlen(pat), found = 0;
    if (m == 0 || m > n) return 0;
    unsigned long long hp = 0, ht = 0, top = 1;           /* top = B^(m-1) mod M */
    for (int i = 0; i < m; i++) {
        hp = (hp * B + (unsigned char)pat[i]) % M;
        ht = (ht * B + (unsigned char)text[i]) % M;
        if (i) top = top * B % M;
    }
    for (int i = 0; ; i++) {
        if (hp == ht && memcmp(text + i, pat, (size_t)m) == 0) { if (found < max) pos[found] = i; found++; }
        if (i + m >= n) break;
        ht = (ht + M - (unsigned char)text[i] * top % M) % M;      /* drop the leftmost character */
        ht = (ht * B + (unsigned char)text[i + m]) % M;           /* append the next one */
    }
    return found;
}
