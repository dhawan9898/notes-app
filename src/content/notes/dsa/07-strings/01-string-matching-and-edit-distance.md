---
title: String Matching and Edit Distance
track: dsa
module: Strings
moduleOrder: 7
order: 1
level: advanced
summary: "Naive, KMP, Z, Rabin-Karp, Horspool, glibc memmem and Aho-Corasick on 32 MB of text (measured, including the adversarial case), then Levenshtein distance, LCS, banded DP, and the byte-versus-code-point trap."
prerequisites:
  - Recursion, Divide and Conquer, and Loop Invariants
  - "Hash Tables: Open Addressing, Deletion, Cuckoo and Hash Flooding"
sources:
  - label: "Knuth, Morris, Pratt — Fast Pattern Matching in Strings (SIAM J. Computing 6(2), 1977)"
  - label: "Boyer & Moore — A Fast String Searching Algorithm (CACM 20(10), 1977)"
  - label: "Horspool — Practical Fast Searching in Strings (Software, Practice and Experience 10(6), 1980)"
  - label: "Karp & Rabin — Efficient Randomized Pattern-Matching Algorithms (IBM J. Research and Development 31(2), 1987)"
  - label: "Aho & Corasick — Efficient String Matching, an Aid to Bibliographic Search (CACM 18(6), 1975)"
  - label: "Crochemore & Perrin — Two-Way String-Matching (J. ACM 38(3), 1991)"
  - label: "Wagner & Fischer — The String-to-String Correction Problem (J. ACM 21(1), 1974)"
  - label: "Myers — An O(ND) Difference Algorithm and Its Variations (Algorithmica 1, 1986)"
  - label: "Navarro — A Guided Tour to Approximate String Matching (ACM Computing Surveys 33(1), 2001)"
  - label: "Gusfield — Algorithms on Strings, Trees and Sequences (Cambridge University Press, 1997)"
  - label: "Unicode Standard Annex #15 — Unicode Normalization Forms"
    url: https://www.unicode.org/reports/tr15/
---

Searching text is one of the oldest and most heavily optimised problems in computing: `grep`, `strstr`, editors, intrusion-detection systems, DNA aligners, spam filters and version-control diffs all depend on it. The naive method is fine until it isn't, so this note works through the classic algorithms, measures them on real data (including the input that breaks the naive scan), and then moves from *exact* matching to *approximate* matching with edit distance.

## Exact matching: one pattern in one text

Let the text have length `n` and the pattern length `m`. Matching algorithms differ in what they remember about the pattern.

| Algorithm | Preprocessing | Search | Idea |
|---|---|---|---|
| Naive | none | `O(nm)` worst case | slide the pattern one position at a time |
| **KMP** | `O(m)` | `O(n)` guaranteed | on mismatch, jump using the pattern's own prefix structure; text pointer never moves back |
| **Z-algorithm** | `O(n + m)` | `O(n + m)` | compute the longest common prefix of every suffix with the pattern-and-text concatenation |
| **Rabin–Karp** | `O(m)` | `O(n)` expected | compare rolling hashes; verify on hit |
| **Boyer–Moore–Horspool** | `O(m + σ)` | `O(n/m)` best, `O(nm)` worst | compare from the right; skip ahead by the bad-character table |
| **Two-Way** (Crochemore–Perrin) | `O(m)` | `O(n)`, constant extra space | critical factorisation of the pattern; used in glibc |
| **Aho–Corasick** | `O(Σ m_i)` | `O(n + matches)` | trie + failure links for *many* patterns at once |

```c
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
```

### KMP in one paragraph

After matching `k` characters and then failing, the naive scan restarts from scratch one position later, though it already *knows* the text equals the pattern's first `k` characters. KMP precomputes `fail[k]`: the length of the longest proper prefix of `p[0..k)` that is also a suffix of it. On a mismatch at pattern position `k` the matcher falls back to `fail[k]` — the longest prefix that can still be a valid partial match — without re-reading text. Each text character is examined a bounded number of times: the pointer `k` rises at most once per text character and every fallback strictly lowers it, so the total work is at most `2n` steps. The Z-algorithm achieves the same bound with a different table (`z[i]` = length of the common prefix of the string and its suffix starting at `i`), and is often easier to get right.

### Rabin–Karp and rolling hashes

