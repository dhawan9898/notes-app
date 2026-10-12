#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

/* ---------- Levenshtein distance (insert / delete / substitute, each cost 1), two rolling rows: O(nm) time, O(min(n,m)) space ---------- */
static int lev(const char *a, int n, const char *b, int m) {
    if (n < m) { const char *t = a; a = b; b = t; int k = n; n = m; m = k; }       /* make b the shorter string: rows have m+1 cells */
    int *prev = malloc((size_t)(m + 1) * sizeof(int)), *cur = malloc((size_t)(m + 1) * sizeof(int));
    for (int j = 0; j <= m; j++) prev[j] = j;
    for (int i = 1; i <= n; i++) {
        cur[0] = i;
        for (int j = 1; j <= m; j++) {
            int sub = prev[j - 1] + (a[i - 1] != b[j - 1]), del = prev[j] + 1, ins = cur[j - 1] + 1;
            int best = sub < del ? sub : del; cur[j] = best < ins ? best : ins;
        }
        int *t = prev; prev = cur; cur = t;
    }
    int r = prev[m]; free(prev); free(cur); return r;
}
/* ---------- banded version: only cells with |i-j| <= k can lie on a path of cost <= k.  Returns the distance, or k+1 if it exceeds k.  O(n k). ---------- */
static int lev_banded(const char *a, int n, const char *b, int m, int k) {
    if (abs(n - m) > k) return k + 1;
    int W = 2 * k + 3, big = k + 1; int *prev = malloc((size_t)W * sizeof(int)), *cur = malloc((size_t)W * sizeof(int));
    /* cell (i, j) lives at index j - i + k + 1 */
    for (int x = 0; x < W; x++) prev[x] = big;
    for (int j = 0; j <= k && j <= m; j++) prev[j + k + 1] = j;
    for (int i = 1; i <= n; i++) {
        for (int x = 0; x < W; x++) cur[x] = big;
        for (int j = (i - k > 0 ? i - k : 0); j <= m && j <= i + k; j++) {
            int x = j - i + k + 1, v = big;
            if (j == 0) v = i <= k ? i : big;
            else {
                int sub = prev[x] + (a[i - 1] != b[j - 1]);                 /* (i-1, j-1) has the same diagonal index */
                int del = x + 1 < W ? prev[x + 1] + 1 : big;                /* (i-1, j) */
                int ins = cur[x - 1] + 1;                                   /* (i, j-1) */
                v = sub < del ? sub : del; if (ins < v) v = ins;
            }
            cur[x] = v > big ? big : v;
        }
        int *t = prev; prev = cur; cur = t;
    }
    int r = prev[m - n + k + 1]; free(prev); free(cur); return r > k ? k + 1 : r;
}
/* ---------- longest common subsequence length ---------- */
static int lcs(const char *a, int n, const char *b, int m) {
    int *prev = calloc((size_t)(m + 1), sizeof(int)), *cur = calloc((size_t)(m + 1), sizeof(int));
    for (int i = 1; i <= n; i++) { for (int j = 1; j <= m; j++) cur[j] = a[i - 1] == b[j - 1] ? prev[j - 1] + 1 : (prev[j] > cur[j - 1] ? prev[j] : cur[j - 1]); int *t = prev; prev = cur; cur = t; }
    int r = prev[m]; free(prev); free(cur); return r;
}
/* ---------- brute force: exponential recursion on tiny strings ---------- */
static int lev_rec(const char *a, int n, const char *b, int m) {
    if (!n) return m;
    if (!m) return n;
    int x = lev_rec(a + 1, n - 1, b + 1, m - 1) + (a[0] != b[0]), y = lev_rec(a + 1, n - 1, b, m) + 1, z = lev_rec(a, n, b + 1, m - 1) + 1;
    return x < y ? (x < z ? x : z) : (y < z ? y : z);
}

