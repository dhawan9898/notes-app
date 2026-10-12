---
title: Greedy Algorithms
track: dsa
module: Algorithm Design
moduleOrder: 8
order: 2
level: advanced
summary: "When making the locally best choice is provably globally optimal — exchange arguments, interval scheduling, fractional knapsack, job sequencing and Huffman coding (with a canonical bit-level encoder) — tested against brute force, with the plausible greedy rules that fail."
sources:
  - label: "Kleinberg & Tardos — Algorithm Design, Ch. 4 (Greedy Algorithms)"
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 15 (Greedy Algorithms)"
  - label: "Huffman — A Method for the Construction of Minimum-Redundancy Codes (Proc. IRE 40(9), 1952)"
  - label: "Shannon — A Mathematical Theory of Communication (Bell System Technical Journal 27, 1948)"
  - label: "Edmonds — Matroids and the Greedy Algorithm (Mathematical Programming 1, 1971)"
  - label: "Liu & Layland — Scheduling Algorithms for Multiprogramming in a Hard-Real-Time Environment (J. ACM 20(1), 1973)"
  - label: "RFC 1951 — DEFLATE Compressed Data Format Specification version 1.3, §3.2.2 (canonical Huffman codes)"
    url: https://www.rfc-editor.org/rfc/rfc1951
  - label: "Linux kernel documentation — Deadline Task Scheduling (SCHED_DEADLINE)"
    url: https://www.kernel.org/doc/html/latest/scheduler/sched-deadline.html
---

A **greedy algorithm** builds a solution by repeatedly making the choice that looks best *right now* and never reconsidering. It is the simplest and fastest algorithmic paradigm — usually a sort plus a scan, `O(n log n)` — and it is correct only for problems with special structure. The craft of greedy algorithms is therefore mostly about **proof**: knowing when the local choice is safe, and recognising when it is not (see the counterexamples below, and the coin-change failure in the dynamic-programming note).

## Why a greedy choice can be safe

Two standard proof techniques cover almost every case.

- **Greedy-choice property via an exchange argument.** Take any optimal solution `O`. Show that you can *swap* its first decision for the greedy one without making it worse. Then some optimal solution begins with the greedy choice, and by optimal substructure the rest of the problem is the same problem, smaller.
- **Greedy stays ahead.** Show that after each step the greedy solution is at least as good as any other partial solution by some measure, and conclude that it ends at least as good.

If a problem has optimal substructure but the exchange argument fails, you need dynamic programming. The theory of **matroids** (Edmonds, 1971) characterises exactly when the simple "sort by weight, take whatever stays independent" greedy is optimal: it covers minimum spanning trees (the previous graph note), scheduling with deadlines and several other problems, and is why Kruskal's algorithm works. Dijkstra's algorithm is also greedy: the closest unsettled vertex is final because weights are non-negative.

## Four classic problems

### Interval scheduling (activity selection)

Given intervals (meetings, jobs, requests), select the largest set that does not overlap. Of the many natural rules — earliest start, shortest duration, fewest conflicts, earliest finish — **only earliest finish time first is optimal**. Exchange argument: let `g` be the interval that ends first. In any optimal schedule, replace its first interval with `g`: `g` ends no later, so nothing that followed it can now conflict. A sibling problem, the **minimum number of rooms** (or machines, or platforms) needed to serve all intervals, equals the maximum number of simultaneously overlapping intervals — found by sorting starts and ends separately and sweeping.

### Fractional knapsack

If items can be split, take them in decreasing value-per-weight order, filling the last one partially. Exchange: if the knapsack contains a lower-ratio fragment while a higher-ratio item remains unfinished, swap weight between them to gain value. The proof breaks for the **0/1** version, where items cannot be split: the textbook counterexample is below, and 0/1 knapsack needs DP.

### Job sequencing with deadlines

