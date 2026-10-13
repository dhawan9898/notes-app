---
title: "Suffix Arrays, LCP and the Burrows-Wheeler FM-Index"
track: dsa
module: Strings
moduleOrder: 7
order: 2
level: expert
summary: "Index a text once and answer substring questions fast: suffix array construction by prefix doubling with radix sort, Kasai's linear-time LCP array, applications (counting, longest repeated and common substrings, distinct substrings), and the Burrows-Wheeler transform with backward search, all verified against naive code."
prerequisites:
  - String Matching and Edit Distance
  - Binary Search and Its Variants
sources:
  - label: "Manber & Myers — Suffix arrays: a new method for on-line string searches (SIAM J. Comput. 1993)"
    url: https://epubs.siam.org/doi/10.1137/0222058
  - label: "Kasai et al. — Linear-Time Longest-Common-Prefix Computation in Suffix Arrays (CPM 2001)"
    url: https://link.springer.com/chapter/10.1007/3-540-48194-X_17
  - label: "Nong, Zhang & Chan — Two Efficient Algorithms for Linear Time Suffix Array Construction (IEEE Trans. Computers 2011), SA-IS"
    url: https://ieeexplore.ieee.org/document/5582081
  - label: "Burrows & Wheeler — A Block-sorting Lossless Data Compression Algorithm (DEC SRC Research Report 124, 1994)"
  - label: "Ferragina & Manzini — Opportunistic Data Structures with Applications (FOCS 2000), the FM-index"
    url: https://ieeexplore.ieee.org/document/892127
  - label: "Gusfield — Algorithms on Strings, Trees, and Sequences (Cambridge University Press, 1997)"
  - label: "Langmead & Salzberg — Fast gapped-read alignment with Bowtie 2 (Nature Methods 2012)"
    url: https://www.nature.com/articles/nmeth.1923
---

KMP, Z and Boyer–Moore search one pattern in one text in time proportional to the text. When the **text is fixed and the queries are many** — a genome, a log archive, a source tree, a dictionary of names — it pays to build an **index** once and answer each query in time proportional to the *pattern*. The simplest index is the **suffix array**.

## Definition

For a text `s` of length *n*, the suffixes are `s[i..n)` for `i = 0 … n−1`. The **suffix array** `sa` lists the starting positions of the suffixes **in lexicographic order of the suffixes**. For `banana`:

| rank | `sa` | suffix |
|---|---|---|
| 0 | 5 | `a` |
| 1 | 3 | `ana` |
| 2 | 1 | `anana` |
| 3 | 0 | `banana` |
| 4 | 4 | `na` |
| 5 | 2 | `nana` |

Every occurrence of a pattern `p` is the **prefix of some suffix**, and all suffixes that begin with `p` are adjacent in sorted order. So the occurrences of `p` form a contiguous range of `sa`, found with two binary searches in `O(m log n)` (compare `p` with the suffix at the midpoint, at most `m` characters each). The array takes `n` integers: far less than a suffix tree (which needs about 10–20 bytes per character) while answering most of the same questions.

The **LCP array** stores, for each rank *i*, the length of the longest common prefix of the suffixes at ranks *i−1* and *i*. It turns the sorted list into a tree-like structure without building one: the longest common prefix of any two suffixes is the minimum of the LCP values between their ranks (a range-minimum query; see the range-query note).

## Building the suffix array

The naive way sorts the *n* suffixes with a comparison that may read `O(n)` characters, giving `O(n² log n)` in the worst case (for `aaaa…a` every comparison runs to the end). Better:

- **Prefix doubling** (Manber–Myers): sort suffixes by their first character, then by their first 2, 4, 8, … characters. After the round with prefix length *k*, each suffix has a **rank** among all prefixes of length *k*. To sort by the first 2k characters, sort by the pair `(rank[i], rank[i+k])`, which are both known from the previous round. With a **radix sort** (counting sort on the second key, then a stable counting sort on the first key) each round costs `O(n)`, and there are at most `⌈log₂ n⌉` rounds: **O(n log n)** in total. The loop stops early as soon as all ranks are distinct.
- **SA-IS** (Nong–Zhang–Chan), the **DC3/skew** algorithm and others achieve **O(n)** time; SA-IS is what production libraries (libdivsufsort, the `sais` family) use and is several times faster than doubling in practice, at the price of a more intricate implementation.