Rabin–Karp treats a window of `m` characters as a number in some base modulo a prime. Sliding the window by one position updates the hash in `O(1)`: subtract the leaving character's contribution, multiply by the base, add the entering character. Matching windows have equal hashes; equal hashes *may* be collisions, so every hit is verified with `memcmp`. The code uses the Mersenne prime `2^61 − 1` (cheap reduction with 128-bit multiplication), making false positives vanishingly rare for non-adversarial data. Its virtue is generality, not speed: it extends to **multiple patterns of the same length** (put hashes in a set), to 2-D matching, to near-duplicate detection and plagiarism checking (hash all k-grams), and to content-defined chunking in deduplicating backups and **rsync** (a weak rolling checksum finds candidate blocks, a strong hash confirms).

### Boyer–Moore–Horspool

Compare the pattern against the text window *right to left*. On a mismatch, look at the text character aligned with the pattern's last position: if it does not occur in the pattern, the whole window can move forward by `m`; otherwise align its last occurrence in the pattern. For large alphabets and long patterns most windows are skipped after a single comparison, so the algorithm reads far fewer than `n` characters — "sublinear" in practice. Full Boyer–Moore adds a *good-suffix* rule for better worst-case behavior; Horspool drops it for simplicity.

### Verification and measurement

The test cross-checks every algorithm against the naive scan on 80,000 random text/pattern pairs over alphabets of 1–4 letters (small alphabets create heavy overlaps and repetition — the cases where fallback logic breaks).

```c
#include <assert.h>
#include <stdio.h>
#include "match.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    long checks = 0;
    for (int alpha = 1; alpha <= 4; alpha++) for (int round = 0; round < 20000; round++) {
        size_t n = rnd() % 60, m = 1 + rnd() % 8; char t[64], p[16];
        for (size_t i = 0; i < n; i++) t[i] = (char)('a' + rnd() % (unsigned)alpha);
        for (size_t i = 0; i < m; i++) p[i] = (char)('a' + rnd() % (unsigned)alpha);
        size_t want = naive_count(t, n, p, m), fh;
        assert(kmp_count(t, n, p, m) == want); assert(z_count(t, n, p, m) == want); assert(rk_count(t, n, p, m, &fh) == want);
        assert(horspool_count(t, n, p, m) == want); assert(memmem_count(t, n, p, m) == want); checks++;
    }
    printf("%ld random (text, pattern) pairs over alphabets of size 1-4: KMP, Z, Rabin-Karp, Horspool and memmem agree with the naive scan (overlaps included)\n", checks);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined test_match.c -o test_match && ./test_match
80000 random (text, pattern) pairs over alphabets of size 1-4: KMP, Z, Rabin-Karp, Horspool and memmem agree with the naive scan (overlaps included)
```

The benchmark scans 32 MB of random DNA (alphabet 4) and of English-like text (27 symbols with realistic letter frequencies), with patterns taken from the text, then the adversarial input for the naive scan.