Unit-time jobs have deadlines and profits; at most one job runs per time slot and a job earns profit only if it finishes by its deadline. Greedy: consider jobs in decreasing profit order and put each in the latest free slot at or before its deadline (skip if none). The "latest free slot ≤ d" query is exactly union–find with the pointer `p[t]` = latest free slot at or below `t`, giving near-linear time after the sort.

### Earliest-deadline-first scheduling

A real-time cousin: for a single processor and jobs with deadlines, running the job with the **earliest deadline** first schedules everything whenever any schedule can (Liu & Layland showed EDF is optimal for preemptive uniprocessor scheduling, schedulable up to 100 % utilisation). Linux implements it as the `SCHED_DEADLINE` policy.

```c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

/* ===== 1. Interval scheduling: choose the maximum number of pairwise non-overlapping intervals.
 *          Greedy: sort by END time, take each interval that starts after the last one taken ended. ===== */
typedef struct { int start, end; } Iv;
static int cmp_end(const void *a, const void *b) { return ((const Iv *)a)->end - ((const Iv *)b)->end; }
static int schedule_greedy(Iv *iv, int n) {
    qsort(iv, (size_t)n, sizeof *iv, cmp_end);
    int cnt = 0, last_end = -1; for (int i = 0; i < n; i++) if (iv[i].start >= last_end) { cnt++; last_end = iv[i].end; }
    return cnt;
}
static int schedule_brute(const Iv *iv, int n) {
    int best = 0;
    for (unsigned m = 0; m < (1u << n); m++) {
        int ok = 1, cnt = 0;
        for (int i = 0; i < n && ok; i++) if (m >> i & 1) { cnt++; for (int j = i + 1; j < n; j++) if ((m >> j & 1) && iv[i].start < iv[j].end && iv[j].start < iv[i].end) { ok = 0; break; } }
        if (ok && cnt > best) best = cnt;
    }
    return best;
}
/* two plausible-looking greedy rules that are WRONG: earliest start, shortest duration */
static int cmp_start(const void *a, const void *b) { return ((const Iv *)a)->start - ((const Iv *)b)->start; }
static int cmp_len(const void *a, const void *b) { return (((const Iv *)a)->end - ((const Iv *)a)->start) - (((const Iv *)b)->end - ((const Iv *)b)->start); }
static int schedule_by(Iv *iv, int n, int (*cmp)(const void *, const void *)) {
    qsort(iv, (size_t)n, sizeof *iv, cmp); int cnt = 0, last_end = -1;
    for (int i = 0; i < n; i++) if (iv[i].start >= last_end) { cnt++; last_end = iv[i].end; }
    return cnt;
}

/* ===== 2. Fractional knapsack: greedy by value/weight ratio is optimal (0/1 knapsack by ratio is NOT) ===== */
typedef struct { double w, v; } Item;
static int cmp_ratio(const void *a, const void *b) { double x = ((const Item *)a)->v / ((const Item *)a)->w, y = ((const Item *)b)->v / ((const Item *)b)->w; return x < y ? 1 : x > y ? -1 : 0; }
static double fractional(Item *it, int n, double cap) {
    qsort(it, (size_t)n, sizeof *it, cmp_ratio); double total = 0;
    for (int i = 0; i < n && cap > 0; i++) { double take = it[i].w < cap ? it[i].w : cap; total += it[i].v * take / it[i].w; cap -= take; }
    return total;
}
static double fractional_brute(const Item *it, int n, double cap) {      /* an optimum has at most one fractional item: try every full set + one fractional extra */
    double best = 0;
    for (unsigned m = 0; m < (1u << n); m++) {
        double w = 0, v = 0; for (int i = 0; i < n; i++) if (m >> i & 1) { w += it[i].w; v += it[i].v; }
        if (w > cap) continue;
        if (v > best) best = v;
        for (int j = 0; j < n; j++) if (!(m >> j & 1)) { double room = cap - w, take = room < it[j].w ? room : it[j].w; double tv = v + it[j].v * take / it[j].w; if (tv > best) best = tv; }
    }
    return best;
}

/* ===== 3. Job sequencing with deadlines: unit-time jobs, each with a deadline and profit; maximize profit.
 *          Greedy: consider jobs by decreasing profit, place each in the LATEST free slot <= its deadline (union-find finds free slots). ===== */
typedef struct { int deadline, profit; } Job;
static int cmp_profit(const void *a, const void *b) { return ((const Job *)b)->profit - ((const Job *)a)->profit; }
static int find(int *p, int x) { while (p[x] != x) { p[x] = p[p[x]]; x = p[x]; } return x; }
static long jobs_greedy(Job *j, int n, int maxd) {
    qsort(j, (size_t)n, sizeof *j, cmp_profit);
    int *p = malloc((size_t)(maxd + 1) * sizeof(int)); for (int i = 0; i <= maxd; i++) p[i] = i;   /* p[t] = latest free slot <= t (0 = none) */
    long total = 0;
    for (int i = 0; i < n; i++) { int d = j[i].deadline < maxd ? j[i].deadline : maxd, slot = find(p, d); if (slot > 0) { total += j[i].profit; p[slot] = slot - 1; } }
    free(p); return total;
}
static long jobs_brute(const Job *j, int n) {
    long best = 0;
    for (unsigned m = 0; m < (1u << n); m++) {                              /* a set is feasible iff after sorting by deadline the k-th job has deadline >= k */
        int cnt[16] = {0}, k = 0; long profit = 0; for (int i = 0; i < n; i++) if (m >> i & 1) { cnt[j[i].deadline < 15 ? j[i].deadline : 15]++; k++; profit += j[i].profit; }
        int ok = 1, upto = 0; for (int d = 0; d < 16; d++) { upto += cnt[d]; if (upto > d) ok = 0; }
        if (ok && profit > best) best = profit;
    }
    return best;
}

/* ===== 4. Minimum number of meeting rooms = maximum overlap: sort starts and ends separately, sweep ===== */
static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }
static int rooms(const Iv *iv, int n) {
    int *st = malloc((size_t)n * sizeof(int)), *en = malloc((size_t)n * sizeof(int));
    for (int i = 0; i < n; i++) { st[i] = iv[i].start; en[i] = iv[i].end; }
    qsort(st, (size_t)n, sizeof(int), cmp_int); qsort(en, (size_t)n, sizeof(int), cmp_int);
    int cur = 0, best = 0, j = 0; for (int i = 0; i < n; i++) { while (en[j] <= st[i]) { j++; cur--; } cur++; if (cur > best) best = cur; }
    free(st); free(en); return best;
}
static int rooms_brute(const Iv *iv, int n) { int best = 0; for (int t = 0; t < 64; t++) { int c = 0; for (int i = 0; i < n; i++) c += iv[i].start <= t && t < iv[i].end; if (c > best) best = c; } return best; }

int main(void) {
    int diff_start = 0, diff_len = 0;
    for (int round = 0; round < 5000; round++) {
        int n = 1 + (int)(rnd() % 12); Iv iv[12], c1[12], c2[12], c3[12];
        for (int i = 0; i < n; i++) { iv[i].start = (int)(rnd() % 30); iv[i].end = iv[i].start + 1 + (int)(rnd() % 12); }
        memcpy(c1, iv, sizeof iv); memcpy(c2, iv, sizeof iv); memcpy(c3, iv, sizeof iv);
        int opt = schedule_brute(iv, n); assert(schedule_greedy(c1, n) == opt);
        if (schedule_by(c2, n, cmp_start) != opt) diff_start++;
        if (schedule_by(c3, n, cmp_len) != opt) diff_len++;
        assert(rooms(iv, n) == rooms_brute(iv, n));
    }
    printf("interval scheduling, 5000 random instances: earliest-FINISH-first equals the brute-force optimum every time;\n");
    printf("  the plausible rules 'earliest start' and 'shortest interval' were suboptimal in %d and %d of them.  Meeting-room sweep == max overlap.\n\n", diff_start, diff_len);
    for (int round = 0; round < 3000; round++) {
        int n = 1 + (int)(rnd() % 7); Item it[7], c[7]; double cap = 1 + (double)(rnd() % 30);
        for (int i = 0; i < n; i++) { it[i].w = 1 + (double)(rnd() % 12); it[i].v = 1 + (double)(rnd() % 30); }
        memcpy(c, it, sizeof it); double g = fractional(c, n, cap), b = fractional_brute(it, n, cap);
        assert(g - b < 1e-9 && b - g < 1e-9);
    }
    printf("fractional knapsack, 3000 random instances: greedy by value/weight ratio equals the exhaustive optimum\n");
    {   /* the textbook counterexample for the 0/1 version: greedy by ratio vs exhaustive search */
        Item it[3] = { {10, 60}, {20, 100}, {30, 120} }; double cap = 50, w = 0, v = 0, best = 0;
        qsort(it, 3, sizeof *it, cmp_ratio); for (int i = 0; i < 3; i++) if (w + it[i].w <= cap) { w += it[i].w; v += it[i].v; }        /* whole items only */
        for (unsigned m = 0; m < 8; m++) { double W = 0, V = 0; for (int i = 0; i < 3; i++) if (m >> i & 1) { W += it[i].w; V += it[i].v; } if (W <= cap && V > best) best = V; }
        printf("  but 0/1 knapsack (whole items only), capacity 50, items (w,v) = (10,60) (20,100) (30,120): ratio-greedy gets %.0f, the optimum is %.0f\n\n", v, best); }
    for (int round = 0; round < 3000; round++) {
        int n = 1 + (int)(rnd() % 10); Job j[10], c[10]; for (int i = 0; i < n; i++) { j[i].deadline = 1 + (int)(rnd() % 6); j[i].profit = 1 + (int)(rnd() % 50); }
        memcpy(c, j, sizeof j); assert(jobs_greedy(c, n, 12) == jobs_brute(j, n));
    }
    printf("job sequencing with deadlines, 3000 random instances: greedy (highest profit first, latest free slot) equals brute force\n");
    return 0;
}
```

