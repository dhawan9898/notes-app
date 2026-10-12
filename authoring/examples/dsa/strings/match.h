#ifndef MATCH_H
#define MATCH_H
#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Every matcher returns the number of (possibly overlapping) occurrences of p[0..m) in t[0..n). */

static inline size_t naive_count(const char *t, size_t n, const char *p, size_t m) {
    size_t c = 0;
    for (size_t i = 0; i + m <= n; i++) { size_t j = 0; while (j < m && t[i + j] == p[j]) j++; c += j == m; }
    return c;
}

/* ---- KMP: failure function fail[j] = length of the longest proper prefix of p[0..j) that is also a suffix ---- */
static inline size_t kmp_count(const char *t, size_t n, const char *p, size_t m) {
    size_t *fail = malloc((m + 1) * sizeof *fail), c = 0; fail[0] = 0; if (m) fail[1] = 0;
    for (size_t i = 1, k = 0; i < m; i++) {
        while (k && p[i] != p[k]) k = fail[k];
        if (p[i] == p[k]) k++;
        fail[i + 1] = k;
    }
    for (size_t i = 0, k = 0; i < n; i++) {                           /* the text pointer never moves backwards */
        while (k && (k == m || t[i] != p[k])) k = fail[k];
        if (t[i] == p[k]) k++;
        if (k == m) c++;
    }
    free(fail); return c;
}

/* ---- Z-algorithm on p + '\0' + t: z[i] = length of the longest common prefix of s and s[i..]; z[i] == m marks a match ---- */
static inline size_t z_count(const char *t, size_t n, const char *p, size_t m) {
    size_t len = m + 1 + n, c = 0; char *s = malloc(len); memcpy(s, p, m); s[m] = 0; memcpy(s + m + 1, t, n);
    size_t *z = malloc(len * sizeof *z); z[0] = len;
    for (size_t i = 1, l = 0, r = 0; i < len; i++) {
        z[i] = i < r ? (z[i - l] < r - i ? z[i - l] : r - i) : 0;
        while (i + z[i] < len && s[z[i]] == s[i + z[i]]) z[i]++;
        if (i + z[i] > r) { l = i; r = i + z[i]; }
        if (i > m && z[i] == m) c++;
    }
    free(s); free(z); return c;
}

/* ---- Rabin-Karp: rolling polynomial hash modulo 2^61-1; every hash hit is verified with memcmp ---- */
static inline uint64_t mulmod61(uint64_t a, uint64_t b) { __uint128_t x = (__uint128_t)a * b; uint64_t r = (uint64_t)(x & ((1ULL << 61) - 1)) + (uint64_t)(x >> 61); return r >= (1ULL << 61) - 1 ? r - ((1ULL << 61) - 1) : r; }
static inline size_t rk_count(const char *t, size_t n, const char *p, size_t m, size_t *false_hits) {
    const uint64_t MOD = (1ULL << 61) - 1, B = 911382323ULL; size_t c = 0; if (false_hits) *false_hits = 0;
    if (m == 0 || m > n) return 0;
    uint64_t hp = 0, ht = 0, pw = 1;
    for (size_t i = 0; i < m; i++) { hp = (mulmod61(hp, B) + (unsigned char)p[i]) % MOD; ht = (mulmod61(ht, B) + (unsigned char)t[i]) % MOD; if (i) pw = mulmod61(pw, B); }
    for (size_t i = 0;; i++) {
        if (hp == ht) { if (!memcmp(t + i, p, m)) c++; else if (false_hits) (*false_hits)++; }
        if (i + m >= n) break;
        ht = (ht + MOD - mulmod61(pw, (unsigned char)t[i])) % MOD;                           /* remove t[i] */
        ht = (mulmod61(ht, B) + (unsigned char)t[i + m]) % MOD;                              /* append t[i+m] */
    }
    return c;
}

/* ---- Boyer-Moore-Horspool: compare right to left, on mismatch shift by the bad-character table of the window's LAST byte ---- */
static inline size_t horspool_count(const char *t, size_t n, const char *p, size_t m) {
    size_t shift[256], c = 0; if (m == 0 || m > n) return 0;
    for (int i = 0; i < 256; i++) shift[i] = m;
    for (size_t i = 0; i + 1 < m; i++) shift[(unsigned char)p[i]] = m - 1 - i;
    for (size_t i = 0; i + m <= n; i += shift[(unsigned char)t[i + m - 1]]) {
        size_t j = m; while (j && t[i + j - 1] == p[j - 1]) j--;
        c += j == 0;
    }
    return c;
}

/* ---- libc: memmem (glibc uses a two-way / SIMD-assisted algorithm) ---- */
static inline size_t memmem_count(const char *t, size_t n, const char *p, size_t m) {
    size_t c = 0; const char *cur = t, *end = t + n;
    while (cur + m <= end && (cur = memmem(cur, (size_t)(end - cur), p, m))) { c++; cur++; }
    return c;
}
#endif