```c
#include <stdio.h>
#include "match.h"
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

typedef size_t (*Fn)(const char *, size_t, const char *, size_t);
static size_t rk_wrap(const char *t, size_t n, const char *p, size_t m) { return rk_count(t, n, p, m, NULL); }

static void row(const char *label, const char *t, size_t n, const char *p, size_t m) {
    static const char *names[] = { "naive", "KMP", "Z", "Rabin-Karp", "Horspool", "memmem" };
    Fn fns[] = { naive_count, kmp_count, z_count, rk_wrap, horspool_count, memmem_count };
    printf("%-22s m=%-4zu", label, m);
    size_t ref = 0;
    for (int i = 0; i < 6; i++) {
        double t0 = now(); size_t c = fns[i](t, n, p, m); double dt = now() - t0;
        if (i == 0) ref = c; else if (c != ref) { printf("  MISMATCH %s\n", names[i]); return; }
        printf(" %10.2f", dt / (double)n * 1e9);
    }
    printf("   (%zu matches)\n", ref);
}

int main(void) {
    size_t n = 32u << 20; char *dna = malloc(n), *eng = malloc(n);
    static const char *freq = "eeeeeeeeeeeettttttttttaaaaaaaaooooooooiiiiiiiinnnnnnnsssssshhhhhhrrrrrrddddllllcccuuummmwwffggyyppbbvkjxqz        ";
    size_t fl = strlen(freq);
    for (size_t i = 0; i < n; i++) { dna[i] = "ACGT"[rnd() & 3]; eng[i] = freq[rnd() % fl]; }
    printf("ns per text byte; text = 32 MB.  columns: naive, KMP, Z, Rabin-Karp, Horspool, memmem\n");
    printf("%-22s %-6s %10s %10s %10s %10s %10s %10s\n", "text", "", "naive", "KMP", "Z", "R-K", "Horspool", "memmem");
    size_t ms[] = { 4, 16, 64 };
    for (int k = 0; k < 3; k++) row("DNA (alphabet 4)", dna, n, dna + 1000003, ms[k]);
    for (int k = 0; k < 3; k++) row("English-like (27 symbols)", eng, n, eng + 1000003, ms[k]);

    printf("\nworst case for the naive scan: text = a^4M, pattern = a^(m-1) b (never matches)\n");
    size_t wn = 4u << 20; char *wt = malloc(wn); memset(wt, 'a', wn); char wp[600];
    for (int k = 0; k < 3; k++) { size_t m = k == 0 ? 8 : k == 1 ? 64 : 512; memset(wp, 'a', m - 1); wp[m - 1] = 'b'; row("a...a vs a..ab", wt, wn, wp, m); }
    free(dna); free(eng); free(wt); return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra bench_match.c -o bench_match && ./bench_match
ns per text byte; text = 32 MB.  columns: naive, KMP, Z, Rabin-Karp, Horspool, memmem
text                               naive        KMP          Z        R-K   Horspool     memmem
DNA (alphabet 4)       m=4          6.35       5.68       9.98      19.76       4.38       1.93   (131559 matches)
DNA (alphabet 4)       m=16         4.56       3.51       5.91       9.86       1.45       0.69   (1 matches)
DNA (alphabet 4)       m=64         3.23       2.73       5.23       9.82       1.45       0.27   (1 matches)
English-like (27 symbols) m=4          1.32       0.90       3.12       9.85       1.13       0.30   (460 matches)
English-like (27 symbols) m=16         1.39       0.90       3.42       9.83       0.37       0.12   (1 matches)
English-like (27 symbols) m=64         1.33       0.90       3.16       9.81       0.18       0.10   (1 matches)

worst case for the naive scan: text = a^4M, pattern = a^(m-1) b (never matches)
a...a vs a..ab         m=8          3.61       1.65       4.18       9.84       3.29       3.85   (0 matches)
a...a vs a..ab         m=64        29.49       1.65       3.97       9.83       3.29       3.89   (0 matches)
a...a vs a..ab         m=512      154.36       1.65       3.98       9.82       3.29       3.02   (0 matches)
```

What to take from the table (values are nanoseconds per text byte; ratios, not absolutes, are portable):

- **On random text the naive scan is not slow.** Mismatches usually occur on the first or second comparison, so it is nearly linear — which is why it survives in so many libraries for short patterns.
- **Horspool beats KMP when the alphabet is large and the pattern long** (English, `m = 64`: several times faster) because it skips; on a 4-letter alphabet with short patterns there is little to skip, so the advantage shrinks.
- **Rabin–Karp and Z are slower per byte** than the others here: the hash update does a modular multiplication per character and Z builds a large auxiliary array. Their value is generality.
- **`memmem` is the fastest by a wide margin** — typically 2–5× faster than the best hand-written algorithm here and up to ~20× faster than the naive scan. The C library uses vectorised code (SSE/AVX2 scanning for a rare byte or a two-byte anchor) plus the Two-Way algorithm for long needles, so *do not hand-roll single-pattern exact search in production C*; use `memmem`/`strstr`. (Note `memmem` is a GNU/BSD extension, not ISO C, hence `_GNU_SOURCE`.)
- **The adversarial row is the lesson for the naive scan**: on `aaaa…a` against `aaa…ab` it re-examines nearly the whole pattern at every position and its cost per byte grows linearly with `m` (from a few ns at `m = 8` to over a hundred at `m = 512`), while KMP stays flat at roughly 2 ns per byte and Horspool at about 3–4 ns, independent of `m`. Input like this arises in real data (long runs, repetitive logs) and in attacks; any matcher fed untrusted input needs a worst-case bound.

## Many patterns at once: Aho–Corasick

