#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "match.h"
#include "../../c/perf/timing.h"

/* Aho-Corasick: a trie of all patterns plus failure links, turned into a full automaton so that scanning the text costs exactly one
 * table lookup per byte, no matter how many patterns there are.  Alphabet: 'a'..'z'; other bytes reset the automaton. */
#define SIGMA 26
typedef struct { int (*go)[SIGMA]; int *fail; int *out; int n, cap; } AC;   /* out[v] = number of patterns that end at v or at a suffix of v */

static int ac_new_node(AC *a) {
    if (a->n == a->cap) { a->cap *= 2; a->go = realloc(a->go, (size_t)a->cap * sizeof *a->go); a->fail = realloc(a->fail, (size_t)a->cap * sizeof(int)); a->out = realloc(a->out, (size_t)a->cap * sizeof(int)); }
    for (int c = 0; c < SIGMA; c++) a->go[a->n][c] = -1;
    a->fail[a->n] = 0; a->out[a->n] = 0; return a->n++;
}
static void ac_init(AC *a) { a->cap = 1024; a->n = 0; a->go = malloc((size_t)a->cap * sizeof *a->go); a->fail = malloc((size_t)a->cap * sizeof(int)); a->out = malloc((size_t)a->cap * sizeof(int)); ac_new_node(a); }
static void ac_add(AC *a, const char *p) {
    int v = 0;
    for (; *p; p++) { int c = *p - 'a'; if (a->go[v][c] < 0) { int u = ac_new_node(a); a->go[v][c] = u; } v = a->go[v][c]; }
    a->out[v]++;
}
static void ac_build(AC *a) {                                         /* BFS: fail[v] = longest proper suffix of v that is also a trie node */
    int *q = malloc((size_t)a->n * sizeof(int)), h = 0, t = 0;
    for (int c = 0; c < SIGMA; c++) { int u = a->go[0][c]; if (u < 0) a->go[0][c] = 0; else { a->fail[u] = 0; q[t++] = u; } }
    while (h < t) {
        int v = q[h++]; a->out[v] += a->out[a->fail[v]];              /* inherit matches of the failure state (dictionary-suffix links folded into a count) */
        for (int c = 0; c < SIGMA; c++) {
            int u = a->go[v][c];
            if (u < 0) a->go[v][c] = a->go[a->fail[v]][c];            /* missing edge = follow the failure chain once, at build time */
            else { a->fail[u] = a->go[a->fail[v]][c]; q[t++] = u; }
        }
    }
    free(q);
}
static size_t ac_count(const AC *a, const char *t, size_t n) {       /* total number of (pattern, position) matches */
    size_t total = 0; int v = 0;
    for (size_t i = 0; i < n; i++) { int c = t[i] - 'a'; v = (unsigned)c < SIGMA ? a->go[v][c] : 0; total += (size_t)a->out[v]; }
    return total;
}

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    /* correctness: duplicates, nested patterns ("he" inside "she"), overlaps, against summing naive counts */
    for (int round = 0; round < 20000; round++) {
        int k = 1 + (int)(rnd() % 6); char pat[6][8]; AC a; ac_init(&a);
        for (int i = 0; i < k; i++) { int len = 1 + (int)(rnd() % 4); for (int j = 0; j < len; j++) pat[i][j] = (char)('a' + rnd() % 3); pat[i][len] = 0; ac_add(&a, pat[i]); }
        ac_build(&a);
        char t[48]; int n = (int)(rnd() % 40); for (int i = 0; i < n; i++) t[i] = (char)('a' + rnd() % 3);
        size_t want = 0; for (int i = 0; i < k; i++) want += naive_count(t, (size_t)n, pat[i], strlen(pat[i]));
        assert(ac_count(&a, t, (size_t)n) == want); free(a.go); free(a.fail); free(a.out);
    }
    printf("20000 random pattern sets (with duplicates and nested patterns) over a 3-letter alphabet: Aho-Corasick = sum of naive counts\n\n");
    { AC a; ac_init(&a); const char *w[] = { "he", "she", "his", "hers" }; for (int i = 0; i < 4; i++) ac_add(&a, w[i]); ac_build(&a);
      printf("classic example: patterns {he, she, his, hers} in \"ushers\": %zu matches (she, he, hers)\n\n", ac_count(&a, "ushers", 6)); free(a.go); free(a.fail); free(a.out); }

    size_t n = 32u << 20; char *t = malloc(n); for (size_t i = 0; i < n; i++) t[i] = (char)('a' + rnd() % 26);
    printf("scan 32 MB of random lowercase text for K random patterns of length 6-10; seconds, and automaton size\n");
    printf("%8s | %14s %12s | %24s\n", "K", "Aho-Corasick", "build", "K separate memmem scans");
    int Ks[] = { 1, 10, 100, 1000, 10000 };
    for (int ki = 0; ki < 5; ki++) {
        int K = Ks[ki]; char **pats = malloc((size_t)K * sizeof *pats); AC a; ac_init(&a);
        double t0 = now();
        for (int i = 0; i < K; i++) { int len = 6 + (int)(rnd() % 5); pats[i] = malloc((size_t)len + 1); for (int j = 0; j < len; j++) pats[i][j] = (char)('a' + rnd() % 26); pats[i][len] = 0; ac_add(&a, pats[i]); }
        ac_build(&a); double tb = now() - t0;
        t0 = now(); size_t total = ac_count(&a, t, n); double ta = now() - t0;
        double tm = -1; size_t chk = 0;
        if (K <= 100) { t0 = now(); for (int i = 0; i < K; i++) chk += memmem_count(t, n, pats[i], strlen(pats[i])); tm = now() - t0; assert(chk == total); }
        if (tm >= 0) printf("%8d | %12.3f s %9.1f ms | %20.3f s   (%d nodes, %.1f MB)\n", K, ta, tb * 1e3, tm, a.n, a.n * sizeof(int) * (SIGMA + 2) / 1048576.0);
        else printf("%8d | %12.3f s %9.1f ms | %24s   (%d nodes, %.1f MB)\n", K, ta, tb * 1e3, "(would be ~K x one scan)", a.n, a.n * sizeof(int) * (SIGMA + 2) / 1048576.0);
        for (int i = 0; i < K; i++) free(pats[i]);
        free(pats); free(a.go); free(a.fail); free(a.out);
    }
    free(t); return 0;
}