### The implementation

The code below contains prefix doubling with radix sort, Kasai's LCP computation, the binary-search pattern counter, and an FM-index (explained later).

```c
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
```

Two details deserve attention. In `build_sa`, suffixes shorter than `k` have no second half; they are given the smallest possible second key (listed first), which is what makes the "sort by the second key" step free: listing suffixes in the order of the previous round's `sa`, shifted by `k`, already gives them sorted by their second key. In `build_lcp` (Kasai et al.), if suffix *i* shares *h* characters with its predecessor in sorted order, then suffix *i+1* shares at least *h−1* with *its* predecessor, so `h` only ever decreases by one per step and the total work is **O(n)** even though the inner `while` looks quadratic.

### Verification

The test compares each structure with a naive implementation on 4,000 random strings over alphabets of size 1 to 4 (so there are plenty of repeats, and every seventh string is a run of one letter, the worst case for doubling): the suffix array against a plain sort of whole suffixes, the LCP array against direct comparison, and pattern counts from both the suffix array and the FM-index against a scan. It runs under AddressSanitizer and UBSan.

```c
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
```

```text
$ gcc -O1 -g -Wall -Wextra -Wno-unused-function -fsanitize=address,undefined test.c -o t && ./t
4000 strings: suffix array, LCP array, SA binary-search counts and FM-index counts all match naive code (120000 pattern queries)
```

## What the suffix array and LCP array can answer

```c
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
```

```text
$ gcc -O2 -Wall -Wextra -Wno-unused-function apps.c -o apps && ./apps
text: "banana bandana banana" (n = 21)
sorted suffixes (first 6): 14:" banana", 6:" bandana", 20:"a", 13:"a banana", 5:"a bandan", 18:"ana"
longest repeated substring: "ana ban" (length 7) = the largest LCP value
distinct substrings: 176   (n(n+1)/2 minus the sum of the LCP array)
occurrences of "ana": 5 at positions 18 11 3 16 1
longest common substring of "the quick brown fox" and "a quick brown dog": " quick brown "
FM-index count of "ana" using only the BWT: 5;  BWT of the text (0 = terminator shown as $): aaannnndnbbb $ naaaaaa

text (1,000,000 bytes)    build (s)   rounds      LCP (s)
random, 4 letters             0.147        5        0.036
periodic (abcdefgh)*          0.269       20        0.007
all the same letter           0.330       20        0.007
```

- **Counting and locating a pattern**: occurrences of `ana` occupy a contiguous block of `sa`; their starting positions are read off directly (5 matches, including overlapping ones).
- **Longest repeated substring**: the largest value in the LCP array, with its position in `sa`: here `"ana ban"`, which occurs twice.
- **Number of distinct substrings**: every substring is a prefix of some suffix; suffix *i* contributes `n − sa[i]` prefixes but the first `lcp[i]` of them were already counted by its neighbor, so the total is `n(n+1)/2 − Σ lcp[i]`.
- **Longest common substring of two strings**: concatenate them with a separator that appears in neither, build the arrays, and take the largest LCP between *adjacent* suffixes that come from different strings.
- Other uses: the shortest unique substring, the longest palindromic substring (with the reversed text), the k-th smallest substring, and the building block of compressed indexes.

The timing table at the end shows the cost structure. Random text finishes in about five doubling rounds because suffixes are distinguished after a few characters; **periodic and constant text** needs the full log₂ n rounds (20 for a million characters) and is a few times slower, which is the adversarial case to test. Kasai's LCP pass is linear and takes a fraction of the build time.

## The Burrows–Wheeler transform and the FM-index

