#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"

/* ===== DFS, recursive ===== */
static void dfs_rec(const CSR *g, int u, char *seen, int *order, int *cnt) {
    seen[u] = 1; order[(*cnt)++] = u;
    for (int k = g->off[u]; k < g->off[u + 1]; k++) if (!seen[g->adj[k]]) dfs_rec(g, g->adj[k], seen, order, cnt);
}
/* ===== DFS, iterative with explicit stack and a per-vertex "next edge" cursor: IDENTICAL visiting order to the recursive one ===== */
static void dfs_iter(const CSR *g, int src, char *seen, int *order, int *cnt) {
    int *stack = malloc((size_t)g->n * sizeof(int)), *cur = malloc((size_t)g->n * sizeof(int)), sp = 0;
    seen[src] = 1; order[(*cnt)++] = src; stack[sp++] = src; cur[src] = g->off[src];
    while (sp) {
        int u = stack[sp - 1];
        if (cur[u] == g->off[u + 1]) { sp--; continue; }                  /* u is finished: pop (post-order moment) */
        int v = g->adj[cur[u]++];
        if (!seen[v]) { seen[v] = 1; order[(*cnt)++] = v; cur[v] = g->off[v]; stack[sp++] = v; }
    }
    free(stack); free(cur);
}

/* ===== connected components (undirected) by BFS ===== */
static int components(const CSR *g, int *comp) {
    int *q = malloc((size_t)g->n * sizeof(int)), c = 0; for (int i = 0; i < g->n; i++) comp[i] = -1;
    for (int s = 0; s < g->n; s++) if (comp[s] < 0) {
        int h = 0, t = 0; q[t++] = s; comp[s] = c;
        while (h < t) { int u = q[h++]; for (int k = g->off[u]; k < g->off[u + 1]; k++) if (comp[g->adj[k]] < 0) { comp[g->adj[k]] = c; q[t++] = g->adj[k]; } }
        c++;
    }
    free(q); return c;
}
/* ===== bipartite check = 2-colouring by BFS; an odd cycle is the only obstruction ===== */
static int bipartite(const CSR *g) {
    int *col = malloc((size_t)g->n * sizeof(int)), *q = malloc((size_t)g->n * sizeof(int)), ok = 1; for (int i = 0; i < g->n; i++) col[i] = -1;
    for (int s = 0; s < g->n && ok; s++) if (col[s] < 0) {
        int h = 0, t = 0; q[t++] = s; col[s] = 0;
        while (h < t && ok) { int u = q[h++]; for (int k = g->off[u]; k < g->off[u + 1]; k++) { int v = g->adj[k]; if (col[v] < 0) { col[v] = 1 - col[u]; q[t++] = v; } else if (col[v] == col[u]) { ok = 0; break; } } }
    }
    free(col); free(q); return ok;
}
/* ===== topological sort, Kahn's algorithm: repeatedly remove vertices of in-degree 0; leftover vertices => cycle ===== */
static int topo_kahn(const CSR *g, int *order) {
    int *indeg = calloc((size_t)g->n, sizeof(int)); for (size_t k = 0; k < g->m; k++) indeg[g->adj[k]]++;
    int h = 0, t = 0; for (int i = 0; i < g->n; i++) if (!indeg[i]) order[t++] = i;
    while (h < t) { int u = order[h++]; for (int k = g->off[u]; k < g->off[u + 1]; k++) if (--indeg[g->adj[k]] == 0) order[t++] = g->adj[k]; }
    free(indeg); return t == g->n;                                         /* 1 = DAG, order is valid */
}
/* ===== directed cycle detection with three colours (white/grey/black) and reverse post-order = topological order ===== */
static int topo_dfs(const CSR *g, int *order) {
    int n = g->n; char *colour = calloc((size_t)n, 1); int *stack = malloc((size_t)n * sizeof(int)), *cur = malloc((size_t)n * sizeof(int)), pos = n, ok = 1;
    for (int s = 0; s < n && ok; s++) if (!colour[s]) {
        int sp = 0; stack[sp++] = s; colour[s] = 1; cur[s] = g->off[s];
        while (sp && ok) {
            int u = stack[sp - 1];
            if (cur[u] == g->off[u + 1]) { colour[u] = 2; order[--pos] = u; sp--; continue; }       /* finished: prepend to the order */
            int v = g->adj[cur[u]++];
            if (colour[v] == 1) { ok = 0; break; }                         /* edge to a vertex still on the stack: back edge = cycle */
            if (colour[v] == 0) { colour[v] = 1; cur[v] = g->off[v]; stack[sp++] = v; }
        }
    }
    free(colour); free(stack); free(cur); return ok;
}
static int valid_topo(const CSR *g, const int *order) {
    int *pos = malloc((size_t)g->n * sizeof(int)); for (int i = 0; i < g->n; i++) pos[order[i]] = i;
    int ok = 1; for (int u = 0; u < g->n; u++) for (int k = g->off[u]; k < g->off[u + 1]; k++) if (pos[u] >= pos[g->adj[k]]) ok = 0;
    free(pos); return ok;
}

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    /* 1. small worked example: the order DFS/BFS produce */
    Edge ex[] = { {0,1},{0,2},{1,3},{1,4},{2,4},{3,5},{4,5} };
    CSR g = csr_build(6, ex, 7, 0);
    int dist[6], q[6], order[6], cnt = 0; char seen[6] = {0};
    bfs(&g, 0, dist, q);
    printf("BFS from 0: order");
    for (int i = 0; i < 6; i++) printf(" %d", q[i]);
    printf("  distances");
    for (int i = 0; i < 6; i++) printf(" %d", dist[i]);
    printf("\n");
    dfs_rec(&g, 0, seen, order, &cnt);
    printf("DFS from 0 (recursive): ");
    for (int i = 0; i < cnt; i++) printf("%d ", order[i]);
    printf("\n");
    int order2[6], cnt2 = 0; char seen2[6] = {0};
    dfs_iter(&g, 0, seen2, order2, &cnt2);
    printf("DFS from 0 (iterative): ");
    for (int i = 0; i < cnt2; i++) printf("%d ", order2[i]);
    printf("\n\n");
    csr_free(&g);

    /* 2. randomized tests against a brute-force transitive closure */
    for (int round = 0; round < 300; round++) {
        int n = 2 + (int)(rnd() % 14), m = (int)(rnd() % (unsigned)(2 * n)); Edge *e = malloc((size_t)(m + 1) * sizeof *e);
        for (int i = 0; i < m; i++) e[i] = (Edge){ (int)(rnd() % (unsigned)n), (int)(rnd() % (unsigned)n) };
        CSR u = csr_build(n, e, (size_t)m, 0), d = csr_build(n, e, (size_t)m, 1);
        char reach[16][16] = {{0}}; for (int i = 0; i < n; i++) reach[i][i] = 1;
        for (int i = 0; i < m; i++) { reach[e[i].u][e[i].v] = reach[e[i].v][e[i].u] = 1; }
        for (int k = 0; k < n; k++) for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) if (reach[i][k] && reach[k][j]) reach[i][j] = 1;
        int comp[16]; int c = components(&u, comp); int expect = 0; char counted[16] = {0};
        for (int i = 0; i < n; i++) { int root = i; for (int j = 0; j < i; j++) if (reach[i][j]) { root = j; break; } if (!counted[root]) { counted[root] = 1; expect++; } }
        assert(c == expect); for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) assert((comp[i] == comp[j]) == reach[i][j]);
        /* bipartite via brute force: try all 2^n colourings for tiny n */
        int bip_brute = 0; for (int mask = 0; mask < (1 << n) && !bip_brute; mask++) { int okc = 1; for (int i = 0; i < m; i++) if (((mask >> e[i].u) & 1) == ((mask >> e[i].v) & 1)) okc = 0; bip_brute = okc; }
        assert(bipartite(&u) == bip_brute);
        /* directed: Kahn and DFS agree on acyclicity, and any produced order is valid */
        int o1[16], o2[16]; int a = topo_kahn(&d, o1), b = topo_dfs(&d, o2); assert(a == b); if (a) { assert(valid_topo(&d, o1)); assert(valid_topo(&d, o2)); }
        /* BFS distances vs Floyd-Warshall on the undirected graph */
        int fw[16][16]; for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) fw[i][j] = i == j ? 0 : 1000000;
        for (int i = 0; i < m; i++) if (e[i].u != e[i].v) fw[e[i].u][e[i].v] = fw[e[i].v][e[i].u] = 1;
        for (int k = 0; k < n; k++) for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) if (fw[i][k] + fw[k][j] < fw[i][j]) fw[i][j] = fw[i][k] + fw[k][j];
        int dd[16], qq[16]; for (int src = 0; src < n; src++) { bfs(&u, src, dd, qq); for (int j = 0; j < n; j++) assert(dd[j] == (fw[src][j] >= 1000000 ? -1 : fw[src][j])); }
        free(e); csr_free(&u); csr_free(&d);
    }
    printf("300 random graphs: components, bipartiteness, Kahn/DFS topological sort and BFS distances match brute force\n");
    return 0;
}