```text
$ gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined greedy.c -o greedy && ./greedy
interval scheduling, 5000 random instances: earliest-FINISH-first equals the brute-force optimum every time;
  the plausible rules 'earliest start' and 'shortest interval' were suboptimal in 1198 and 3510 of them.  Meeting-room sweep == max overlap.

fractional knapsack, 3000 random instances: greedy by value/weight ratio equals the exhaustive optimum
  but 0/1 knapsack (whole items only), capacity 50, items (w,v) = (10,60) (20,100) (30,120): ratio-greedy gets 160, the optimum is 220

job sequencing with deadlines, 3000 random instances: greedy (highest profit first, latest free slot) equals brute force
```

Each greedy is compared with exhaustive search on thousands of small random instances. The experiment also checks the *wrong* rules: "earliest start" and "shortest interval" — both sound reasonable — fail on a large share of random instances, which is exactly why a greedy rule must come with a proof rather than an intuition. And the 0/1 knapsack counterexample is computed, not just claimed: ratio-greedy gets 160, the optimum is 220.

## Huffman coding

Given symbol frequencies, find a **prefix code** (no codeword is a prefix of another, so a bit stream decodes unambiguously without separators) with minimum expected length. Huffman's greedy algorithm repeatedly merges the two least frequent subtrees into one whose weight is their sum; the merge order builds the code tree, and each symbol's codeword is its path from the root.

