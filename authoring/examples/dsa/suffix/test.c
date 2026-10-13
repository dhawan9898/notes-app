// Verify the suffix array, LCP, binary-search counting and FM-index against naive implementations on random and adversarial strings.
#include "sa.h"
#include <stdint.h>
#include <stdio.h>

static uint64_t rng = 0x9E3779B97F4A7C15ULL;
static unsigned long long rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

static const unsigned char *g_s; static int g_n;
static int cmp_suffix(const void *a, const void *b) {                    // naive: compare whole suffixes
    int x = *(const int *)a, y = *(const int *)b;
    int lx = g_n - x, ly = g_n - y, c = memcmp(g_s + x, g_s + y, lx < ly ? lx : ly);
    return c ? c : lx - ly;
}
static int naive_count(const unsigned char *s, int n, const unsigned char *p, int m) {
    int c = 0; for (int i = 0; i + m <= n; i++) c += !memcmp(s + i, p, m); return c;
}

int main(void) {
    long checks = 0;
    for (int trial = 0; trial < 4000; trial++) {
        int n = 1 + rnd() % 60, alpha = 1 + rnd() % 4;                    // tiny alphabets give many repeats
        unsigned char *s = malloc(n + 1);
        for (int i = 0; i < n; i++) s[i] = 'a' + rnd() % alpha;
        if (trial % 7 == 0) for (int i = 0; i < n; i++) s[i] = 'a';       // all equal: the worst case for doubling
        s[n] = 0;
        int *sa = malloc(n * sizeof(int)), *want = malloc(n * sizeof(int)), *lcp = malloc(n * sizeof(int));
        build_sa(s, n, sa); build_lcp(s, n, sa, lcp);
        for (int i = 0; i < n; i++) want[i] = i;
        g_s = s; g_n = n; qsort(want, n, sizeof(int), cmp_suffix);
        if (memcmp(sa, want, n * sizeof(int))) { printf("SA mismatch: %s\n", s); return 1; }
        for (int i = 1; i < n; i++) {                                     // naive LCP
            int a = sa[i - 1], b = sa[i], h = 0; while (a + h < n && b + h < n && s[a + h] == s[b + h]) h++;
            if (lcp[i] != h) { printf("LCP mismatch: %s\n", s); return 1; }
        }
        fm_t f; fm_build(&f, s, n);
        for (int q = 0; q < 30; q++) {
            int m = 1 + rnd() % 5; unsigned char p[8]; for (int i = 0; i < m; i++) p[i] = 'a' + rnd() % alpha;
            int w = naive_count(s, n, p, m);
            if (sa_count(s, n, sa, p, m, NULL) != w || fm_count(&f, p, m) != w) { printf("count mismatch on %s\n", s); return 1; }
            checks++;
        }
        free(f.bwt); free(f.occ); free(s); free(sa); free(want); free(lcp);
    }
    printf("4000 strings: suffix array, LCP array, SA binary-search counts and FM-index counts all match naive code (%ld pattern queries)\n", checks);
    return 0;
}
