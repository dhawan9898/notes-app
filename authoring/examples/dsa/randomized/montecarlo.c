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
