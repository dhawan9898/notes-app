// k-d tree nearest neighbour and range counting against brute force, in 2, 3 and 8 dimensions, including duplicate points.
#include <stdio.h>
#include <stdint.h>
#define D DIM
#include "kd.h"

static uint64_t rng = 0x9E3779B97F4A7C15ULL;
static double urand(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (rng >> 11) * (1.0 / 9007199254740992.0); }

int main(void) {
    long checks = 0;
    for (int trial = 0; trial < 300; trial++) {
        int n = 1 + (int)(urand() * 400), grid = (trial % 3 == 0) ? 4 : 0;           // coarse grid => many duplicates and equal coordinates
        kpt *p = malloc(n * sizeof *p), *orig = malloc(n * sizeof *orig);
        for (int i = 0; i < n; i++) { p[i].id = i; for (int a = 0; a < D; a++) p[i].c[a] = grid ? floor(urand() * grid) : urand(); }
        memcpy(orig, p, n * sizeof *p);
        kd_build(p, n);
        for (int q = 0; q < 40; q++) {
            double qq[D]; for (int a = 0; a < D; a++) qq[a] = grid ? urand() * grid : urand();
            double want = INFINITY; for (int i = 0; i < n; i++) { double d = dist2k(orig[i].c, qq); if (d < want) want = d; }
            double got; kd_nearest(p, n, qq, &got, NULL);
            if (got != want) { printf("nearest mismatch D=%d\n", D); return 1; }
            double lo[D], hi[D]; for (int a = 0; a < D; a++) { double x = urand() * (grid ? grid : 1), y = urand() * (grid ? grid : 1); lo[a] = x < y ? x : y; hi[a] = x < y ? y : x; }
            long cnt = 0; for (int i = 0; i < n; i++) { int in = 1; for (int a = 0; a < D; a++) if (orig[i].c[a] < lo[a] || orig[i].c[a] > hi[a]) in = 0; cnt += in; }
            if (kd_range(p, n, lo, hi, NULL) != cnt) { printf("range mismatch D=%d\n", D); return 1; }
            checks += 2;
        }
        free(p); free(orig);
    }
    printf("D=%d: %ld nearest-neighbour and range queries match brute force\n", D, checks);
    return 0;
}
