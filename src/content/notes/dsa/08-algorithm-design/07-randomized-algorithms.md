---
title: "Randomized Algorithms: Las Vegas, Monte Carlo and Why Randomness Helps"
track: dsa
module: Algorithm Design
moduleOrder: 8
order: 7
level: advanced
summary: "Algorithms that flip coins: Las Vegas (always right, random time: randomized quicksort and quickselect) and Monte Carlo (fast, small chance of error: Freivalds' matrix check, Miller-Rabin, Karger's min cut, reservoir sampling), with each probability bound measured, plus how to generate random numbers correctly and how to reason about expected cost and amplification."
prerequisites:
  - "Comparison Sorts — Insertion, Merge, Quick, Heap and Introsort"
  - Number Theory Essentials
  - "Hash Tables: Open Addressing, Deletion, Cuckoo and Hash Flooding"
sources:
  - label: "Motwani & Raghavan — Randomized Algorithms (Cambridge University Press, 1995)"
  - label: "Mitzenmacher & Upfal — Probability and Computing (2nd ed., Cambridge University Press, 2017)"
  - label: "Freivalds — Probabilistic machines can use less running time (IFIP Congress 1977)"
  - label: "Rabin — Probabilistic algorithm for testing primality (Journal of Number Theory 12, 1980)"
    url: https://www.sciencedirect.com/science/article/pii/0022314X80900840
  - label: "Karger — Global min-cuts in RNC, and other ramifications of a simple min-cut algorithm (SODA 1993)"
  - label: "Vitter — Random sampling with a reservoir (ACM TOMS 11(1), 1985)"
    url: https://dl.acm.org/doi/10.1145/3147.3165
  - label: "Hoare — Algorithm 65: Find (CACM 4(7), 1961), quickselect"
---

A **randomized algorithm** uses random bits as part of its logic. Randomness is not a trick for the lazy: for some problems the best known algorithm is randomized (or randomization makes a simple algorithm competitive with a complicated deterministic one), and for others, such as defending against an adversary who chooses the input, nothing deterministic can work. Two flavours matter.

| | Las Vegas | Monte Carlo |
|---|---|---|
| Answer | always correct | correct with probability ≥ 1 − ε |
| Running time | random (a random variable) | fixed or bounded |
| Analysis | expected running time, tail bounds | error probability, repetition |
| Examples | randomized quicksort, quickselect, hash tables with random hash functions, treaps | Miller–Rabin, Freivalds, Karger min-cut, Bloom filters |
| Conversion | stop after a time limit and you have a Monte Carlo algorithm | verify the answer and repeat until it is right: a Las Vegas algorithm, when verification is cheap |

## Las Vegas: randomizing the pivot

Quicksort with a fixed rule (always the first or last element as the pivot) has a worst case of O(n²), and the bad inputs are not exotic: *sorted* or *reversed* data makes every pivot the smallest or largest element. An adversary (or merely an unlucky data source) can force it every time. Choosing the pivot **uniformly at random** changes the guarantee: for *every* input, the expected number of comparisons is about **2n ln n ≈ 1.39 n log₂ n**, because the probability that two elements `i < j` are ever compared is `2/(j − i + 1)` (they are compared only if one of them is the first pivot chosen among the elements between them), and summing over all pairs gives `2n H_n`. No input is bad; only unlucky coin flips are, and the probability of a running time much more than the expectation falls off exponentially.

**Quickselect** finds the k-th smallest element by partitioning as quicksort does but recursing into only one side. With random pivots the expected cost is at most about **3.4 n** comparisons for a random rank (and 2n for the median in the best analysis): O(n) expected, versus O(n log n) to sort first. The deterministic **median of medians** variant guarantees O(n) worst case but with large constants, so practical libraries use quickselect with introspection (fall back to heap-based selection if recursion gets deep).

```c
#include <stdint.h>
static uint64_t rng_state = 0x9E3779B97F4A7C15ULL;
static inline uint64_t rnd64(void) { rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17; return rng_state; }
static inline uint64_t rnd_below(uint64_t n) { uint64_t limit = UINT64_MAX - UINT64_MAX % n, x; do x = rnd64(); while (x >= limit); return x % n; }   // unbiased: reject the uneven tail
static inline double rnd01(void) { return (rnd64() >> 11) * (1.0 / 9007199254740992.0); }
```

