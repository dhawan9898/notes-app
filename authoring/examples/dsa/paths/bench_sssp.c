#define _GNU_SOURCE
#include <stdio.h>
#include "sssp.h"
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

static void run(const char *label, const WCSR *g) {
    ll *d1 = malloc((size_t)g->n * sizeof *d1), *d2 = malloc((size_t)g->n * sizeof *d2); int *p = malloc((size_t)g->n * sizeof *p); size_t maxh;
    double t0 = now(); dijkstra_indexed(g, 0, d1, p); double ti = now() - t0;
    t0 = now(); dijkstra_lazy(g, 0, d2, p, &maxh); double tl = now() - t0;
    ll far = 0; int reached = 0; for (int i = 0; i < g->n; i++) { if (d1[i] != d2[i]) { printf("MISMATCH at %d\n", i); return; } if (d1[i] < INF) { reached++; if (d1[i] > far) far = d1[i]; } }
    printf("%-28s %9d %10zu | %9.0f %9.0f | %10zu (%.1fx n) | farthest %lld\n", label, g->n, g->m, ti * 1e3, tl * 1e3, maxh, (double)maxh / g->n, far);
    free(d1); free(d2); free(p);
}

int main(void) {
    printf("%-28s %9s %10s | %9s %9s | %-22s\n", "graph", "vertices", "arcs", "indexed ms", "lazy ms", "lazy: max heap size");
    for (int k = 0; k < 2; k++) {
        int n = 1000000; size_t m = k ? 4000000 : 8000000; WEdge *e = malloc(m * sizeof *e);
        for (size_t i = 0; i < m; i++) e[i] = (WEdge){ (int)(rnd() % (unsigned)n), (int)(rnd() % (unsigned)n), 1 + (int)(rnd() % 100) };
        WCSR g = wcsr_build(n, e, m, 0); run(k ? "random sparse (avg deg 8)" : "random denser (avg deg 16)", &g); wcsr_free(&g); free(e);
    }
    int W = 1000, H = 1000; size_t m = 0; WEdge *e = malloc((size_t)W * H * 2 * sizeof *e);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { int u = y * W + x; if (x + 1 < W) e[m++] = (WEdge){ u, u + 1, 1 + (int)(rnd() % 9) }; if (y + 1 < H) e[m++] = (WEdge){ u, u + W, 1 + (int)(rnd() % 9) }; }
    WCSR g = wcsr_build(W * H, e, m, 0); run("1000x1000 grid (road-like)", &g); wcsr_free(&g); free(e);
    return 0;
}
