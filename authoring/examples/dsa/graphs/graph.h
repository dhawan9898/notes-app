#ifndef GRAPH_H
#define GRAPH_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int u, v; } Edge;

/* ---------- CSR (compressed sparse row): two arrays, edges of vertex u are adj[off[u] .. off[u+1]) ---------- */
typedef struct { int n; size_t m; int *off; int *adj; } CSR;

static inline CSR csr_build(int n, const Edge *e, size_t m, int directed) {
    CSR g = { n, directed ? m : 2 * m, calloc((size_t)n + 1, sizeof(int)), NULL };
    for (size_t i = 0; i < m; i++) { g.off[e[i].u + 1]++; if (!directed) g.off[e[i].v + 1]++; }          /* count out-degrees */
    for (int i = 0; i < n; i++) g.off[i + 1] += g.off[i];                                                  /* prefix sums -> offsets */
    g.adj = malloc((g.m ? g.m : 1) * sizeof(int)); int *fill = malloc((size_t)n * sizeof(int)); memcpy(fill, g.off, (size_t)n * sizeof(int));
    for (size_t i = 0; i < m; i++) { g.adj[fill[e[i].u]++] = e[i].v; if (!directed) g.adj[fill[e[i].v]++] = e[i].u; }
    free(fill); return g;
}
static inline void csr_free(CSR *g) { free(g->off); free(g->adj); }

/* ---------- classic linked adjacency list: one malloc'd node per directed edge ---------- */
typedef struct ANode { int v; struct ANode *next; } ANode;
typedef struct { int n; ANode **head; ANode *pool; size_t used; } AList;

static inline AList alist_build(int n, const Edge *e, size_t m, int directed, int pool_alloc) {
    AList g = { n, calloc((size_t)n, sizeof(ANode *)), NULL, 0 };
    if (pool_alloc) g.pool = malloc((directed ? m : 2 * m) * sizeof(ANode));                     /* nodes allocated contiguously in insertion order */
    for (size_t i = 0; i < m; i++) for (int d = 0; d < (directed ? 1 : 2); d++) {
        int u = d ? e[i].v : e[i].u, v = d ? e[i].u : e[i].v;
        ANode *x = pool_alloc ? &g.pool[g.used++] : malloc(sizeof *x);
        x->v = v; x->next = g.head[u]; g.head[u] = x;
    }
    return g;
}
static inline void alist_free(AList *g, int pool_alloc) {
    if (pool_alloc) free(g->pool);
    else for (int u = 0; u < g->n; u++) for (ANode *x = g->head[u], *nx; x; x = nx) { nx = x->next; free(x); }
    free(g->head);
}

/* ---------- BFS on CSR: queue of vertices, dist[] doubles as the visited set ---------- */
static inline void bfs(const CSR *g, int src, int *dist, int *queue) {
    for (int i = 0; i < g->n; i++) dist[i] = -1;
    int h = 0, t = 0; queue[t++] = src; dist[src] = 0;
    while (h < t) {
        int u = queue[h++];
        for (int k = g->off[u]; k < g->off[u + 1]; k++) { int v = g->adj[k]; if (dist[v] < 0) { dist[v] = dist[u] + 1; queue[t++] = v; } }
    }
}
#endif
