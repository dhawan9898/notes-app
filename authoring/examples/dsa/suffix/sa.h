// Suffix array by prefix doubling with radix sort (O(n log n)), Kasai's LCP in O(n), pattern search, and a Burrows-Wheeler FM-index.
#include <stdlib.h>
#include <string.h>

// Suffix array of s[0..n) (bytes). sa[i] = start of the i-th smallest suffix. Returns the number of doubling rounds used.
static int build_sa(const unsigned char *s, int n, int *sa) {
    int m = n > 256 ? n : 256, rounds = 0;
    int *rank = malloc(n * sizeof(int)), *tmp = malloc(n * sizeof(int)), *sa2 = malloc(n * sizeof(int)), *cnt = malloc((m + 1) * sizeof(int));
    // round 0: sort by first byte
    memset(cnt, 0, (m + 1) * sizeof(int));
    for (int i = 0; i < n; i++) cnt[s[i]]++;
    for (int i = 1; i < 256; i++) cnt[i] += cnt[i - 1];
    for (int i = n - 1; i >= 0; i--) sa[--cnt[s[i]]] = i;
    rank[sa[0]] = 0;
    for (int i = 1; i < n; i++) rank[sa[i]] = rank[sa[i - 1]] + (s[sa[i]] != s[sa[i - 1]]);
    for (int k = 1; rank[sa[n - 1]] < n - 1; k <<= 1, rounds++) {
        // sort by (rank[i], rank[i+k]): the second key is already in order if we list suffixes by their second half first
        int p = 0;
        for (int i = n - k; i < n; i++) sa2[p++] = i;                    // suffixes with no second half sort first (empty key)
        for (int i = 0; i < n; i++) if (sa[i] >= k) sa2[p++] = sa[i] - k;
        memset(cnt, 0, (m + 1) * sizeof(int));
        int classes = rank[sa[n - 1]] + 1;
        for (int i = 0; i < n; i++) cnt[rank[i]]++;
        for (int i = 1; i < classes; i++) cnt[i] += cnt[i - 1];
        for (int i = n - 1; i >= 0; i--) sa[--cnt[rank[sa2[i]]]] = sa2[i];      // stable counting sort by the first key
        tmp[sa[0]] = 0;
        for (int i = 1; i < n; i++) {
            int a = sa[i - 1], b = sa[i];
            int same = rank[a] == rank[b] && (a + k < n ? rank[a + k] : -1) == (b + k < n ? rank[b + k] : -1);
            tmp[b] = tmp[a] + !same;
        }
        memcpy(rank, tmp, n * sizeof(int));
    }
    free(rank); free(tmp); free(sa2); free(cnt);
    return rounds;
}

// Kasai: lcp[i] = length of the longest common prefix of suffixes sa[i-1] and sa[i] (lcp[0] = 0), in O(n).
static void build_lcp(const unsigned char *s, int n, const int *sa, int *lcp) {
    int *rank = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) rank[sa[i]] = i;
    for (int i = 0, h = 0; i < n; i++) {
        if (rank[i] == 0) { h = 0; lcp[0] = 0; continue; }
        int j = sa[rank[i] - 1];
        while (i + h < n && j + h < n && s[i + h] == s[j + h]) h++;
        lcp[rank[i]] = h;
        if (h > 0) h--;                                   // the key fact: moving to i+1 loses at most one character of the match
    }
    free(rank);
}

// Occurrences of p (length m) in s: binary search for the range of suffixes that start with p. Returns the count; *first = lowest index in sa.
static int cmp_prefix(const unsigned char *s, int n, int suf, const unsigned char *p, int m) {
    int len = n - suf < m ? n - suf : m;
    int c = memcmp(s + suf, p, len);
    if (c) return c;
    return len < m ? -1 : 0;                              // suffix is a proper prefix of p: smaller
}
static int sa_count(const unsigned char *s, int n, const int *sa, const unsigned char *p, int m, int *first) {
    int lo = 0, hi = n;
    while (lo < hi) { int mid = (lo + hi) / 2; if (cmp_prefix(s, n, sa[mid], p, m) < 0) lo = mid + 1; else hi = mid; }
    int left = lo; hi = n;
    while (lo < hi) { int mid = (lo + hi) / 2; if (cmp_prefix(s, n, sa[mid], p, m) <= 0) lo = mid + 1; else hi = mid; }
    if (first) *first = left;
    return lo - left;
}

/* ---- Burrows-Wheeler transform and FM-index (backward search) ---- */
typedef struct { int n; unsigned char *bwt; int C[257]; int *occ; int step; } fm_t;   // occ: checkpoints every 'step' positions for each byte

// text must NOT contain byte 0; a terminator 0 is appended internally.
static void fm_build(fm_t *f, const unsigned char *text, int len) {
    int n = len + 1; unsigned char *s = malloc(n); memcpy(s, text, len); s[len] = 0;
    int *sa = malloc(n * sizeof(int)); build_sa(s, n, sa);
    f->n = n; f->bwt = malloc(n);
    for (int i = 0; i < n; i++) f->bwt[i] = sa[i] ? s[sa[i] - 1] : s[n - 1];          // the character that precedes each sorted suffix
    memset(f->C, 0, sizeof f->C);
    for (int i = 0; i < n; i++) f->C[s[i] + 1]++;
    for (int c = 1; c <= 256; c++) f->C[c] += f->C[c - 1];                            // C[c] = number of characters smaller than c
    f->step = 64; int blocks = n / f->step + 2;
    f->occ = calloc((size_t)blocks * 256, sizeof(int));
    int run[256] = {0};
    for (int i = 0; i < n; i++) { if (i % f->step == 0) memcpy(f->occ + (size_t)(i / f->step) * 256, run, sizeof run); run[f->bwt[i]]++; }
    free(s); free(sa);
}
static int fm_occ(const fm_t *f, unsigned char c, int i) {                            // occurrences of c in bwt[0..i)
    int b = i / f->step, r = f->occ[(size_t)b * 256 + c];
    for (int j = b * f->step; j < i; j++) r += f->bwt[j] == c;
    return r;
}
// number of occurrences of p in the text, using only the BWT and the small tables: the text itself is not needed
static int fm_count(const fm_t *f, const unsigned char *p, int m) {
    int lo = 0, hi = f->n;
    for (int i = m - 1; i >= 0 && lo < hi; i--) {
        unsigned char c = p[i];
        lo = f->C[c] + fm_occ(f, c, lo);
        hi = f->C[c] + fm_occ(f, c, hi);
    }
    return hi - lo;
}
