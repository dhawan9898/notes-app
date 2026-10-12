#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../graphs/graph.h"
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

/* ===== Tarjan's SCC, iterative: one DFS; a vertex whose low-link equals its index is the root of a component ===== */
static int tarjan_scc(const CSR *g, int *comp) {
    int n = g->n, idx = 0, ncomp = 0, sp = 0, cp = 0;
    int *index = malloc((size_t)n * sizeof(int)), *low = malloc((size_t)n * sizeof(int)), *cur = malloc((size_t)n * sizeof(int));
    int *call = malloc((size_t)n * sizeof(int)), *stk = malloc((size_t)n * sizeof(int)); char *on = calloc((size_t)n, 1);
    for (int i = 0; i < n; i++) { index[i] = -1; comp[i] = -1; }
    for (int r = 0; r < n; r++) if (index[r] < 0) {
        call[cp++] = r; index[r] = low[r] = idx++; cur[r] = g->off[r]; stk[sp++] = r; on[r] = 1;
        while (cp) {
            int u = call[cp - 1];
            if (cur[u] < g->off[u + 1]) {
                int v = g->adj[cur[u]++];
                if (index[v] < 0) { index[v] = low[v] = idx++; cur[v] = g->off[v]; stk[sp++] = v; on[v] = 1; call[cp++] = v; }   /* tree edge: descend */
                else if (on[v] && index[v] < low[u]) low[u] = index[v];                                                      /* edge to a vertex still on the component stack */
            } else {
                if (low[u] == index[u]) { int v; do { v = stk[--sp]; on[v] = 0; comp[v] = ncomp; } while (v != u); ncomp++; }   /* u is a component root: pop it */
                cp--; if (cp) { int p = call[cp - 1]; if (low[u] < low[p]) low[p] = low[u]; }                                  /* propagate low-link to the parent */
            }
        }
    }
    free(index); free(low); free(cur); free(call); free(stk); free(on); return ncomp;
}

/* ===== bridges and articulation points of an undirected graph: low-link on the DFS tree (edge ids avoid confusing multi-edges with the parent edge) ===== */
typedef struct { int n; int *off, *to, *eid; } UG;
static UG ug_build(int n, const Edge *e, int m) {
    UG g = { n, calloc((size_t)n + 1, sizeof(int)), malloc((size_t)(2 * m + 1) * sizeof(int)), malloc((size_t)(2 * m + 1) * sizeof(int)) };
    for (int i = 0; i < m; i++) { g.off[e[i].u + 1]++; g.off[e[i].v + 1]++; }
    for (int i = 0; i < n; i++) g.off[i + 1] += g.off[i];
    int *fill = malloc((size_t)n * sizeof(int)); memcpy(fill, g.off, (size_t)n * sizeof(int));
    for (int i = 0; i < m; i++) { g.to[fill[e[i].u]] = e[i].v; g.eid[fill[e[i].u]++] = i; g.to[fill[e[i].v]] = e[i].u; g.eid[fill[e[i].v]++] = i; }
    free(fill); return g;
}
static void ug_free(UG *g) { free(g->off); free(g->to); free(g->eid); }
static void cut_structure(const UG *g, int m, char *is_bridge, char *is_ap) {
    int n = g->n, idx = 0;
    int *tin = malloc((size_t)n * sizeof(int)), *low = malloc((size_t)n * sizeof(int)), *cur = malloc((size_t)n * sizeof(int)), *pe = malloc((size_t)n * sizeof(int)), *st = malloc((size_t)n * sizeof(int));
    for (int i = 0; i < n; i++) { tin[i] = -1; is_ap[i] = 0; } for (int i = 0; i < m; i++) is_bridge[i] = 0;
    for (int r = 0; r < n; r++) if (tin[r] < 0) {
        int sp = 0, root_children = 0; st[sp++] = r; tin[r] = low[r] = idx++; cur[r] = g->off[r]; pe[r] = -1;
        while (sp) {
            int u = st[sp - 1];
            if (cur[u] < g->off[u + 1]) {
                int k = cur[u]++, v = g->to[k], id = g->eid[k];
                if (id == pe[u]) continue;                                  /* do not walk back along the edge we arrived by */
                if (tin[v] < 0) { tin[v] = low[v] = idx++; cur[v] = g->off[v]; pe[v] = id; st[sp++] = v; if (u == r) root_children++; }
                else if (tin[v] < low[u]) low[u] = tin[v];                  /* back edge */
            } else {
                sp--;
                if (sp) {
                    int p = st[sp - 1]; if (low[u] < low[p]) low[p] = low[u];
                    if (low[u] > tin[p]) is_bridge[pe[u]] = 1;              /* nothing below u reaches p or above except via this edge */
                    if (p != r && low[u] >= tin[p]) is_ap[p] = 1;           /* subtree of u cannot bypass p */
                }
            }
        }
        if (root_children > 1) is_ap[r] = 1;                                /* the root is a cut vertex iff it has two or more DFS children */
    }
    free(tin); free(low); free(cur); free(pe); free(st);
}