```c
// Las Vegas algorithms: always correct, random running time. Randomized quicksort and quickselect with comparison counts.
#include "rnd.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static long comps;
static void swap(int *a, int *b) { int t = *a; *a = *b; *b = t; }
// Lomuto partition around the LAST element: returns the pivot's final position. Distinct keys assumed (equal keys need a 3-way partition).
static int lomuto(int *a, int lo, int hi) {
    int p = a[hi], i = lo;
    for (int j = lo; j < hi; j++) { comps++; if (a[j] < p) swap(&a[i++], &a[j]); }
    swap(&a[i], &a[hi]);
    return i;
}
static void qs_last(int *a, int lo, int hi) {              // deterministic: pivot = last element, quadratic on sorted input
    while (lo < hi) {
        int m = lomuto(a, lo, hi);
        if (m - lo < hi - m) { qs_last(a, lo, m - 1); lo = m + 1; } else { qs_last(a, m + 1, hi); hi = m - 1; }    // recurse on the smaller side
    }
}
static void qs_random(int *a, int lo, int hi) {            // same algorithm, uniformly random pivot
    while (lo < hi) {
        swap(&a[hi], &a[lo + rnd_below(hi - lo + 1)]);
        int m = lomuto(a, lo, hi);
        if (m - lo < hi - m) { qs_random(a, lo, m - 1); lo = m + 1; } else { qs_random(a, m + 1, hi); hi = m - 1; }
    }
}
// quickselect for the k-th smallest (0-based), random pivot: recurse into one side only
static int quickselect(int *a, int n, int k) {
    int lo = 0, hi = n - 1;
    while (lo < hi) {
        swap(&a[hi], &a[lo + rnd_below(hi - lo + 1)]);
        int m = lomuto(a, lo, hi);
        if (k == m) return a[m]; else if (k < m) hi = m - 1; else lo = m + 1;
    }
    return a[lo];
}

int main(void) {
    int n = 20000; int *a = malloc(n * sizeof *a);
    printf("sorting n = %d integers; comparisons made (expected for random pivots: about 2 n ln n = %.0f)\n", n, 2.0 * n * log(n));
    printf("%-34s %14s\n", "input / pivot rule", "comparisons");
    for (int i = 0; i < n; i++) a[i] = i;
    comps = 0; qs_last(a, 0, n - 1);   printf("%-34s %14ld\n", "sorted input, last-element pivot", comps);
    for (int i = 0; i < n; i++) a[i] = i;
    comps = 0; qs_random(a, 0, n - 1); printf("%-34s %14ld\n", "sorted input, random pivot", comps);
    for (int i = 0; i < n; i++) a[i] = n - i;
    comps = 0; qs_random(a, 0, n - 1); printf("%-34s %14ld\n", "reversed input, random pivot", comps);
    long total = 0, worst = 0, best = 1L << 60;
    for (int run = 0; run < 200; run++) { for (int i = 0; i < n; i++) a[i] = i; comps = 0; qs_random(a, 0, n - 1); total += comps; if (comps > worst) worst = comps; if (comps < best) best = comps; }
    printf("200 runs on sorted input, random pivot: mean %ld, min %ld, max %ld  (the running time is random, the answer is always sorted)\n", total / 200, best, worst);
    long tot = 0; int ok = 1;
    for (int run = 0; run < 200; run++) { for (int i = 0; i < n; i++) a[i] = (int)rnd_below(1000000); comps = 0; int k = (int)rnd_below(n);
        int *b = malloc(n * sizeof *b); for (int i = 0; i < n; i++) b[i] = a[i]; qs_random(b, 0, n - 1); long c1 = comps; comps = 0;
        int got = quickselect(a, n, k); if (got != b[k]) ok = 0; tot += comps; (void)c1; free(b); }
    printf("quickselect: 200 random ranks all correct: %s; average %.2f comparisons per element (expected at most 3.4 n for a random rank)\n", ok ? "yes" : "NO", (double)tot / 200 / n);
    free(a); return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra lasvegas.c -o lv -lm && ./lv
sorting n = 20000 integers; comparisons made (expected for random pivots: about 2 n ln n = 396140)
input / pivot rule                    comparisons
sorted input, last-element pivot        199990000
sorted input, random pivot                 338965
reversed input, random pivot               333483
200 runs on sorted input, random pivot: mean 338717, min 310104, max 394197  (the running time is random, the answer is always sorted)
quickselect: 200 random ranks all correct: yes; average 3.04 comparisons per element (expected at most 3.4 n for a random rank)
```

