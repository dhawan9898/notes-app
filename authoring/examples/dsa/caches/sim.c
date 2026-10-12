#define _GNU_SOURCE
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "policies.h"

static uint64_t s = 88172645463325252ULL;
static uint64_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
static double urand(void) { return (double)(rnd() >> 11) / 9007199254740992.0; }

/* Zipf sampler by inverse CDF with a precomputed cumulative table and binary search */
static int *make_zipf(size_t n, int NK, double alpha) {
    double *cdf = malloc((size_t)NK * sizeof *cdf), sum = 0; for (int i = 0; i < NK; i++) { sum += 1.0 / pow(i + 1.0, alpha); cdf[i] = sum; }
    int *perm = malloc((size_t)NK * sizeof(int)); for (int i = 0; i < NK; i++) perm[i] = i;
    for (int i = NK - 1; i > 0; i--) { int j = (int)(rnd() % (uint64_t)(i + 1)), t = perm[i]; perm[i] = perm[j]; perm[j] = t; }   /* popular keys are scattered, not 0,1,2... */
    int *tr = malloc(n * sizeof(int));
    for (size_t i = 0; i < n; i++) { double u = urand() * sum; int lo = 0, hi = NK - 1; while (lo < hi) { int m = (lo + hi) / 2; if (cdf[m] < u) lo = m + 1; else hi = m; } tr[i] = perm[lo]; }
    free(cdf); free(perm); return tr;
}

/* hot[i] = 1 marks the requests whose hit ratio is reported (NULL = all requests). */
static void table(const char *title, const int *tr, size_t n, int NK, const int *sizes, int ns, const unsigned char *hot) {
    printf("%s\n%-10s %8s %8s %8s %8s %8s\n", title, "cache size", "FIFO", "CLOCK", "LRU", "LFU", "OPT");
    unsigned char *hf = calloc(n, 1); size_t denom = 0; for (size_t i = 0; i < n; i++) denom += hot ? hot[i] : 1;
    for (int i = 0; i < ns; i++) {
        int c = sizes[i]; double r[5];
        for (int p = 0; p < 5; p++) {
            memset(hf, 0, n);
            ll tot = p == 0 ? sim_fifo(tr, n, NK, c, hf) : p == 1 ? sim_clock(tr, n, NK, c, hf) : p == 2 ? sim_lru(tr, n, NK, c, hf) : p == 3 ? sim_lfu(tr, n, NK, c, hf) : sim_opt(tr, n, NK, c, hf);
            ll counted = 0; for (size_t j = 0; j < n; j++) counted += hf[j] && (!hot || hot[j]); (void)tot;
            r[p] = 100.0 * (double)counted / (double)denom;
        }
        assert(r[4] >= r[0] && r[4] >= r[1] && r[4] >= r[2] && r[4] >= r[3]);   /* Belady's OPT is optimal: no policy may beat it */
        printf("%5d (%2.0f%%) %7.1f%% %7.1f%% %7.1f%% %7.1f%% %7.1f%%\n", c, 100.0 * c / NK, r[0], r[1], r[2], r[3], r[4]);
    }
    printf("\n"); free(hf);
}

int main(void) {
    int NK = 100000; size_t n = 2000000; int sizes[] = { 100, 500, 1000, 5000, 20000 };
    /* sanity: the policies on a tiny hand-checkable trace.  Capacity 3, trace 1 2 3 4 1 2 5 1 2 3 4 5 (Belady's anomaly example for FIFO) */
    { int t[] = { 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5 };
      printf("tiny trace 1 2 3 4 1 2 5 1 2 3 4 5, capacity 3:  FIFO %lld hits, LRU %lld, CLOCK %lld, LFU %lld, OPT %lld;  FIFO with capacity 4: %lld hits (MORE memory, FEWER hits: Belady's anomaly)\n\n",
             sim_fifo(t, 12, 6, 3, NULL), sim_lru(t, 12, 6, 3, NULL), sim_clock(t, 12, 6, 3, NULL), sim_lfu(t, 12, 6, 3, NULL), sim_opt(t, 12, 6, 3, NULL), sim_fifo(t, 12, 6, 4, NULL)); }

    int *z = make_zipf(n, NK, 1.0);
    table("1. Zipf workload (alpha = 1.0, 100,000 keys, 2M requests): hit ratio over all requests", z, n, NK, sizes, 5, NULL);

    /* scan pollution: 70% Zipf requests, 30% a sequential sweep over 100,000 keys that are each touched once per sweep (backup, table scan) */
    int *mix = malloc(n * sizeof(int)); unsigned char *hot = calloc(n, 1); int scan_pos = 0; int NK2 = 2 * NK;
    for (size_t i = 0; i < n; i++) { if (rnd() % 10 < 3) { mix[i] = NK + scan_pos; scan_pos = (scan_pos + 1) % NK; } else { mix[i] = z[i]; hot[i] = 1; } }
    table("2. Scan pollution: 70% Zipf + 30% sequential sweep; hit ratio of the Zipf (useful) requests only", mix, n, NK2, sizes, 5, hot);

    /* loop: cyclic access over a working set slightly larger than the cache */
    int *loop = malloc(n * sizeof(int)); int L = 1100; for (size_t i = 0; i < n; i++) loop[i] = (int)(i % (size_t)L);
    int lsz[] = { 1000, 1100 };
    table("3. Loop over 1,100 keys (working set slightly larger than a 1,000-entry cache)", loop, n, NK, lsz, 2, NULL);
    free(z); free(mix); free(loop); free(hot); return 0;
}
