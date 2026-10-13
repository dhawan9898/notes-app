// Nearest-neighbour search cost versus dimension: how many of the n = 200,000 tree nodes does a query visit? (Compile once per dimension.)
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#define D DIM
#include "kd.h"

static uint64_t rng = 0x2545F4914F6CDD1DULL;
static double urand(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (rng >> 11) * (1.0 / 9007199254740992.0); }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

int main(void) {
    int n = 200000, Q = 2000;
    kpt *p = malloc(n * sizeof *p), *raw = malloc(n * sizeof *raw);
    for (int i = 0; i < n; i++) for (int a = 0; a < D; a++) raw[i].c[a] = urand();
    memcpy(p, raw, n * sizeof *p);
    double t0 = now(); kd_build(p, n); double tb = now() - t0;
    long visited = 0; double tq = 0, tbf = 0; double sink = 0;
    for (int q = 0; q < Q; q++) {
        double qq[D]; for (int a = 0; a < D; a++) qq[a] = urand();
        long v; double d; t0 = now(); kd_nearest(p, n, qq, &d, &v); tq += now() - t0; visited += v; sink += d;
        if (q < 100) { t0 = now(); double best = INFINITY; for (int i = 0; i < n; i++) { double e = dist2k(raw[i].c, qq); if (e < best) best = e; } tbf += now() - t0; sink += best; }
    }
    printf("D=%2d  build %.3f s  nodes visited per query: %8.0f of %d (%5.1f%%)   kd-tree %7.1f us/query   brute force %7.1f us/query\n",
           D, tb, (double)visited / Q, n, 100.0 * visited / Q / n, tq / Q * 1e6, tbf / 100 * 1e6);
    if (sink < 0) puts("");
    return 0;
}