Reading the results:

- Sorted input with a last-element pivot costs **199,990,000** comparisons, exactly n(n−1)/2 for n = 20,000: every partition peels off one element.
- The same input with a random pivot costs about 340,000: within a few percent of the predicted 2n ln n ≈ 396,000 minus lower-order terms, regardless of the input order (sorted and reversed behave alike).
- Over 200 runs the cost varies between roughly 310,000 and 394,000: **the running time is a random variable, but the output is always sorted.** That is what Las Vegas means.
- Quickselect found the correct k-th element in all 200 random trials (checked against a full sort), with about 3 comparisons per element, matching the bound.

**Generating random numbers correctly** is part of the algorithm. `rnd_below(n)` above uses rejection to avoid *modulo bias*: `x % n` for a 64-bit `x` slightly favours small residues unless `n` divides 2⁶⁴, which for large `n` is a measurable skew. Use a good generator (xorshift64, PCG, `getrandom` for security) and never `rand() % n` when quality matters. Shuffling uses **Fisher–Yates** (swap element *i* with a random element of positions 0..i), which produces each permutation with equal probability; the common bug of swapping each element with a random element of the *whole* array makes some permutations more likely than others.

## Monte Carlo: trading certainty for speed

A Monte Carlo algorithm with one-sided error (it can be wrong in only one direction) is cheap to **amplify**: repeat it independently *t* times; if the error probability of one run is at most *p*, the chance that all runs are wrong is at most *p^t*. Twenty repetitions of an algorithm that is wrong half the time give an error of one in a million; at fifty repetitions the failure is rarer than a hardware fault in the computer running it.

### Freivalds: verifying a matrix product

Checking whether `A·B = C` for n×n matrices by recomputing the product costs O(n³) (O(n^2.37) with the best theory). **Freivalds' algorithm** picks a random 0/1 vector `r` and compares `A·(B·r)` with `C·r`: three matrix-vector products, **O(n²)**. If `A·B = C` the test always agrees. If `A·B ≠ C`, the vector `(AB − C)·r` is nonzero with probability at least ½ (a nonzero row of the difference is a linear equation that a random 0/1 vector satisfies with probability at most ½). So the error is one-sided, at most ½ per trial.

### Miller–Rabin: primality testing

Fermat's test (does `a^(n−1) ≡ 1 mod n`?) is fooled by Carmichael numbers such as 561. **Miller–Rabin** writes `n − 1 = 2^s · d` and checks that the sequence `a^d, a^(2d), …, a^((n−1)/2)` either starts with 1 or contains `n − 1`; any other pattern proves `n` composite. A prime always passes. For any odd composite, at most a quarter of the bases `a` are liars, so *k* random bases give error at most 4^−k. (For 64-bit integers a fixed set of 12 bases gives a deterministic, always-correct test; for larger numbers random bases remain the practical choice.)

### Karger: minimum cut by contraction

The **minimum cut** of a graph is the fewest edges whose removal disconnects it. **Karger's algorithm** repeatedly picks a uniformly random edge and *contracts* its endpoints (merges them into one vertex) until two super-vertices remain; the edges between them form a cut. A fixed minimum cut survives all contractions with probability at least `2/(n(n−1))`, because contracting a random edge hits a min-cut edge with small probability when the min cut is small compared with the number of edges. A single run is rarely right for large graphs, but repeating it `O(n² log n)` times finds a minimum cut with high probability, and the Karger–Stein recursion improves the total to roughly `O(n² log³ n)`.

