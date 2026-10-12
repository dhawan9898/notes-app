#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dsu.h"
#include "../paths/wgraph.h"
#include "../../c/perf/timing.h"

typedef long long ll;
static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

/* ---------- Kruskal: sort edges by weight, add an edge unless it closes a cycle (DSU) ---------- */
static int cmp_w(const void *a, const void *b) { return ((const WEdge *)a)->w - ((const WEdge *)b)->w; }
static ll kruskal(int n, WEdge *e, size_t m, size_t *used) {
    qsort(e, m, sizeof *e, cmp_w);
    DSU d; dsu_init(&d, n, DSU_BOTH); ll total = 0; *used = 0;
    for (size_t i = 0; i < m && *used < (size_t)n - 1; i++) if (dsu_union(&d, e[i].u, e[i].v)) { total += e[i].w; (*used)++; }
    dsu_free(&d); return total;
}

/* ---------- Prim: grow one tree from vertex 0, always adding the cheapest edge leaving it (lazy binary heap of (weight, vertex)) ---------- */
typedef struct { int w, v; } PE;
static void pup(PE *h, size_t i) { PE x = h[i]; while (i && h[(i - 1) / 2].w > x.w) { h[i] = h[(i - 1) / 2]; i = (i - 1) / 2; } h[i] = x; }
static void pdown(PE *h, size_t n, size_t i) {
    PE x = h[i];
    for (;;) { size_t c = 2 * i + 1; if (c >= n) break; if (c + 1 < n && h[c + 1].w < h[c].w) c++; if (x.w <= h[c].w) break; h[i] = h[c]; i = c; }
    h[i] = x;
}
static ll prim(const WCSR *g, size_t *used) {
    char *in = calloc((size_t)g->n, 1); PE *h = malloc((g->m + 1) * sizeof *h); size_t n = 0; ll total = 0; *used = 0;
    in[0] = 1; for (int k = g->off[0]; k < g->off[1]; k++) { h[n] = (PE){ g->w[k], g->to[k] }; pup(h, n++); }
    while (n && *used < (size_t)g->n - 1) {
        PE top = h[0]; h[0] = h[--n]; if (n) pdown(h, n, 0);
        if (in[top.v]) continue;                                        /* stale: this vertex joined through a cheaper edge already */
        in[top.v] = 1; total += top.w; (*used)++;
        for (int k = g->off[top.v]; k < g->off[top.v + 1]; k++) if (!in[g->to[k]]) { h[n] = (PE){ g->w[k], g->to[k] }; pup(h, n++); }
    }
    free(in); free(h); return total;
}

/* ---------- brute force for tiny graphs: try every subset of n-1 edges ---------- */
static ll brute_mst(int n, const WEdge *e, int m) {
    ll best = -1;
    for (unsigned mask = 0; mask < (1u << m); mask++) {
        if (__builtin_popcount(mask) != n - 1) continue;
        DSU d; dsu_init(&d, n, DSU_BOTH); int ok = 1; ll total = 0;
        for (int i = 0; i < m && ok; i++) if (mask >> i & 1) { if (!dsu_union(&d, e[i].u, e[i].v)) ok = 0; total += e[i].w; }
        dsu_free(&d); if (ok && (best < 0 || total < best)) best = total;
    }
    return best;
}

int main(void) {
    for (int round = 0; round < 1500; round++) {
        int n = 2 + (int)(rnd() % 6), extra = (int)(rnd() % 7), m = n - 1 + extra; WEdge e[16], ek[16];
        for (int i = 1; i < n; i++) e[i - 1] = (WEdge){ (int)(rnd() % (unsigned)i), i, 1 + (int)(rnd() % 12) };       /* a random spanning tree keeps the graph connected */
        for (int i = n - 1; i < m; i++) e[i] = (WEdge){ (int)(rnd() % (unsigned)n), (int)(rnd() % (unsigned)n), 1 + (int)(rnd() % 12) };
        memcpy(ek, e, sizeof e); WCSR g = wcsr_build(n, e, (size_t)m, 0); size_t u1, u2;
        ll k = kruskal(n, ek, (size_t)m, &u1), p = prim(&g, &u2), b = brute_mst(n, e, m);
        assert(k == b && p == b && u1 == (size_t)n - 1 && u2 == (size_t)n - 1);
        wcsr_free(&g);
    }
    printf("1500 random small graphs: Kruskal and Prim both equal the brute-force minimum spanning tree weight\n\n");

    printf("%-34s %10s %10s | %10s %10s\n", "graph", "vertices", "edges", "Kruskal ms", "Prim ms");
    for (int k = 0; k < 2; k++) {
        int n = 1000000; size_t m = k ? 2000000 : 8000000; WEdge *e = malloc(m * sizeof *e), *e2 = malloc(m * sizeof *e2);
        for (int i = 1; i < n; i++) e[i - 1] = (WEdge){ (int)(rnd() % (unsigned)i), i, 1 + (int)(rnd() % 1000000) };
        for (size_t i = (size_t)n - 1; i < m; i++) e[i] = (WEdge){ (int)(rnd() % (unsigned)n), (int)(rnd() % (unsigned)n), 1 + (int)(rnd() % 1000000) };
        memcpy(e2, e, m * sizeof *e); size_t u; double t0 = now();
        ll kw = kruskal(n, e2, m, &u); double tk = now() - t0;
        WCSR g = wcsr_build(n, e, m, 0); t0 = now(); ll pw = prim(&g, &u); double tp = now() - t0;
        assert(kw == pw);
        printf("%-34s %10d %10zu | %10.0f %10.0f   (MST weight %lld)\n", k ? "random, 2 edges per vertex" : "random, 8 edges per vertex", n, m, tk * 1e3, tp * 1e3, kw);
        wcsr_free(&g); free(e); free(e2);
    }
    return 0;
}