Why it is optimal (sketch): in an optimal tree the two least frequent symbols can be taken as siblings at the deepest level (exchange argument: swapping a rarer symbol deeper never increases cost), so merging them into a single "symbol" of combined weight reduces the problem to a smaller instance.

A second theorem bounds what is possible at all: **Shannon's source-coding theorem** says no symbol code can average fewer bits per symbol than the entropy `H = −Σ p_i log₂ p_i`, and Huffman's average length lies in `[H, H + 1)`.

**Canonical codes.** Only the *lengths* of an optimal code matter; any assignment of codes with those lengths works. The **canonical** assignment gives codes of equal length consecutive values in symbol order, so a decoder needs only the list of code lengths to reconstruct every codeword — which is why DEFLATE (RFC 1951 §3.2.2; used by gzip, zlib, PNG, ZIP) transmits just the lengths. It also allows a very small decoder: for each length `l`, store the first code and the index of its first symbol, and a code of length `l` is valid exactly when it falls in that length's consecutive block.

The program below builds the code lengths with a heap, assigns canonical codes, encodes two million symbols into an actual bit buffer, decodes it with the canonical table, and compares sizes with the entropy bound. It also cross-checks Huffman's cost against brute force over *every* length assignment that satisfies the Kraft equality for 2000 random small alphabets.

