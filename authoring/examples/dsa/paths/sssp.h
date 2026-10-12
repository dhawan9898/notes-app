#ifndef SSSP_H
#define SSSP_H
#include "wgraph.h"
#include "../heaps/indexed.h"

typedef long long ll;

/* ===== Dijkstra #1: indexed heap with decrease-key (heap never holds more than n entries) ===== */
static inline void dijkstra_indexed(const WCSR *g, int src, ll *dist, int *pred) {
    for (int i = 0; i < g->n; i++) { dist[i] = INF; pred[i] = -1; }
    IPQ q; ipq_init(&q, g->n); dist[src] = 0; ipq_insert(&q, src, 0);
    while (q.n) {
        int u = ipq_extract(&q);
        for (int k = g->off[u]; k < g->off[u + 1]; k++) {
            int v = g->to[k]; ll nd = dist[u] + g->w[k];
            if (nd < dist[v]) { int fresh = dist[v] == INF; dist[v] = nd; pred[v] = u; if (fresh) ipq_insert(&q, v, nd); else ipq_decrease(&q, v, nd); }
        }
    }
    ipq_free(&q);
}

/* ===== Dijkstra #2: "lazy deletion" - push duplicates, skip stale entries when popped ===== */
typedef struct { ll d; int v; } HE;
static inline void lazy_up(HE *h, size_t i) { HE x = h[i]; while (i && h[(i - 1) / 2].d > x.d) { h[i] = h[(i - 1) / 2]; i = (i - 1) / 2; } h[i] = x; }
static inline void lazy_down(HE *h, size_t n, size_t i) {
    HE x = h[i];
    for (;;) { size_t c = 2 * i + 1; if (c >= n) break; if (c + 1 < n && h[c + 1].d < h[c].d) c++; if (x.d <= h[c].d) break; h[i] = h[c]; i = c; }
    h[i] = x;
}
static inline void dijkstra_lazy(const WCSR *g, int src, ll *dist, int *pred, size_t *max_heap) {
    for (int i = 0; i < g->n; i++) { dist[i] = INF; pred[i] = -1; }
    size_t cap = g->m + 1, n = 0; HE *h = malloc(cap * sizeof *h); dist[src] = 0; h[n++] = (HE){ 0, src }; *max_heap = 1;
    while (n) {
        HE top = h[0]; h[0] = h[--n]; if (n) lazy_down(h, n, 0);
        if (top.d > dist[top.v]) continue;                                   /* stale: a shorter path was already settled */
        for (int k = g->off[top.v]; k < g->off[top.v + 1]; k++) {
            int v = g->to[k]; ll nd = top.d + g->w[k];
            if (nd < dist[v]) { dist[v] = nd; pred[v] = top.v; h[n] = (HE){ nd, v }; lazy_up(h, n++); if (n > *max_heap) *max_heap = n; }
        }
    }
    free(h);
}

/* ===== Bellman-Ford: relax every edge n-1 times; one more pass that still relaxes proves a reachable negative cycle ===== */
static inline int bellman_ford(int n, const WEdge *e, size_t m, int src, ll *dist) {
    for (int i = 0; i < n; i++) dist[i] = INF;
    dist[src] = 0;
    for (int pass = 0; pass < n; pass++) {
        int changed = 0;
        for (size_t i = 0; i < m; i++) if (dist[e[i].u] < INF && dist[e[i].u] + e[i].w < dist[e[i].v]) { dist[e[i].v] = dist[e[i].u] + e[i].w; changed = 1; }
        if (!changed) return 1;                                              /* converged early */
        if (pass == n - 1) return 0;                                         /* still changing after n-1 passes: negative cycle */
    }
    return 1;
}

/* ===== Floyd-Warshall: all pairs in O(n^3), also handles negative edges (not negative cycles) ===== */
static inline void floyd(int n, ll *d) {                                     /* d is n*n, initialised with edge weights, INF, 0 on the diagonal */
    for (int k = 0; k < n; k++) for (int i = 0; i < n; i++) { ll dik = d[(size_t)i * n + k]; if (dik >= INF) continue;
        for (int j = 0; j < n; j++) { ll dkj = d[(size_t)k * n + j]; if (dkj >= INF) continue; ll t = dik + dkj; if (t < d[(size_t)i * n + j]) d[(size_t)i * n + j] = t; } }
}

/* ===== 0-1 BFS: weights are 0 or 1 -> a deque instead of a heap, O(V+E) ===== */
static inline void bfs01(const WCSR *g, int src, ll *dist) {
    int n = g->n, cap = 2 * n + 4 + (int)g->m; int *dq = malloc((size_t)(2 * cap) * sizeof(int)); int head = cap, tail = cap;   /* grows both ways from the middle */
    for (int i = 0; i < n; i++) dist[i] = INF;
    dist[src] = 0; dq[tail++] = src;
    while (head < tail) {
        int u = dq[head++];
        for (int k = g->off[u]; k < g->off[u + 1]; k++) {
            int v = g->to[k]; ll nd = dist[u] + g->w[k];
            if (nd < dist[v]) { dist[v] = nd; if (g->w[k] == 0) dq[--head] = v; else dq[tail++] = v; }
        }
    }
    free(dq);
}
#endif