/* ---------- UTF-8: edit distance on bytes vs on code points ---------- */
static int utf8_decode(const char *s, int *cp) {                      /* returns bytes consumed; assumes valid UTF-8 */
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) { *cp = c; return 1; }
    if (c < 0xE0) { *cp = (c & 0x1F) << 6 | (s[1] & 0x3F); return 2; }
    if (c < 0xF0) { *cp = (c & 0x0F) << 12 | (s[1] & 0x3F) << 6 | (s[2] & 0x3F); return 3; }
    *cp = (c & 0x07) << 18 | (s[1] & 0x3F) << 12 | (s[2] & 0x3F) << 6 | (s[3] & 0x3F); return 4;
}
static int lev_cp(const char *a, const char *b) {
    int A[64], B[64], n = 0, m = 0, cp;
    while (*a) a += utf8_decode(a, &cp), A[n++] = cp;
    while (*b) b += utf8_decode(b, &cp), B[m++] = cp;
    int d[65][65];
    for (int i = 0; i <= n; i++) d[i][0] = i;
    for (int j = 0; j <= m; j++) d[0][j] = j;
    for (int i = 1; i <= n; i++) for (int j = 1; j <= m; j++) { int v = d[i - 1][j - 1] + (A[i - 1] != B[j - 1]); if (d[i - 1][j] + 1 < v) v = d[i - 1][j] + 1; if (d[i][j - 1] + 1 < v) v = d[i][j - 1] + 1; d[i][j] = v; }
    return d[n][m];
}

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    printf("lev(\"kitten\", \"sitting\") = %d, lev(\"flaw\", \"lawn\") = %d, lcs(\"AGGTAB\", \"GXTXAYB\") = %d\n", lev("kitten", 6, "sitting", 7), lev("flaw", 4, "lawn", 4), lcs("AGGTAB", 6, "GXTXAYB", 7));
    for (int round = 0; round < 20000; round++) {
        char a[10], b[10]; int n = (int)(rnd() % 8), m = (int)(rnd() % 8);
        for (int i = 0; i < n; i++) a[i] = (char)('a' + rnd() % 3);
        for (int i = 0; i < m; i++) b[i] = (char)('a' + rnd() % 3);
        int d = lev(a, n, b, m); assert(d == lev_rec(a, n, b, m)); assert(d == lev(b, m, a, n));
        for (int k = 0; k <= 8; k++) assert(lev_banded(a, n, b, m, k) == (d <= k ? d : k + 1));
        assert(n + m - 2 * lcs(a, n, b, m) >= d);                                                         /* indel distance bounds substitution distance from above */
    }
    printf("20000 random pairs: DP == exponential recursion, symmetric, banded == min(d, k+1) for k = 0..8, indel distance n+m-2*LCS >= edit distance\n\n");
    printf("UTF-8: \"naive\" vs \"na\xc3\xafve\": %d byte edits (the byte DP sees two substitutions/insertions), %d code-point edit\n", lev("naive", 5, "na\xc3\xafve", 6), lev_cp("naive", "na\xc3\xafve"));

    printf("\nedit distance of two random 20,000-character strings that differ by about 1%% of positions\n");
    int n = 20000; char *a = malloc((size_t)n), *b = malloc((size_t)n);
    for (int i = 0; i < n; i++) { a[i] = (char)('a' + rnd() % 4); b[i] = rnd() % 100 ? a[i] : (char)('a' + rnd() % 4); }
    double t0 = now(); int d = lev(a, n, b, n); double t1 = now() - t0;
    printf("  full DP        : distance %d, %7.1f ms, %.1f million cells/s\n", d, t1 * 1e3, (double)n * n / t1 / 1e6);
    for (int k = 50; k <= 400; k *= 2) { t0 = now(); int e = lev_banded(a, n, b, n, k); double tt = now() - t0; printf("  banded k = %-4d: distance %s%d, %7.2f ms (%.0fx faster)\n", k, e > k ? ">" : "", e > k ? k : e, tt * 1e3, t1 / tt); }
    free(a); free(b); return 0;
}
