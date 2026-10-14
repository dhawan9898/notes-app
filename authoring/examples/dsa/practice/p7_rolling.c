#include <stdio.h>
#include <string.h>

#define BASE 256
#define MOD 1000003                                              /* a small prime so collisions are easy to provoke */

/* Rabin-Karp: compare a rolling hash of each text window with the pattern's hash; verify real matches character by character. */
int main(void) {
    const char *t = "the cat sat on the mat with the other cat", *p = "the";
    int n = (int)strlen(t), m = (int)strlen(p);
    long ph = 0, th = 0, high = 1;
    for (int i = 0; i < m - 1; i++) high = high * BASE % MOD;    /* BASE^(m-1), the weight of the leading character */
    for (int i = 0; i < m; i++) { ph = (ph * BASE + p[i]) % MOD; th = (th * BASE + t[i]) % MOD; }
    int hash_hits = 0, real = 0;
    printf("matches of \"%s\" at:", p);
    for (int i = 0; i + m <= n; i++) {
        if (th == ph) {
            hash_hits++;
            if (strncmp(t + i, p, (size_t)m) == 0) { real++; printf(" %d", i); }     /* a hash match is only a candidate */
        }
        if (i + m < n) th = ((th - t[i] * high % MOD + MOD) * BASE + t[i + m]) % MOD;  /* drop t[i], add t[i + m]: O(1) per step */
    }
    printf("\nwindows with equal hash: %d, confirmed matches: %d\n", hash_hits, real);
    return 0;
}