### Reservoir sampling

To pick *k* items uniformly from a stream whose length is unknown, keep the first *k*; then for item number `i` (counting from 0) generate `j` uniformly in `[0, i]` and, if `j < k`, replace the j-th stored item. Each item ends up in the sample with probability exactly `k/n`, using O(k) memory and one pass. (Weighted and distributed variants exist; Vitter's skipping variants avoid generating a random number per item.)

```c
// Monte Carlo algorithms: fast, with a bounded probability of error that repetition drives down. Four classics, each checked against ground truth.
#include "rnd.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- 1. Freivalds: is A*B == C for n x n matrices? O(n^2) per trial instead of O(n^3); error <= 1/2 per trial ---- */
#define N 120
static long long A[N][N], B[N][N], C[N][N];
static int freivalds(int trials) {
    for (int t = 0; t < trials; t++) {
        long long r[N], br[N], cr[N], abr[N];
        for (int i = 0; i < N; i++) r[i] = rnd64() & 1;
        for (int i = 0; i < N; i++) { br[i] = cr[i] = 0; for (int j = 0; j < N; j++) { br[i] += B[i][j] * r[j]; cr[i] += C[i][j] * r[j]; } }
        for (int i = 0; i < N; i++) { abr[i] = 0; for (int j = 0; j < N; j++) abr[i] += A[i][j] * br[j]; }
        for (int i = 0; i < N; i++) if (abr[i] != cr[i]) return 0;          // definitely not equal
    }
    return 1;                                                                // equal with high probability
}

/* ---- 2. Miller-Rabin primality test: error <= 1/4 per random base for composites ---- */
static uint64_t mulmod(uint64_t a, uint64_t b, uint64_t m) { return (unsigned __int128)a * b % m; }
static uint64_t powmod(uint64_t b, uint64_t e, uint64_t m) { uint64_t r = 1; b %= m; while (e) { if (e & 1) r = mulmod(r, b, m); b = mulmod(b, b, m); e >>= 1; } return r; }
static int miller_rabin(uint64_t n, int rounds) {
    if (n < 2) return 0;
    if (n < 4) return 1;
    if (n % 2 == 0) return 0;
    uint64_t d = n - 1; int s = 0; while (d % 2 == 0) { d /= 2; s++; }
    for (int i = 0; i < rounds; i++) {
        uint64_t a = 2 + rnd_below(n - 3), x = powmod(a, d, n);
        if (x == 1 || x == n - 1) continue;
        int composite = 1; for (int r = 1; r < s; r++) { x = mulmod(x, x, n); if (x == n - 1) { composite = 0; break; } }
        if (composite) return 0;
    }
    return 1;
}

/* ---- 3. Karger's randomized minimum cut by random edge contraction: success probability >= 2/(n(n-1)) per run ---- */
#define V 12
static int edges[200][2], m;
static int find(int *p, int x) { while (p[x] != x) x = p[x] = p[p[x]]; return x; }
static int karger_once(void) {
    int p[V]; for (int i = 0; i < V; i++) p[i] = i;
    int comps = V;
    while (comps > 2) { int e = (int)rnd_below(m), a = find(p, edges[e][0]), b = find(p, edges[e][1]); if (a != b) { p[a] = b; comps--; } }
    int cut = 0; for (int e = 0; e < m; e++) if (find(p, edges[e][0]) != find(p, edges[e][1])) cut++;
    return cut;
}

/* ---- 4. Reservoir sampling: a uniform sample of k items from a stream of unknown length in O(k) memory ---- */
static void reservoir(int k, int stream_len, int *res) {
    for (int i = 0; i < stream_len; i++) {
        if (i < k) res[i] = i;
        else { uint64_t j = rnd_below(i + 1); if (j < (uint64_t)k) res[j] = i; }       // item i enters with probability k/(i+1)
    }
}

int main(void) {
    // Freivalds
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) { A[i][j] = rnd_below(10); B[i][j] = rnd_below(10); }
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) { C[i][j] = 0; for (int k = 0; k < N; k++) C[i][j] += A[i][k] * B[k][j]; }
    int ok_right = freivalds(20); C[N / 2][N / 3] += 1;                    // corrupt a single entry
    int fooled_1 = 0, fooled_5 = 0;
    for (int t = 0; t < 4000; t++) { fooled_1 += freivalds(1); fooled_5 += freivalds(5); }
    printf("Freivalds (%dx%d): correct product accepted: %s; a product wrong in ONE entry is wrongly accepted in %.1f%% of single trials (bound 50%%) and %.2f%% of 5-trial runs (bound 3.1%%)\n",
           N, N, ok_right ? "yes" : "NO", fooled_1 / 40.0, fooled_5 / 40.0);

    // Miller-Rabin
    uint64_t primes[] = {1000000007ULL, 998244353ULL, 18446744073709551557ULL};
    int pr_ok = 1; for (int i = 0; i < 3; i++) pr_ok &= miller_rabin(primes[i], 20);
    printf("Miller-Rabin: three known primes (including 2^64-59) accepted: %s; ", pr_ok ? "yes" : "NO");
    // the Carmichael number 561 and strong pseudoprime 3215031751 against single random bases
    int fooled561 = 0, fooledsp = 0; for (int t = 0; t < 20000; t++) { fooled561 += miller_rabin(561, 1); fooledsp += miller_rabin(3215031751ULL, 1); }
    printf("single-base error rate: Carmichael 561 %.2f%%, strong pseudoprime 3215031751 %.2f%% (bound 25%%); with 20 bases both are rejected: %s\n",
           fooled561 / 200.0, fooledsp / 200.0, (!miller_rabin(561, 20) && !miller_rabin(3215031751ULL, 20)) ? "yes" : "NO");

    // Karger on a graph with a known minimum cut: two cliques of 6 vertices joined by 3 edges (minimum cut = 3)
    m = 0; for (int a = 0; a < 6; a++) for (int b = a + 1; b < 6; b++) { edges[m][0] = a; edges[m++][1] = b; edges[m][0] = a + 6; edges[m++][1] = b + 6; }
    edges[m][0] = 0; edges[m++][1] = 6; edges[m][0] = 1; edges[m++][1] = 7; edges[m][0] = 2; edges[m++][1] = 8;
    int hits = 0, runs = 20000; for (int t = 0; t < runs; t++) if (karger_once() == 3) hits++;
    printf("Karger min cut (2 cliques of 6 joined by 3 edges, true min cut 3): one contraction run finds it with probability %.1f%% (guaranteed at least %.1f%%); best of 100 runs finds it in %.1f%% of experiments\n",
           100.0 * hits / runs, 200.0 / (V * (V - 1)), 100.0 * (1 - pow(1 - (double)hits / runs, 100)));

    // Reservoir sampling uniformity
    int k = 5, len = 40, counts[40] = {0}, res[5], trials = 200000;
    for (int t = 0; t < trials; t++) { reservoir(k, len, res); for (int i = 0; i < k; i++) counts[res[i]]++; }
    double expect = (double)trials * k / len, chi = 0, mx = 0;
    for (int i = 0; i < len; i++) { chi += (counts[i] - expect) * (counts[i] - expect) / expect; double dev = fabs(counts[i] - expect) / expect; if (dev > mx) mx = dev; }
    printf("Reservoir sampling (k=5 of a 40-item stream, %d trials): every item selected %.0f times on average; largest deviation %.2f%%; chi-square %.1f with 39 degrees of freedom (a uniform sampler gives about 39, values above ~62 would be suspicious)\n", trials, expect, mx * 100, chi);
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra montecarlo.c -o mc -lm && ./mc
Freivalds (120x120): correct product accepted: yes; a product wrong in ONE entry is wrongly accepted in 50.3% of single trials (bound 50%) and 2.85% of 5-trial runs (bound 3.1%)
Miller-Rabin: three known primes (including 2^64-59) accepted: yes; single-base error rate: Carmichael 561 1.54%, strong pseudoprime 3215031751 24.55% (bound 25%); with 20 bases both are rejected: yes
Karger min cut (2 cliques of 6 joined by 3 edges, true min cut 3): one contraction run finds it with probability 18.8% (guaranteed at least 1.5%); best of 100 runs finds it in 100.0% of experiments
Reservoir sampling (k=5 of a 40-item stream, 200000 trials): every item selected 25000 times on average; largest deviation 1.51%; chi-square 41.6 with 39 degrees of freedom (a uniform sampler gives about 39, values above ~62 would be suspicious)
```

How the measurements line up with the theory:

- **Freivalds**: a product corrupted in a single entry is wrongly accepted by almost exactly 50 % of single trials (the bound is tight when only one entry is wrong) and by about 3 % of 5-trial runs (the bound is (½)⁵ = 3.1 %). The correct product is always accepted.
- **Miller–Rabin**: the Carmichael number 561, which fools Fermat's test for every base coprime to it, is rejected by Miller–Rabin on all but a few percent of single random bases. The strong pseudoprime 3,215,031,751 (composite, but a strong liar for all prime bases up to 11) is mistaken for a prime by 24.5 % of random single bases, essentially the worst-case bound of 25 %. With 20 bases both are rejected, and the 64-bit prime 2⁶⁴ − 59 is correctly accepted.
- **Karger**: on two 6-cliques joined by three edges the minimum cut is 3. One contraction run finds it about 19 % of the time, far above the worst-case guarantee of 2/(12·11) = 1.5 %, because the graph has a clear structure; 100 runs find it essentially always.
- **Reservoir sampling**: over 200,000 trials of sampling 5 of 40 items, every item was selected about 25,000 times with a maximum deviation of about 1.5 %, and the chi-square statistic is close to its expected value of 39 (the number of degrees of freedom).

## More places randomness helps

- **Hashing**: choosing the hash function at random from a universal family defeats adversarial inputs (see the hash table note's flooding experiment); **treaps** and **skip lists** use random priorities and levels to stay balanced without rotations; **Bloom filters** and **count-min sketches** are Monte Carlo data structures (see the probabilistic structures note).
- **Hashing for similarity**: MinHash and locality-sensitive hashing estimate set similarity and find near neighbours.
- **Load balancing**: "power of two choices" (pick two servers at random, use the less loaded) reduces the maximum load from `log n / log log n` to `log log n`.
- **Randomized rounding** and **random sampling** for approximation algorithms; **Monte Carlo simulation** and **MCMC** for numerical estimates (error shrinks as 1/√samples).
- **Property-based testing and fuzzing** are randomized search for counterexamples; seeds must be logged so failures can be reproduced.

## Practical advice

- **Seed deliberately**: a fixed seed gives reproducible runs (essential for tests and debugging); seed from the OS (`getrandom`, `/dev/urandom`) when unpredictability is required, such as hash-table seeds against flooding.
- **Test randomized code statistically**: check correctness always (Las Vegas outputs), measure error rates against the proven bound (Monte Carlo), and use enough trials: a bound violated once in a million runs shows only with millions of runs.
- **Use the right generator**: the C library's `rand()` is low quality and not thread-safe; xorshift or PCG is fine for algorithms; use a cryptographic source for security.
- **Independence matters**: the amplification argument `p^t` needs independent random bits for each repetition; reusing a poor generator with correlated outputs invalidates it.
- **Derandomization** (conditional expectations, pairwise independence, expander walks) turns many randomized algorithms into deterministic ones when guarantees must be absolute, usually at a cost in simplicity.

## Key takeaways

- Las Vegas algorithms are always correct with random running time (randomized quicksort: expected 2n ln n comparisons for every input); Monte Carlo algorithms are fast with a small, controllable error probability.
- One-sided error is amplified exponentially by independent repetition: error ≤ p^t.
- Freivalds checks a matrix product in O(n²), Miller–Rabin tests primality with error ≤ 4^−k, Karger finds a min cut by random contraction, and reservoir sampling draws a uniform sample from a stream in O(k) memory.
- Generate random numbers without modulo bias, shuffle with Fisher–Yates, and verify claimed probabilities experimentally.
