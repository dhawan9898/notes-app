#define _GNU_SOURCE
#include <stdio.h>
#include "dsu.h"
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    static const char *label[4] = { "naive", "path compression", "union by rank", "both" };
    printf("n unions of random pairs followed by n finds; parent pointers followed per operation, and ns per operation\n");
    printf("%9s | %-18s %12s %10s\n", "n", "heuristics", "steps/op", "ns/op");
    for (int n = 100000; n <= 10000000; n *= 10) {
        int *a = malloc((size_t)n * sizeof *a), *b = malloc((size_t)n * sizeof *b), *f = malloc((size_t)n * sizeof *f);
        for (int i = 0; i < n; i++) { a[i] = (int)(rnd() % (unsigned)n); b[i] = (int)(rnd() % (unsigned)n); f[i] = (int)(rnd() % (unsigned)n); }
        for (int mode = 0; mode < 4; mode++) {
            if (mode == DSU_NAIVE && n > 100000) { printf("%9d | %-18s %12s %10s\n", n, label[mode], "(skipped)", "-"); continue; }
            DSU d; dsu_init(&d, n, mode); long sink = 0; double t0 = now();
            for (int i = 0; i < n; i++) dsu_union(&d, a[i], b[i]);
            for (int i = 0; i < n; i++) sink += dsu_find(&d, f[i]);
            double dt = now() - t0;
            printf("%9d | %-18s %12.2f %10.0f\n", n, label[mode], (double)d.steps / (2.0 * n), dt / (2.0 * n) * 1e9);
            dsu_free(&d); if (sink == 42) puts("");
        }
        free(a); free(b); free(f);
    }
    /* adversarial order for naive linking: union(i+1, i) hangs the old tree below a new singleton root, so node 0 ends up n-1 deep */
    printf("\nchain-building order union(i+1, i), then 1000 finds of node 0 (the deepest node):\n");
    for (int mode = 0; mode < 4; mode++) {
        int n = 100000; DSU d; dsu_init(&d, n, mode);
        for (int i = 0; i + 1 < n; i++) dsu_union(&d, i + 1, i);
        d.steps = 0; for (int i = 0; i < 1000; i++) dsu_find(&d, 0);
        printf("  %-18s %10.1f pointer steps per find\n", label[mode], (double)d.steps / 1000.0);
        dsu_free(&d);
    }
    return 0;
}
