#include <assert.h>
#include <stdio.h>
#include "sssp.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    int tested = 0, neg_found = 0;
    for (int round = 0; round < 2000; round++) {
        int n = 2 + (int)(rnd() % 12), m = (int)(rnd() % (unsigned)(4 * n)); int directed = (int)(rnd() & 1);
        WEdge *e = malloc((size_t)(m + 1) * sizeof *e);
        for (int i = 0; i < m; i++) e[i] = (WEdge){ (int)(rnd() % (unsigned)n), (int)(rnd() % (unsigned)n), (int)(rnd() % 20) };      /* non-negative weights */
        WCSR g = wcsr_build(n, e, (size_t)m, directed);
        ll *fw = malloc((size_t)n * (size_t)n * sizeof *fw);
        for (int i = 0; i < n * n; i++) fw[i] = INF;
        for (int i = 0; i < n; i++) fw[i * n + i] = 0;
        for (int i = 0; i < m; i++) { if (e[i].w < fw[e[i].u * n + e[i].v]) fw[e[i].u * n + e[i].v] = e[i].w; if (!directed && e[i].w < fw[e[i].v * n + e[i].u]) fw[e[i].v * n + e[i].u] = e[i].w; }
        floyd(n, fw);
        for (int src = 0; src < n; src++) {
            ll d1[16], d2[16], d3[16]; int p[16]; size_t mh;
            dijkstra_indexed(&g, src, d1, p); dijkstra_lazy(&g, src, d2, p, &mh);
            WEdge *ee = malloc((size_t)(2 * m + 1) * sizeof *ee); size_t mm = 0;
            for (int i = 0; i < m; i++) { ee[mm++] = e[i]; if (!directed) ee[mm++] = (WEdge){ e[i].v, e[i].u, e[i].w }; }
            assert(bellman_ford(n, ee, mm, src, d3)); free(ee);
            for (int j = 0; j < n; j++) { assert(d1[j] == fw[src * n + j]); assert(d2[j] == d1[j]); assert(d3[j] == d1[j]); }
            tested++;
        }
        /* 0-1 BFS against Dijkstra on the same topology with 0/1 weights */
        WEdge *z = malloc((size_t)(m + 1) * sizeof *z);
        for (int i = 0; i < m; i++) z[i] = (WEdge){ e[i].u, e[i].v, (int)(rnd() & 1) };
        WCSR gz = wcsr_build(n, z, (size_t)m, directed); ll a[16], b[16]; int p[16]; dijkstra_indexed(&gz, 0, a, p); bfs01(&gz, 0, b);
        for (int j = 0; j < n; j++) assert(a[j] == b[j]);
        free(z); wcsr_free(&gz);
        /* negative edges: Bellman-Ford must agree with Floyd-Warshall when there is no negative cycle, and flag one when there is */
        WEdge *ng = malloc((size_t)(m + 1) * sizeof *ng);
        for (int i = 0; i < m; i++) ng[i] = (WEdge){ e[i].u, e[i].v, (int)(rnd() % 25) - 5 };
        ll *f2 = malloc((size_t)n * (size_t)n * sizeof *f2);
        for (int i = 0; i < n * n; i++) f2[i] = INF;
        for (int i = 0; i < n; i++) f2[i * n + i] = 0;
        for (int i = 0; i < m; i++) if (ng[i].w < f2[ng[i].u * n + ng[i].v]) f2[ng[i].u * n + ng[i].v] = ng[i].w;
        floyd(n, f2); int has_neg_cycle = 0; for (int i = 0; i < n; i++) if (f2[i * n + i] < 0) has_neg_cycle = 1;
        ll db[16]; int ok = bellman_ford(n, ng, (size_t)m, 0, db);
        int reach_neg = 0; for (int i = 0; i < n; i++) if (f2[i * n + i] < 0 && f2[0 * n + i] < INF) reach_neg = 1;
        assert(ok == !reach_neg); (void)has_neg_cycle; neg_found += !ok;
        if (ok) for (int j = 0; j < n; j++) assert(db[j] == f2[j]);
        free(ng); free(f2); free(fw); free(e); wcsr_free(&g);
    }
    printf("2000 random graphs (%d single-source runs): Dijkstra (indexed and lazy), Bellman-Ford and Floyd-Warshall agree; 0-1 BFS matches Dijkstra;\n", tested);
    printf("Bellman-Ford with negative edges matched Floyd-Warshall and flagged a reachable negative cycle in %d graphs\n", neg_found);
    return 0;
}