```c
#define _GNU_SOURCE
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned long long u64;
#define MAXSYM 256

/* ---------- build optimal code LENGTHS with the greedy rule: repeatedly merge the two least frequent subtrees ---------- */
typedef struct { u64 w; int left, right; } HNode;          /* leaves are 0..n-1, internal nodes follow */
static void hsift_down(int *h, int n, int i, const HNode *t) {
    int x = h[i];
    for (;;) { int c = 2 * i + 1; if (c >= n) break; if (c + 1 < n && t[h[c + 1]].w < t[h[c]].w) c++; if (t[x].w <= t[h[c]].w) break; h[i] = h[c]; i = c; }
    h[i] = x;
}
static void hsift_up(int *h, int i, const HNode *t) { int x = h[i]; while (i && t[h[(i - 1) / 2]].w > t[x].w) { h[i] = h[(i - 1) / 2]; i = (i - 1) / 2; } h[i] = x; }
static void code_lengths(const u64 *freq, int n, int *len) {
    if (n == 1) { len[0] = 1; return; }                        /* a one-symbol alphabet still needs one bit per symbol */
    HNode *t = malloc((size_t)(2 * n) * sizeof *t); int *h = malloc((size_t)n * sizeof(int)), hn = 0, total = n;
    for (int i = 0; i < n; i++) { t[i] = (HNode){ freq[i], -1, -1 }; h[hn] = i; hsift_up(h, hn++, t); }
    while (hn > 1) {
        int a = h[0]; h[0] = h[--hn]; if (hn) hsift_down(h, hn, 0, t);
        int b = h[0]; h[0] = h[--hn]; if (hn) hsift_down(h, hn, 0, t);
        t[total] = (HNode){ t[a].w + t[b].w, a, b }; h[hn] = total; hsift_up(h, hn++, t); total++;
    }
    int root = h[0], *depth = calloc((size_t)total, sizeof(int)), *st = malloc((size_t)total * sizeof(int)), sp = 0; st[sp++] = root;
    while (sp) { int v = st[--sp]; if (t[v].left < 0) len[v] = depth[v]; else { depth[t[v].left] = depth[t[v].right] = depth[v] + 1; st[sp++] = t[v].left; st[sp++] = t[v].right; } }
    free(t); free(h); free(depth); free(st);
}

/* ---------- canonical codes (as in DEFLATE, RFC 1951 3.2.2): codes of equal length are consecutive integers in symbol order, so only lengths need be stored ---------- */
static void canonical(const int *len, int n, uint32_t *code) {
    int maxl = 0; for (int i = 0; i < n; i++) if (len[i] > maxl) maxl = len[i];
    int *count = calloc((size_t)maxl + 2, sizeof(int)); uint32_t *next = calloc((size_t)maxl + 2, sizeof(uint32_t));
    for (int i = 0; i < n; i++) count[len[i]]++;
    uint32_t c = 0; count[0] = 0; for (int l = 1; l <= maxl; l++) { c = (c + (uint32_t)count[l - 1]) << 1; next[l] = c; }
    for (int i = 0; i < n; i++) code[i] = next[len[i]]++;
    free(count); free(next);
}

/* ---------- bit-level encoder / decoder ---------- */
typedef struct { uint8_t *buf; u64 bits; } BitW;
static void put_bits(BitW *w, uint32_t code, int len) { for (int i = len - 1; i >= 0; i--) { if ((w->bits & 7) == 0) w->buf[w->bits >> 3] = 0; if (code >> i & 1) w->buf[w->bits >> 3] |= (uint8_t)(0x80 >> (w->bits & 7)); w->bits++; } }

int main(void) {
    /* ---- an English-like byte stream: 2,000,000 symbols drawn from letter frequencies ---- */
    static const char *pool = "eeeeeeeeeeeeettttttttttaaaaaaaaoooooooiiiiiiinnnnnnnsssssshhhhhhrrrrrrddddllllcccuuummmwwffggyyppbbvkjxqz          ,.\n";
    size_t pl = strlen(pool), N = 2000000; uint8_t *text = malloc(N); uint64_t r = 88172645463325252ULL;
    for (size_t i = 0; i < N; i++) { r ^= r << 13; r ^= r >> 7; r ^= r << 17; text[i] = (uint8_t)pool[(r >> 11) % pl]; }
    u64 freq[256] = {0}; for (size_t i = 0; i < N; i++) freq[text[i]]++;
    int sym[MAXSYM], n = 0; u64 f[MAXSYM]; for (int c = 0; c < 256; c++) if (freq[c]) { sym[n] = c; f[n++] = freq[c]; }
    int len[MAXSYM]; code_lengths(f, n, len);
    /* sort symbols by (length, symbol) is implicit: canonical() assigns codes in index order, so first reorder by length */
    int order[MAXSYM]; for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 1; i < n; i++) { int x = order[i], j = i; while (j && (len[order[j - 1]] > len[x] || (len[order[j - 1]] == len[x] && sym[order[j - 1]] > sym[x]))) { order[j] = order[j - 1]; j--; } order[j] = x; }
    int slen[MAXSYM]; for (int i = 0; i < n; i++) slen[i] = len[order[i]];
    uint32_t scode[MAXSYM]; canonical(slen, n, scode);
    uint32_t code_of[256]; int len_of[256]; for (int i = 0; i < n; i++) { code_of[sym[order[i]]] = scode[i]; len_of[sym[order[i]]] = slen[i]; }

    double kraft = 0, entropy = 0; for (int i = 0; i < n; i++) { kraft += ldexp(1.0, -len[i]); double p = (double)f[i] / (double)N; entropy -= p * log2(p); }
    u64 coded_bits = 0; for (int i = 0; i < n; i++) coded_bits += f[i] * (u64)len[i];
    printf("%zu symbols, %d distinct\n", N, n);
    printf("%-34s %10s\n", "code", "bits/symbol");
    printf("%-34s %10.4f\n", "fixed 8-bit bytes", 8.0);
    printf("%-34s %10.4f\n", "fixed ceil(log2 n) bits", ceil(log2(n)));
    printf("%-34s %10.4f\n", "Huffman (measured)", (double)coded_bits / (double)N);
    printf("%-34s %10.4f   (Shannon entropy: no symbol code can beat it)\n", "entropy H", entropy);
    printf("Kraft sum of the code lengths = %.6f (a complete prefix code sums to exactly 1)\n", kraft);
    printf("code lengths: shortest %d bits, longest %d bits\n", slen[0], slen[n - 1]);

    /* ---- encode, then decode with the canonical decoder ---- */
    BitW w = { malloc((coded_bits + 7) / 8 + 8), 0 };   /* size from the computed total: guessing "N * 4 / 8" overflowed (AddressSanitizer caught it) */
    for (size_t i = 0; i < N; i++) put_bits(&w, code_of[text[i]], len_of[text[i]]);
    printf("encoded size: %llu bits = %llu bytes (%.1f%% of the %zu-byte original)\n", w.bits, (w.bits + 7) / 8, 100.0 * (double)((w.bits + 7) / 8) / (double)N, N);
    int maxl = slen[n - 1], *cnt = calloc((size_t)maxl + 2, sizeof(int)); for (int i = 0; i < n; i++) cnt[slen[i]]++;
    uint32_t *first = calloc((size_t)maxl + 2, sizeof(uint32_t)); int *offset = calloc((size_t)maxl + 2, sizeof(int)); { uint32_t c = 0; int idx = 0; for (int l = 1; l <= maxl; l++) { c <<= 1; first[l] = c; offset[l] = idx; c += (uint32_t)cnt[l]; idx += cnt[l]; } }
    uint8_t *out = malloc(N); size_t produced = 0; u64 pos = 0;
    while (produced < N) {
        uint32_t code = 0; int l = 0;
        for (;;) { code = code << 1 | ((w.buf[pos >> 3] >> (7 - (pos & 7))) & 1); pos++; l++; if (cnt[l] && code - first[l] < (uint32_t)cnt[l]) break; }   /* a code of length l is valid iff it falls in l's consecutive block */
        out[produced++] = (uint8_t)sym[order[offset[l] + (int)(code - first[l])]];
    }
    assert(memcmp(out, text, N) == 0);
    printf("decoded with the canonical decoder: identical to the input\n\n");

    /* ---- optimality: for small alphabets, search every length assignment that satisfies Kraft equality ---- */
    int checked = 0;
    for (int round = 0; round < 2000; round++) {
        r = r * 6364136223846793005ULL + 1442695040888963407ULL;
        int m = 2 + (int)((r >> 33) % 5); u64 fr[8]; for (int i = 0; i < m; i++) { r = r * 6364136223846793005ULL + 1442695040888963407ULL; fr[i] = 1 + (r >> 33) % 50; }
        int L[8]; code_lengths(fr, m, L); u64 huff = 0; for (int i = 0; i < m; i++) huff += fr[i] * (u64)L[i];
        u64 best = ~0ULL; int lens[8]; int total = 1; for (int i = 0; i < m; i++) total *= (m - 1);
        for (int code = 0; code < total; code++) {
            int c = code; double k = 0; for (int i = 0; i < m; i++) { lens[i] = 1 + c % (m - 1); c /= (m - 1); k += ldexp(1.0, -lens[i]); }
            if (k == 1.0) { u64 cost = 0; for (int i = 0; i < m; i++) cost += fr[i] * (u64)lens[i]; if (cost < best) best = cost; }
        }
        assert(huff == best); checked++;
    }
    printf("%d random alphabets of 2-6 symbols: the greedy Huffman cost equals the minimum over all complete prefix-code length assignments\n", checked);
    free(text); free(w.buf); free(cnt); free(first); free(offset); free(out);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined huffman.c -o huffman -lm && ./huffman
2000000 symbols, 30 distinct
code                               bits/symbol
fixed 8-bit bytes                      8.0000
fixed ceil(log2 n) bits                5.0000
Huffman (measured)                     4.4828
entropy H                              4.4565   (Shannon entropy: no symbol code can beat it)
Kraft sum of the code lengths = 1.000000 (a complete prefix code sums to exactly 1)
code lengths: shortest 3 bits, longest 7 bits
encoded size: 8965554 bits = 1120695 bytes (56.0% of the 2000000-byte original)
decoded with the canonical decoder: identical to the input

2000 random alphabets of 2-6 symbols: the greedy Huffman cost equals the minimum over all complete prefix-code length assignments
```

