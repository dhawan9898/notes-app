// What a suffix array + LCP array answers, and how construction scales on random, periodic and constant text.
#include "sa.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

int main(void) {
    const char *text = "banana bandana banana";
    int n = strlen(text); int *sa = malloc(n * sizeof(int)), *lcp = malloc(n * sizeof(int));
    build_sa((const unsigned char *)text, n, sa); build_lcp((const unsigned char *)text, n, sa, lcp);
    printf("text: \"%s\" (n = %d)\n", text, n);
    printf("sorted suffixes (first 6): ");
    for (int i = 0; i < 6; i++) printf("%d:\"%.*s\"%s", sa[i], n - sa[i] > 8 ? 8 : n - sa[i], text + sa[i], i < 5 ? ", " : "\n");
    int best = 0; for (int i = 1; i < n; i++) if (lcp[i] > lcp[best]) best = i;
    printf("longest repeated substring: \"%.*s\" (length %d) = the largest LCP value\n", lcp[best], text + sa[best], lcp[best]);
    long long distinct = (long long)n * (n + 1) / 2; for (int i = 1; i < n; i++) distinct -= lcp[i];
    printf("distinct substrings: %lld   (n(n+1)/2 minus the sum of the LCP array)\n", distinct);
    int first, c = sa_count((const unsigned char *)text, n, sa, (const unsigned char *)"ana", 3, &first);
    printf("occurrences of \"ana\": %d at positions", c); for (int i = 0; i < c; i++) printf(" %d", sa[first + i]); printf("\n");

    // longest common substring of two strings: concatenate with a separator and look at adjacent suffixes from different strings
    const char *x = "the quick brown fox", *y = "a quick brown dog";
    int nx = strlen(x), ny = strlen(y), m = nx + 1 + ny; unsigned char *z = malloc(m + 1);
    memcpy(z, x, nx); z[nx] = '\1'; memcpy(z + nx + 1, y, ny); z[m] = 0;                  // '\1' occurs in neither string
    int *sa2 = malloc(m * sizeof(int)), *lcp2 = malloc(m * sizeof(int)); build_sa(z, m, sa2); build_lcp(z, m, sa2, lcp2);
    int bl = 0, bp = 0; for (int i = 1; i < m; i++) if ((sa2[i] < nx) != (sa2[i - 1] < nx) && lcp2[i] > bl && sa2[i] != nx && sa2[i - 1] != nx) { bl = lcp2[i]; bp = sa2[i]; }
    printf("longest common substring of \"%s\" and \"%s\": \"%.*s\"\n", x, y, bl, z + bp);

    fm_t f; fm_build(&f, (const unsigned char *)text, n);
    printf("FM-index count of \"ana\" using only the BWT: %d;  BWT of the text (0 = terminator shown as $): ", fm_count(&f, (const unsigned char *)"ana", 3));
    for (int i = 0; i < f.n; i++) putchar(f.bwt[i] ? f.bwt[i] : '$');
    putchar('\n');

    printf("\n%-24s %10s %8s %12s\n", "text (1,000,000 bytes)", "build (s)", "rounds", "LCP (s)");
    int N = 1000000; unsigned char *big = malloc(N + 1); int *bsa = malloc(N * sizeof(int)), *blcp = malloc(N * sizeof(int));
    srand(3);
    for (int kind = 0; kind < 3; kind++) {
        for (int i = 0; i < N; i++) big[i] = kind == 0 ? 'a' + rand() % 4 : kind == 1 ? "abcdefgh"[i % 8] : 'a';
        double t0 = now(); int r = build_sa(big, N, bsa); double ts = now() - t0;
        t0 = now(); build_lcp(big, N, bsa, blcp); double tl = now() - t0;
        printf("%-24s %10.3f %8d %12.3f\n", kind == 0 ? "random, 4 letters" : kind == 1 ? "periodic (abcdefgh)*" : "all the same letter", ts, r, tl);
    }
    return 0;
}
