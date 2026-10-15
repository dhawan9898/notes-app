int edit_distance(const char *a, const char *b) {
    int n = (int)strlen(a), m = (int)strlen(b);
    int d[64][64];                                   /* d[i][j]: distance between a[0..i) and b[0..j) */
    for (int i = 0; i <= n; i++) d[i][0] = i;
    for (int j = 0; j <= m; j++) d[0][j] = j;
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= m; j++) {
            int sub = d[i - 1][j - 1] + (a[i - 1] != b[j - 1]);
            int del = d[i - 1][j] + 1, ins = d[i][j - 1] + 1;
            d[i][j] = sub < del ? (sub < ins ? sub : ins) : (del < ins ? del : ins);
        }
    return d[n][m];
}