Observe: the measured average code length is about 4.48 bits per symbol, within 0.03 bits of the entropy `H ≈ 4.46` (the bound no symbol code can beat) and well under the 5 bits a fixed-width code for 30 symbols needs; the encoded stream is about 56 % of the original bytes; the Kraft sum is exactly 1 (a *complete* prefix code); the decoded stream matches the original bit for bit; and the greedy cost equals the brute-force optimum on all 2000 small alphabets.

An implementation detail worth recording: the first version of this program sized the output buffer with a guess (`N·4/8` bytes) that was too small for a 4.48 bit/symbol code; **AddressSanitizer reported the heap overflow at the first run**. Sizing buffers from the computed bit count is the fix. Bit-packing code is a prime place for off-by-one buffer errors; test it under sanitizers.

Practical notes: real formats **limit the code length** (DEFLATE: 15 bits) using a package–merge or heuristic adjustment; adaptive and arithmetic/range/ANS coders beat Huffman by coding fractional bits per symbol (Huffman cannot go below 1 bit for even a near-certain symbol); and JPEG, MP3 and PNG all use Huffman-style entropy coding as one stage.

## Greedy that fails

| Greedy rule | Fails on |
|---|---|
| Coin change: largest coin first | coins `{4,3,1}`, amount 6 (greedy 4+1+1, optimal 3+3); works for "canonical" systems such as 1, 5, 10, 25 |
| 0/1 knapsack by value/weight ratio | the (10,60), (20,100), (30,120) example above |
| Interval scheduling by earliest start or shortest length | the random instances above |
| Shortest path by always taking the cheapest next edge | graphs where an expensive first edge leads to cheap rest (Dijkstra tracks *accumulated* distance, not edge cost) |
| Travelling salesman: nearest neighbour | by arbitrary factors (it has no constant approximation guarantee) |
| Graph colouring: first fit | can use `n/2` colours on a bipartite graph in bad vertex order |