Searching for `K` patterns by running a single-pattern search `K` times costs `K` passes over the text. Aho–Corasick builds a trie of all patterns, adds a **failure link** to each node (the longest proper suffix of the node's string that is also a trie node — KMP's fallback generalised to a tree), and then completes every missing transition by following failure links at build time. The result is a deterministic finite automaton: scanning the text is one table lookup per byte, regardless of `K`, and a node's *output count* includes matches that end at its failure-link ancestors (so `she` also reports `he`).

It is the algorithm behind `fgrep`/`grep -F` with many patterns, antivirus and intrusion-detection signature matching (Snort, Suricata; Hyperscan generalises the idea), profanity and keyword filters, and bioinformatics motif search.

```c
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
```

```text
$ gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined ac.c -o ac_asan && ./ac_asan | head -3 && gcc -std=gnu11 -O2 -Wall -Wextra ac.c -o ac && ./ac | tail -7
20000 random pattern sets (with duplicates and nested patterns) over a 3-letter alphabet: Aho-Corasick = sum of naive counts

classic example: patterns {he, she, his, hers} in "ushers": 3 matches (she, he, hers)
scan 32 MB of random lowercase text for K random patterns of length 6-10; seconds, and automaton size
       K |   Aho-Corasick        build |  K separate memmem scans
       1 |        0.111 s       0.0 ms |                0.005 s   (9 nodes, 0.0 MB)
      10 |        0.111 s       0.0 ms |                0.050 s   (81 nodes, 0.0 MB)
     100 |        0.113 s       0.1 ms |                0.504 s   (730 nodes, 0.1 MB)
    1000 |        0.197 s       0.9 ms | (would be ~K x one scan)   (6492 nodes, 0.7 MB)
   10000 |        0.332 s       7.3 ms | (would be ~K x one scan)   (58374 nodes, 6.2 MB)
```

The first lines (sanitizer build) check 20,000 random pattern sets — with duplicates and nested patterns — against the sum of naive counts. The table (optimized build) shows the automaton scanning 32 MB in about the same time (within a factor of 1.5) whether it holds 1 pattern or 1000 — its cost is per byte, not per pattern — and then growing only gently at 10,000 patterns because the 58,000-node automaton (6 MB) no longer fits in the CPU's fast caches. Running `memmem` once per pattern wins for a single pattern (about 20× faster here) but loses once there are a few dozen: the crossover is between 10 and 100 patterns, and at 10,000 patterns the repeated scans would take on the order of a minute against a third of a second. The automaton's size is the price: dense tables cost `nodes × alphabet` words, which is why production implementations compress transitions or use sparse representations for large alphabets.

## Suffix structures (overview)

For many queries against one fixed text, build an *index* instead. A **suffix array** lists the starting positions of all suffixes in sorted order; any pattern occurrence is a contiguous range, found with binary search in `O(m log n)`. It can be built in `O(n)` (SA-IS; Nong, Zhang & Chan) and stores `4n` bytes. A **suffix tree** or **suffix automaton** answers more queries (longest repeated substring, longest common substring, counting distinct substrings) in linear space but with large constants. Compressed forms — the **FM-index** built on the Burrows–Wheeler transform, the same transform bzip2 uses — are how DNA aligners (BWA, Bowtie) search a human genome in memory of a few GB.

## Approximate matching: edit distance

Often the strings are merely *close*: spell checking, fuzzy search, DNA alignment, deduplication, diff. The **Levenshtein distance** between `a` and `b` is the minimum number of single-character insertions, deletions and substitutions turning one into the other. The classic dynamic program (Wagner–Fischer) fills a table where `D[i][j]` is the distance between the first `i` characters of `a` and the first `j` of `b`:

`D[i][j] = min( D[i−1][j−1] + [a_i ≠ b_j],  D[i−1][j] + 1,  D[i][j−1] + 1 )`

with `D[i][0] = i` and `D[0][j] = j`. It takes `O(nm)` time; since each row depends only on the previous one, the table need not be stored — two rows give `O(min(n, m))` space (recovering the alignment itself needs Hirschberg's divide-and-conquer to stay linear in space). The **longest common subsequence** (LCS) is the same table with different costs (match = +1, no substitution) and is the basis of `diff`: the characters not in the LCS are the changes. (Production `diff` and `git diff` use Myers' `O(ND)` algorithm, fast when the two files differ little; the dynamic-programming note covers the general technique.)

