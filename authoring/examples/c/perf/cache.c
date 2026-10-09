#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "timing.h"

#define N 4096                              /* 4096 x 4096 ints = 64 MiB */
static int a[N][N];

int main(void) {
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) a[i][j] = i ^ j;

    double t0 = now(); long s1 = 0;
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) s1 += a[i][j];      /* row-major: sequential addresses */
    double t1 = now(); long s2 = 0;
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) s2 += a[i][j];      /* column-major: stride of 16 KiB */
    double t2 = now();
    DO_NOT_OPTIMIZE(s1); DO_NOT_OPTIMIZE(s2);
    printf("sum by rows    : %7.1f ms\nsum by columns : %7.1f ms  (%.1fx slower, same result: %s)\n", (t1 - t0) * 1e3, (t2 - t1) * 1e3, (t2 - t1) / (t1 - t0), s1 == s2 ? "yes" : "NO");

    /* working-set size vs time per access: random pointer chasing through arrays of growing size */
    printf("\nrandom access latency by working-set size (pointer chasing):\n");
    for (size_t kb = 4; kb <= 262144; kb *= 8) {
        size_t n = kb * 1024 / sizeof(size_t);
        size_t *next = malloc(n * sizeof *next), *perm = malloc(n * sizeof *perm);
        for (size_t i = 0; i < n; i++) perm[i] = i;
        uint64_t r = 88172645463325252ULL;
        for (size_t i = n - 1; i > 0; i--) { r ^= r << 13; r ^= r >> 7; r ^= r << 17; size_t j = r % (i + 1); size_t t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
        for (size_t i = 0; i < n; i++) next[perm[i]] = perm[(i + 1) % n];       /* one big cycle in random order */
        size_t p = 0; size_t steps = 20000000;
        double s = now();
        for (size_t i = 0; i < steps; i++) p = next[p];
        double dt = now() - s;
        DO_NOT_OPTIMIZE(p);
        printf("  %7zu KiB : %6.2f ns per access\n", kb, dt / steps * 1e9);
        free(next); free(perm);
    }
    return 0;
}
