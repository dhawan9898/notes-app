#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

/* ---------- brute force: try every permutation of cities 1..n-1 (fix city 0 as start): (n-1)! tours ---------- */
static int best_len, nn; static int *dm;
static void permute(int *p, int k, int len_so_far) {
    if (len_so_far >= best_len) return;                                          /* branch and bound: this partial tour is already too long */
    if (k == nn) { int total = len_so_far + dm[p[nn - 1] * nn + p[0]]; if (total < best_len) best_len = total; return; }
    for (int i = k; i < nn; i++) { int t = p[k]; p[k] = p[i]; p[i] = t; permute(p, k + 1, len_so_far + dm[p[k - 1] * nn + p[k]]); t = p[k]; p[k] = p[i]; p[i] = t; }
}
static int tsp_brute(int n, int *d) {
    nn = n; dm = d; best_len = 1 << 30;
    int p[16];
    for (int i = 0; i < n; i++) p[i] = i;
    permute(p, 1, 0);
    return best_len;
}

/* ---------- Held-Karp: dp[mask][j] = shortest path starting at city 0, visiting exactly the cities in `mask`, ending at j.  O(2^n n^2) time, O(2^n n) memory ---------- */
static int held_karp(int n, const int *d) {
    size_t full = (size_t)1 << n; int *dp = malloc(full * (size_t)n * sizeof(int));
    for (size_t i = 0; i < full * (size_t)n; i++) dp[i] = 1 << 29;
    dp[1 * n + 0] = 0;                                                          /* only city 0 visited, standing at city 0 */
    for (size_t mask = 1; mask < full; mask++) {
        if (!(mask & 1)) continue;
        for (int j = 0; j < n; j++) {
            int cur = dp[mask * n + j]; if (cur >= 1 << 29 || !(mask >> j & 1)) continue;
            for (int k = 1; k < n; k++) if (!(mask >> k & 1)) { size_t nm = mask | ((size_t)1 << k); int v = cur + d[j * n + k]; if (v < dp[nm * n + k]) dp[nm * n + k] = v; }
        }
    }
    int best = 1 << 29; for (int j = 1; j < n; j++) if (dp[(full - 1) * n + j] + d[j * n + 0] < best) best = dp[(full - 1) * n + j] + d[j * n + 0];
    free(dp); return best;
}

int main(void) {
    /* correctness: Held-Karp equals brute force on random complete graphs (asymmetric distances allowed) */
    for (int round = 0; round < 300; round++) {
        int n = 2 + (int)(rnd() % 7); int d[64]; for (int i = 0; i < n * n; i++) d[i] = 1 + (int)(rnd() % 50);
        for (int i = 0; i < n; i++) d[i * n + i] = 0;
        int hk = held_karp(n, d), bf = tsp_brute(n, d); assert(hk == bf);
    }
    printf("300 random instances with 2-8 cities: Held-Karp bitmask DP == exhaustive search over all tours\n\n");
    printf("%4s %16s %12s %14s %12s\n", "n", "(n-1)! tours", "brute+bound ms", "Held-Karp ms", "table memory");
    for (int n = 8; n <= 21; n += (n < 12 ? 1 : (n < 16 ? 2 : 1))) {
        int *d = malloc((size_t)n * n * sizeof *d); for (int i = 0; i < n * n; i++) d[i] = 10 + (int)(rnd() % 90); for (int i = 0; i < n; i++) d[i * n + i] = 0;
        double fact = 1; for (int k = 2; k < n; k++) fact *= k;
        double tb = -1; if (n <= 12) { double t0 = now(); int b = tsp_brute(n, d); tb = now() - t0; (void)b; }
        double t0 = now(); int hk = held_karp(n, d); double th = now() - t0; (void)hk;
        if (tb >= 0) printf("%4d %16.3g %12.2f %14.2f %9.1f MB\n", n, fact, tb * 1e3, th * 1e3, (double)((size_t)1 << n) * n * 4 / 1e6);
        else printf("%4d %16.3g %12s %14.2f %9.1f MB\n", n, fact, "(not run)", th * 1e3, (double)((size_t)1 << n) * n * 4 / 1e6);
        free(d); if (th > 20) break;
    }
    return 0;
}