**Bounded distance.** If you only care whether two strings are within `k` edits — "did you mean…?" — only cells within `k` of the diagonal can matter, because each step off the diagonal costs an edit. The *banded* DP does `O(nk)` work. Bit-parallel versions (Myers 1999) pack a column into machine words for another factor of ~64.

```c
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
```

```text
$ gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined edit.c -o edit_asan && ./edit_asan | head -5 && gcc -std=gnu11 -O2 -Wall -Wextra edit.c -o edit && ./edit | tail -6
lev("kitten", "sitting") = 3, lev("flaw", "lawn") = 2, lcs("AGGTAB", "GXTXAYB") = 4
20000 random pairs: DP == exponential recursion, symmetric, banded == min(d, k+1) for k = 0..8, indel distance n+m-2*LCS >= edit distance

UTF-8: "naive" vs "naïve": 2 byte edits (the byte DP sees two substitutions/insertions), 1 code-point edit

edit distance of two random 20,000-character strings that differ by about 1% of positions
  full DP        : distance 155,   514.6 ms, 777.3 million cells/s
  banded k = 50  : distance >50,    9.92 ms (52x faster)
  banded k = 100 : distance >100,   19.84 ms (26x faster)
  banded k = 200 : distance 155,   39.47 ms (13x faster)
  banded k = 400 : distance 155,   78.85 ms (7x faster)
```

The sanitizer build runs the correctness checks: 20,000 random pairs where the DP equals an exponential brute-force recursion, is symmetric, and the banded version equals `min(d, k+1)` for every `k`; the classic examples (`kitten`→`sitting` = 3) agree with the textbook. The optimized build shows the banded DP, on two 20,000-character strings that differ at about 1 % of positions: with a band wide enough to contain the true distance it returns the same answer as the full table, roughly 8–60× faster here depending on the band width, and with a band too narrow it correctly reports "more than `k`" even faster.

### Bytes are not characters

The UTF-8 line is the most common real-world bug in this area. `"naïve"` stores `ï` as two bytes (`C3 AF`), so an edit distance computed on bytes says the words differ by **2** edits, while on code points it is **1**. And code points are still not what users see: `é` can be one code point or `e` plus a combining accent, so two strings that *look* identical can have a non-zero distance until both are normalised (Unicode Normalization Forms, UAX #15). Case-insensitive comparison has the same problem (`ß`, Turkish dotless `i`). For anything user-facing, decode UTF-8 to code points (or grapheme clusters) before applying these algorithms, normalise first, and be explicit about locale.

## Pitfalls

- **`strlen` in the loop condition** (`for (i = 0; i < strlen(s); i++)`) is quadratic if the compiler cannot hoist it.
- **Embedded NUL bytes**: `strstr`/`strcmp` stop at them; binary data needs `memmem`/`memcmp` with explicit lengths.
- **Signed `char` as an array index**: bytes ≥ 0x80 become negative indices; cast to `unsigned char` before indexing tables (the code does).
- **Overlapping matches**: decide whether `aa` occurs once or twice in `aaa` — the algorithms here report overlaps; `strstr`-in-a-loop that advances by `m` does not.
- **Rolling-hash weaknesses**: a small modulus or fixed base lets adversaries craft collisions (and anti-hash tests exist for 2^64 overflow hashes); verify hits, use a large prime and a randomised base.
- **Quadratic memory** for full edit-distance tables on large inputs (20,000 × 20,000 × 4 bytes = 1.6 GB): use two rows or a band.
- **Treating distance as similarity**: a distance of 3 means very different things for 4-letter and 400-letter strings; normalise by length when comparing.

## Key takeaways

1. The naive scan is `O(nm)` in the worst case but fast on random text; KMP and the Z-algorithm guarantee `O(n + m)`; Horspool is sublinear on typical text.
2. For a single pattern in production C, call `memmem`/`strstr` — vectorised and Two-Way-based, they beat the textbook algorithms by large factors.
3. Aho–Corasick scans once for any number of patterns at one lookup per byte; it overtakes repeated single-pattern searches at a few dozen patterns.
4. Levenshtein distance is an `O(nm)` DP that needs only two rows of memory; if you only need to know whether the distance is at most `k`, the banded DP is `O(nk)` and orders of magnitude faster.
5. Characters are not bytes: decode and normalise Unicode before measuring distance.
6. Always include an adversarial input in matching benchmarks — repeated characters expose worst cases that random data hides.