The **Burrows–Wheeler transform (BWT)** of a text (with a unique terminator `$` smaller than every other character appended) is obtained by sorting all rotations (equivalently all suffixes) and writing down the **last column**: for each suffix in sorted order, the character that *precedes* it, `bwt[i] = s[sa[i] − 1]`. In the output above the BWT of `banana bandana banana` is the string printed on the last line: equal characters cluster together (long runs of `a` and `n`), which is why bzip2 follows the BWT with move-to-front and Huffman coding.

The remarkable fact is that the BWT can be **inverted and searched without the original text**. Define:

- `C[c]` = the number of characters in the text smaller than `c`;
- `Occ(c, i)` = the number of occurrences of `c` in `bwt[0..i)`.

All suffixes beginning with a character `c` occupy a block of rows starting at `C[c]`. **Backward search** processes the pattern from its last character to its first, maintaining the range `[lo, hi)` of rows (sorted suffixes) that start with the part of the pattern matched so far. Prepending character `c` maps the range to `[C[c] + Occ(c, lo), C[c] + Occ(c, hi))`. After the whole pattern is processed, `hi − lo` is the **number of occurrences**, with `m` steps and no reference to the text. The `fm_count` function above does exactly this, and the test confirms it matches the naive count. To *locate* occurrences, the index keeps a sampled subset of the suffix array and walks backwards until it hits a sample.

Space: the BWT string plus sampled `Occ` checkpoints (every 64 positions here) and a sampled suffix array. Real FM-indexes use wavelet trees or compressed bit vectors so that the whole structure takes less space than the *original text* (for DNA about 2 bits per base plus overhead), which is how **Bowtie** and **BWA** align billions of short reads against a human genome (3 billion bases) on a machine with a few gigabytes.

## Choosing the structure

| Need | Structure |
|---|---|
| One pattern, one text, once | KMP, Z, Boyer–Moore–Horspool |
| Many fixed patterns, scan a text | Aho–Corasick automaton |
| Many queries on a fixed text; count and locate | suffix array + binary search (simple, `4n` bytes) |
| Same, with LCP-based questions | suffix array + LCP + RMQ |
| Very large text, memory-bound (genomics) | FM-index (compressed) |
| Online construction, substring counts, lots of memory | suffix automaton, suffix tree (Ukkonen) |
| Approximate matching (edit distance ≤ k) | FM-index with backtracking, or q-gram filtering plus verification |

## Pitfalls

- **Terminator and separators:** the BWT needs a unique smallest terminator not occurring in the text; strings concatenated for common-substring queries need separators that occur nowhere. Mixing them up produces matches across string boundaries.
- **Binary data and the byte 0:** the code above uses byte 0 as the FM terminator; text with embedded zero bytes needs a different alphabet mapping.
- **Memory:** the doubling implementation uses five integer arrays of size *n*; for a 100 MB text use SA-IS or libdivsufsort and 32-bit indexes (a 4 GB text needs 64-bit indexes).
- **Comparison cost:** binary search compares pattern and suffix with `memcmp` for up to `m` bytes; for repetitive text with long patterns, use the LCP to skip, or an auxiliary index.
- **Case, Unicode:** suffix arrays sort bytes; to search Unicode text case-insensitively, normalize the text first, and note that the byte suffixes of UTF-8 start in the middle of characters unless you restrict to valid boundaries.

## Key takeaways

- A suffix array lists all suffixes in sorted order; the occurrences of any pattern form a contiguous range found by binary search in `O(m log n)`.
- Prefix doubling with radix sort builds it in `O(n log n)`; SA-IS does it in `O(n)`; Kasai's algorithm builds the LCP array in `O(n)`.
- With the LCP array, longest repeated substring, distinct substring counting and longest common substring become one linear scan.
- The Burrows–Wheeler transform plus `C` and `Occ` tables gives an FM-index: pattern counting by backward search in `O(m)` steps without storing the text, and in compressed form it makes genome-scale search practical.
- Test construction against a naive sort on tiny alphabets and constant strings: the worst cases are where the bugs hide.