Greedy is also a respectable **approximation** technique when exact greedy fails: greedy set cover is within a factor `ln n` of optimal (and that is essentially the best possible unless P = NP); greedy for submodular maximisation gives `1 − 1/e`; Graham's list scheduling is a 2-approximation for makespan.

## Pitfalls

- **Intuition is not proof.** Every greedy rule sounds plausible; write the exchange argument, or find a counterexample by brute force on tiny random inputs — a fifteen-line harness like the one above finds most wrong rules in seconds.
- **Ties and sort stability**: a wrong tie-break (finish time equal, which first?) can matter for some variants; define the order completely.
- **Sorting key overflow**: `a - b` comparators on large values overflow; compare with `<` and `>`.
- **Floating-point ratios** in fractional-knapsack-style problems: compare by cross-multiplication (`v1*w2 > v2*w1`) when exactness matters.
- **Closed vs half-open intervals**: whether `[1,3]` and `[3,5]` overlap changes the answer; the code uses half-open intervals.
- **Using greedy where the problem is NP-hard** and presenting the result as optimal; label heuristics as heuristics.
- **Buffer sizing in bit-level encoders** (above): derive the size from the data, not from an assumed ratio.

## Key takeaways

1. A greedy algorithm needs a proof — an exchange argument or "stays ahead" — not just a plausible rule; the wrong-rule experiment shows how often intuition fails.
2. Interval scheduling: sort by finish time. Rooms needed = maximum overlap. Fractional knapsack: sort by ratio. Deadline scheduling: EDF, and union–find for slot lookup.
3. Huffman's merge-two-lightest rule gives an optimal prefix code within one bit of entropy; canonical codes let a format store only code lengths (DEFLATE).
4. Many core graph algorithms (Kruskal, Prim, Dijkstra) are greedy, justified by the cut property and matroid theory.
5. When greedy fails (0/1 knapsack, general coin change), the problem usually needs dynamic programming or is NP-hard.
6. Always cross-check a greedy solution against brute force on small random inputs, and size bit buffers from computed lengths.
