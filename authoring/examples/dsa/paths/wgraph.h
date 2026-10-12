#ifndef WGRAPH_H
#define WGRAPH_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int u, v, w; } WEdge;
typedef struct { int n; size_t m; int *off, *to, *w; } WCSR;                 /* weighted CSR */
#define INF 0x3f3f3f3f3f3f3f3fLL

static inline WCSR wcsr_build(int n, const WEdge *e, size_t m, int directed) {
    WCSR g = { n, directed ? m : 2 * m, calloc((size_t)n + 1, sizeof(int)), NULL, NULL };
    for (size_t i = 0; i < m; i++) { g.off[e[i].u + 1]++; if (!directed) g.off[e[i].v + 1]++; }
    for (int i = 0; i < n; i++) g.off[i + 1] += g.off[i];
    g.to = malloc(g.m * sizeof(int)); g.w = malloc(g.m * sizeof(int)); int *fill = malloc((size_t)n * sizeof(int)); memcpy(fill, g.off, (size_t)n * sizeof(int));
    for (size_t i = 0; i < m; i++) {
        int p = fill[e[i].u]++; g.to[p] = e[i].v; g.w[p] = e[i].w;
        if (!directed) { p = fill[e[i].v]++; g.to[p] = e[i].u; g.w[p] = e[i].w; }
    }
    free(fill); return g;
}
static inline void wcsr_free(WCSR *g) { free(g->off); free(g->to); free(g->w); }
#endif
