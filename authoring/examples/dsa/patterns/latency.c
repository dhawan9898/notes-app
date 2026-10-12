#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint64_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }

/* Dependent loads: each load's address comes from the previous load, so latency cannot be hidden.  A random cyclic permutation defeats the prefetcher. */
static double chase_ns(size_t bytes) {
    size_t n = bytes / sizeof(size_t); size_t *a = malloc(n * sizeof *a), *perm = malloc(n * sizeof *perm);
    for (size_t i = 0; i < n; i++) perm[i] = i;
    for (size_t i = n - 1; i > 0; i--) { size_t j = rnd() % (i + 1), t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
    for (size_t i = 0; i < n; i++) a[perm[i]] = perm[(i + 1) % n];                      /* one cycle through all slots */
    size_t steps = 20000000, p = 0; double t0 = now(); for (size_t i = 0; i < steps; i++) p = a[p]; double dt = now() - t0;
    volatile size_t sink = p; (void)sink; free(a); free(perm); return dt / (double)steps * 1e9;
}
static double seq_gbps(size_t bytes) {
    uint64_t *a = malloc(bytes); size_t n = bytes / 8; for (size_t i = 0; i < n; i++) a[i] = i;
    int reps = (int)(2000000000ull / bytes) + 1; uint64_t sum = 0; double t0 = now(); for (int r = 0; r < reps; r++) for (size_t i = 0; i < n; i++) sum += a[i]; double dt = now() - t0;
    volatile uint64_t sink = sum; (void)sink; free(a); return (double)bytes * reps / dt / 1e9;
}

int main(void) {
    printf("%12s | %22s | %20s\n", "working set", "dependent load latency", "sequential read");
    size_t sizes[] = { 16u << 10, 128u << 10, 1u << 20, 8u << 20, 32u << 20, 256u << 20 };
    for (int i = 0; i < 6; i++) { char buf[32]; if (sizes[i] < (1u << 20)) snprintf(buf, sizeof buf, "%zu KiB", sizes[i] >> 10); else snprintf(buf, sizeof buf, "%zu MiB", sizes[i] >> 20); printf("%12s | %19.1f ns | %14.1f GB/s\n", buf, chase_ns(sizes[i]), seq_gbps(sizes[i])); }
    return 0;
}