/* ---- brute-force oracles for tiny graphs ---- */
static int components_without(int n, const Edge *e, int m, int skip_edge, int skip_vertex) {
    int comp[16]; for (int i = 0; i < n; i++) comp[i] = i;
    for (int ch = 1; ch;) { ch = 0; for (int i = 0; i < m; i++) { if (i == skip_edge || e[i].u == skip_vertex || e[i].v == skip_vertex) continue;
        int a = comp[e[i].u], b = comp[e[i].v]; if (a != b) { int lo = a < b ? a : b; comp[e[i].u] = comp[e[i].v] = lo; ch = 1; } } }
    int seen[16] = {0}, c = 0;                                                  /* count distinct labels among the non-skipped vertices */
     for (int i = 0; i < n; i++) if (i != skip_vertex && !seen[comp[i]]) { seen[comp[i]] = 1; c++; }
    return c;
}

int main(void) {
    int bridges_total = 0, aps_total = 0, sccs_total = 0;
    for (int round = 0; round < 3000; round++) {
        int n = 2 + (int)(rnd() % 9), m = (int)(rnd() % (unsigned)(2 * n + 1)); Edge e[32];
        for (int i = 0; i < m; i++) { do { e[i] = (Edge){ (int)(rnd() % (unsigned)n), (int)(rnd() % (unsigned)n) }; } while (e[i].u == e[i].v); }   /* no self-loops; multi-edges allowed */
        /* --- directed: SCC vs mutual reachability --- */
        CSR d = csr_build(n, e, (size_t)m, 1); int comp[16]; int nc = tarjan_scc(&d, comp);
        char r[16][16] = {{0}}; for (int i = 0; i < n; i++) r[i][i] = 1; for (int i = 0; i < m; i++) r[e[i].u][e[i].v] = 1;
        for (int k = 0; k < n; k++) for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) if (r[i][k] && r[k][j]) r[i][j] = 1;
        int expect = 0; char counted[16] = {0};
        for (int i = 0; i < n; i++) { int root = i; for (int j = 0; j < i; j++) if (r[i][j] && r[j][i]) { root = j; break; } if (!counted[root]) { counted[root] = 1; expect++; } }
        assert(nc == expect); for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) assert((comp[i] == comp[j]) == (r[i][j] && r[j][i]));
        for (int i = 0; i < m; i++) { /* Tarjan numbers components in reverse topological order of the condensation: every edge goes to a component with an id <= its source's */
            assert(comp[e[i].u] >= comp[e[i].v]); }
        sccs_total += nc; csr_free(&d);
        /* --- undirected: bridges and articulation points vs deleting and recounting --- */
        UG g = ug_build(n, e, m); char br[32], ap[16]; cut_structure(&g, m, br, ap);
        int base = components_without(n, e, m, -1, -1);
        for (int i = 0; i < m; i++) { int more = components_without(n, e, m, i, -1) > base; assert(more == br[i]); bridges_total += br[i]; }
        for (int v = 0; v < n; v++) {
            int isolated_before = 0; for (int j = 0; j < m; j++) if (e[j].u == v || e[j].v == v) isolated_before = 1;
            int after = components_without(n, e, m, -1, v);
            int expect_ap = isolated_before && after > base;                 /* an isolated vertex is its own component; removing it lowers the count */
            assert(expect_ap == ap[v]); aps_total += ap[v];
        }
        ug_free(&g);
    }
    printf("3000 random small graphs: Tarjan SCC equals mutual reachability (components come out in reverse topological order);\n");
    printf("bridges and articulation points equal 'delete it and recount components' (%d bridges, %d articulation points, %d SCCs seen)\n\n", bridges_total, aps_total, sccs_total);

    /* scale: a million-vertex random digraph and a long path-like graph (deep DFS) */
    int n = 1000000; size_t m = 3000000; Edge *e = malloc(m * sizeof *e);
    for (size_t i = 0; i < m; i++) e[i] = (Edge){ (int)(rnd() % (unsigned)n), (int)(rnd() % (unsigned)n) };
    CSR g = csr_build(n, e, m, 1); int *comp = malloc((size_t)n * sizeof(int)); double t0 = now(); int nc = tarjan_scc(&g, comp); double t = now() - t0;
    int *size = calloc((size_t)nc, sizeof(int)); int big = 0; for (int i = 0; i < n; i++) if (++size[comp[i]] > big) big = size[comp[i]];
    printf("random digraph, %d vertices, %zu arcs: %d strongly connected components, the largest has %d vertices; Tarjan took %.0f ms\n", n, m, nc, big, t * 1e3);
    csr_free(&g); free(e); free(comp); free(size);
    int L = 2000000; Edge *pe = malloc((size_t)L * sizeof *pe); for (int i = 0; i + 1 < L; i++) pe[i] = (Edge){ i, i + 1 }; pe[L - 1] = (Edge){ L - 1, 0 };
    CSR cg = csr_build(L, pe, (size_t)L, 1); comp = malloc((size_t)L * sizeof(int)); t0 = now(); nc = tarjan_scc(&cg, comp);
    printf("one directed cycle of %d vertices (DFS depth %d): %d component, no stack overflow because the DFS is iterative (%.0f ms)\n", L, L, nc, (now() - t0) * 1e3);
    csr_free(&cg); free(pe); free(comp);
    return 0;
}
