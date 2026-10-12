---
title: "Union-Find, Spanning Trees, Connectivity and Max-Flow"
track: dsa
module: Graphs
moduleOrder: 6
order: 3
level: expert
summary: "Disjoint sets with path compression and union by rank (measured), Kruskal and Prim, Tarjan's strongly connected components, bridges and articulation points, and max-flow/min-cut with Edmonds-Karp, Dinic and bipartite matching — each verified against brute force."
sources:
  - label: "Tarjan — Efficiency of a Good But Not Linear Set Union Algorithm (J. ACM 22(2), 1975)"
  - label: "Kruskal — On the Shortest Spanning Subtree of a Graph and the Traveling Salesman Problem (Proc. AMS 7(1), 1956)"
  - label: "Prim — Shortest Connection Networks and Some Generalizations (Bell System Technical Journal 36, 1957)"
  - label: "Tarjan — Depth-First Search and Linear Graph Algorithms (SIAM J. Computing 1(2), 1972)"
  - label: "Ford & Fulkerson — Maximal Flow Through a Network (Canadian J. Mathematics 8, 1956)"
  - label: "Edmonds & Karp — Theoretical Improvements in Algorithmic Efficiency for Network Flow Problems (J. ACM 19(2), 1972)"
  - label: "Dinitz — Algorithm for Solution of a Problem of Maximum Flow in a Network with Power Estimation (Soviet Math. Doklady 11, 1970)"
  - label: "Hopcroft & Karp — An n^(5/2) Algorithm for Maximum Matchings in Bipartite Graphs (SIAM J. Computing 2(4), 1973)"
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 19 (Data Structures for Disjoint Sets), Ch. 21 (Minimum Spanning Trees), Ch. 22.5 (Strongly connected components), Ch. 24 (Maximum Flow)"
---

This note collects four graph toolkits that sit on top of traversal: **disjoint sets** (a tiny structure that answers "are these in the same group?" in practically constant time), **minimum spanning trees** (the cheapest way to connect everything), **connectivity analysis** (strongly connected components, bridges, articulation points) and **network flow** (how much can be sent through a capacitated network, and what is the bottleneck).

## Union–find (disjoint-set union)

The structure maintains a partition of `{0 … n−1}` into sets under two operations: `find(x)` returns a canonical representative of x's set; `union(a, b)` merges two sets. Each set is a tree stored with parent pointers; the root is the representative. Two cheap heuristics make it nearly constant-time:

- **Union by rank (or size):** hang the shallower tree beneath the deeper one, so tree depth stays `O(log n)`.
- **Path compression:** during `find`, repoint every node on the search path directly at the root, flattening the tree for later queries.

With both, a sequence of `m` operations on `n` elements costs `O(m · α(n))`, where `α` is the inverse Ackermann function — below 5 for any `n` that fits in the universe (Tarjan, 1975, who also proved this is optimal). The code lets the heuristics be switched on and off so their effect can be measured.

```c
#ifndef DSU_H
#define DSU_H
#include <stdint.h>
#include <stdlib.h>

/* Disjoint-set union (union-find) with selectable heuristics so their effect can be measured. */
enum { DSU_NAIVE = 0, DSU_COMPRESS = 1, DSU_RANK = 2, DSU_BOTH = 3 };
typedef struct { int *p, *rank; int n, mode; unsigned long long steps; } DSU;   /* steps = parent pointers followed */

static inline void dsu_init(DSU *d, int n, int mode) {
    d->p = malloc((size_t)n * sizeof(int)); d->rank = calloc((size_t)n, sizeof(int)); d->n = n; d->mode = mode; d->steps = 0;
    for (int i = 0; i < n; i++) d->p[i] = i;
}
static inline void dsu_free(DSU *d) { free(d->p); free(d->rank); }
static inline int dsu_find(DSU *d, int x) {
    int r = x;
    while (d->p[r] != r) { r = d->p[r]; d->steps++; }
    if (d->mode & DSU_COMPRESS)                                      /* path compression: point every node on the path at the root */
        while (d->p[x] != r) { int nx = d->p[x]; d->p[x] = r; x = nx; }
    return r;
}
static inline int dsu_union(DSU *d, int a, int b) {                  /* returns 1 if a and b were in different sets */
    a = dsu_find(d, a); b = dsu_find(d, b);
    if (a == b) return 0;
    if (d->mode & DSU_RANK) {                                         /* union by rank: attach the shallower tree below the deeper one */
        if (d->rank[a] < d->rank[b]) { int t = a; a = b; b = t; }
        d->p[b] = a; if (d->rank[a] == d->rank[b]) d->rank[a]++;
    } else d->p[b] = a;
    return 1;
}
#endif
```

```c
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
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra bench_dsu.c -o bench_dsu && ./bench_dsu
n unions of random pairs followed by n finds; parent pointers followed per operation, and ns per operation
        n | heuristics             steps/op      ns/op
   100000 | naive                   3966.15      22786
   100000 | path compression           2.61         21
   100000 | union by rank              1.93         19
   100000 | both                       1.19         13
  1000000 | naive                 (skipped)          -
  1000000 | path compression           2.89         31
  1000000 | union by rank              1.95         25
  1000000 | both                       1.19         18
 10000000 | naive                 (skipped)          -
 10000000 | path compression           3.18        137
 10000000 | union by rank              1.93         81
 10000000 | both                       1.19         68

chain-building order union(i+1, i), then 1000 finds of node 0 (the deepest node):
  naive                 99999.0 pointer steps per find
  path compression        101.0 pointer steps per find
  union by rank             1.0 pointer steps per find
  both                      1.0 pointer steps per find
```

What this shows:

- **Naive linking** averages thousands of pointer steps per operation even on random input (each union of two random roots can lengthen paths) and is skipped at larger sizes because it would take far too long.
- **Either heuristic alone** brings the average to 2–3 steps per operation, and **both together** to about 1.2 — essentially a parent lookup. At ten million elements the cost per operation rises to 60–80 ns not because of the algorithm but because each step is a cache miss.
- The **adversarial order** (`union(i+1, i)`, which hangs the growing tree under a fresh singleton) turns the naive structure into a chain of 100,000 nodes: 99,999 steps per find. Path compression alone flattens it after the first query (the average is dominated by that first expensive find), while union by rank never lets the chain form (1 step).

Applications: Kruskal's algorithm (below), incremental connectivity ("add edges, ask if connected"), connected-component labeling in image processing, percolation, equivalence of types in a unifier, offline LCA (Tarjan), and detecting cycles while building a graph.

## Minimum spanning trees

A **spanning tree** connects all `V` vertices with `V − 1` edges; the **minimum spanning tree** (MST) has the least total weight. Both classic algorithms rest on one theorem, the **cut property**: for any partition of the vertices into two sets, the lightest edge crossing it belongs to *some* MST (to every MST, if weights are distinct). A symmetric **cycle property** says the heaviest edge on any cycle can be discarded.

- **Kruskal** — sort all edges by weight; scan in order; keep an edge unless it connects two vertices already in the same component (union–find tells in near-O(1)). `O(E log E)`, dominated by the sort. Natural when edges are already a list or the graph is sparse.
- **Prim** — grow a single tree from a start vertex; always add the cheapest edge leaving it, using a priority queue (the same loop as Dijkstra with the key being edge weight instead of path length). `O(E log V)`; better when the graph is dense or given as an adjacency structure.
- **Borůvka** — every component picks its cheapest outgoing edge simultaneously, merging in `O(log V)` rounds. The basis of most parallel and distributed MST algorithms.

If weights tie, the MST may not be unique but its total weight is.

```c
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
```

```text
$ gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined mst.c -o mst_asan && ./mst_asan | head -1 && gcc -std=gnu11 -O2 -Wall -Wextra mst.c -o mst && ./mst
1500 random small graphs: Kruskal and Prim both equal the brute-force minimum spanning tree weight
1500 random small graphs: Kruskal and Prim both equal the brute-force minimum spanning tree weight

graph                                vertices      edges | Kruskal ms    Prim ms
random, 8 edges per vertex            1000000    8000000 |       2930       3854   (MST weight 75081411725)
random, 2 edges per vertex            1000000    2000000 |        381        725   (MST weight 285664315102)
```

The first line comes from the sanitizer build, which runs the random small-graph test: 1500 graphs where Kruskal and Prim are both compared with the **exhaustive minimum over all spanning trees** (every subset of `n − 1` edges is tried). The table is the optimized build on a million-vertex random graph: the two algorithms are within a small factor of each other, each limited by the same random memory access, with the sort dominating Kruskal on dense inputs.

**Networking note.** The *Spanning Tree: STP, RSTP and MSTP* note solves a different problem despite the name: STP elects a root bridge and builds a *shortest-path tree toward it* (by path cost) so that a Layer-2 network is loop-free; it does not minimise total link cost. MST algorithms are what you use to *design* a network (cheapest set of links connecting all sites), in clustering (single-linkage clustering is a Kruskal run), and as an approximation for Steiner-tree and travelling-salesman problems.

## Strongly connected components

In a directed graph, `u` and `v` are in the same **strongly connected component** (SCC) if each can reach the other. Collapsing each SCC into one vertex yields the **condensation**, always a DAG — which is why SCC is the first step in analyses such as: cycle detection in dependency graphs and call graphs (a component of size > 1 is a cycle), reachability for 2-SAT, dead-code and recursion analysis in compilers, and garbage-collection unreachability in cyclic structures.

**Tarjan's algorithm** finds all SCCs in one DFS. Each vertex gets a discovery index and a **low-link**: the smallest index reachable from its DFS subtree using tree edges followed by at most one edge to a vertex still on the component stack. When a vertex's low-link equals its own index it is the root of an SCC: pop the stack down to it. **Kosaraju's algorithm** does two passes (DFS for finish order, then DFS on the transposed graph in decreasing finish order) and is simpler to prove. The components come out in **reverse topological order** of the condensation — a useful property the test below verifies.

## Bridges and articulation points

In an undirected graph, a **bridge** is an edge whose removal disconnects the graph, and an **articulation point** (cut vertex) is a vertex whose removal does. These are the *single points of failure*: the link or router that, if it dies, partitions the network. The same DFS low-link idea finds them in `O(V + E)`: an edge `(p, u)` is a bridge if nothing in `u`'s subtree has a back edge to `p` or above (`low[u] > tin[p]`), and a non-root `p` is an articulation point if some child's subtree cannot bypass it (`low[u] ≥ tin[p]`); the DFS root is one iff it has two or more children. Edge identifiers, not endpoint comparisons, are used to skip the parent edge so parallel links are handled correctly (two parallel links are not a bridge).

Network engineers use this directly: audit a topology for bridges and cut vertices (and then add redundant links), or check that an L2 fabric survives any single failure (see the *Resilience* module notes on FHRP and BFD).

```c
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
```

```text
$ gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined scc.c -o scc_asan && ./scc_asan
3000 random small graphs: Tarjan SCC equals mutual reachability (components come out in reverse topological order);
bridges and articulation points equal 'delete it and recount components' (4065 bridges, 3026 articulation points, 14368 SCCs seen)

random digraph, 1000000 vertices, 3000000 arcs: 115853 strongly connected components, the largest has 884148 vertices; Tarjan took 822 ms
one directed cycle of 2000000 vertices (DFS depth 2000000): 1 component, no stack overflow because the DFS is iterative (169 ms)
```

All three DFS-based algorithms are **iterative** with explicit stacks and cursors — the last line runs a directed cycle of two million vertices, where a recursive DFS would overflow the stack (previous note). Correctness is checked on 3000 random small graphs: SCC membership equals mutual reachability from a transitive closure; bridges and articulation points equal the brute-force answer "delete it and recount the components"; and every arc leads to an equal-or-lower component index.

## Maximum flow and minimum cut

A **flow network** has a capacity on each arc, a source `s` and a sink `t`. A **flow** respects capacities and conserves flow at every other vertex. The **max-flow min-cut theorem** (Ford & Fulkerson) says the largest flow equals the capacity of the smallest **cut** (a partition of the vertices with `s` on one side and `t` on the other, counting only arcs leaving `s`'s side). The proof is also the algorithm: while a path with spare capacity exists, push flow along it; the **residual graph** records what is left, and also lets later steps *undo* earlier flow by pushing along reverse arcs. When no augmenting path remains, the vertices still reachable from `s` define a minimum cut.

- **Ford–Fulkerson** with arbitrary augmenting paths can be slow (even non-terminating with irrational capacities).
- **Edmonds–Karp** always augments along the path with the fewest arcs (BFS), bounding augmentations by `O(VE)` and time by `O(VE²)`.
- **Dinic** builds a *level graph* by BFS and sends a **blocking flow** through it with DFS, repeating until `t` is unreachable: `O(V²E)` in general and `O(E√V)` on unit-capacity networks such as bipartite matching. It is the usual choice in practice.
- **Push–relabel** (Goldberg–Tarjan) and, more recently, almost-linear-time algorithms are asymptotically faster.

**Applications**: bipartite matching (source → left vertices → right vertices → sink, all capacities 1: the flow value is the maximum matching size, and **Hopcroft–Karp** is the specialised `O(E√V)` algorithm); assignment and scheduling; image segmentation (min-cut separates foreground from background); network capacity and survivability (the min-cut between two sites is the number of edge-disjoint paths — Menger's theorem); and project selection.

```c
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

typedef long long ll;
static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

/* Flow network in "paired edge" form: edge i and edge i^1 are a forward arc and its residual (reverse) arc. */
typedef struct { int n, m, cap_m; int *head, *nxt, *to; ll *cap; int *level, *it; } Flow;
static void fl_init(Flow *f, int n, int max_edges) {
    f->n = n; f->m = 0; f->cap_m = 2 * max_edges;
    f->head = malloc((size_t)n * sizeof(int)); memset(f->head, -1, (size_t)n * sizeof(int));
    f->nxt = malloc((size_t)f->cap_m * sizeof(int)); f->to = malloc((size_t)f->cap_m * sizeof(int)); f->cap = malloc((size_t)f->cap_m * sizeof(ll));
    f->level = malloc((size_t)n * sizeof(int)); f->it = malloc((size_t)n * sizeof(int));
}
static void fl_free(Flow *f) { free(f->head); free(f->nxt); free(f->to); free(f->cap); free(f->level); free(f->it); }
static void fl_add(Flow *f, int u, int v, ll c) {
    f->to[f->m] = v; f->cap[f->m] = c; f->nxt[f->m] = f->head[u]; f->head[u] = f->m++;
    f->to[f->m] = u; f->cap[f->m] = 0; f->nxt[f->m] = f->head[v]; f->head[v] = f->m++;          /* residual arc starts with capacity 0 */
}

/* ---- Edmonds-Karp: repeatedly augment along a SHORTEST (fewest edges) residual path found by BFS; O(V E^2) ---- */
static ll edmonds_karp(Flow *f, int src, int dst, long *augmentations) {
    ll total = 0; int *prev_e = malloc((size_t)f->n * sizeof(int)), *q = malloc((size_t)f->n * sizeof(int)); *augmentations = 0;
    for (;;) {
        for (int i = 0; i < f->n; i++) prev_e[i] = -2;
        prev_e[src] = -1; int h = 0, t = 0; q[t++] = src;
        while (h < t && prev_e[dst] == -2) { int u = q[h++]; for (int e = f->head[u]; e >= 0; e = f->nxt[e]) if (f->cap[e] > 0 && prev_e[f->to[e]] == -2) { prev_e[f->to[e]] = e; q[t++] = f->to[e]; } }
        if (prev_e[dst] == -2) break;                                      /* no augmenting path: the flow is maximum (max-flow min-cut) */
        ll push = (ll)1 << 60; for (int v = dst; v != src; v = f->to[prev_e[v] ^ 1]) if (f->cap[prev_e[v]] < push) push = f->cap[prev_e[v]];
        for (int v = dst; v != src; v = f->to[prev_e[v] ^ 1]) { f->cap[prev_e[v]] -= push; f->cap[prev_e[v] ^ 1] += push; }
        total += push; (*augmentations)++;
    }
    free(prev_e); free(q); return total;
}

/* ---- Dinic: BFS builds a level graph, then DFS pushes a BLOCKING flow along level-increasing arcs; O(V^2 E), unit networks O(E sqrt V) ---- */
static int dinic_bfs(Flow *f, int src, int dst) {
    for (int i = 0; i < f->n; i++) f->level[i] = -1;
    int *q = malloc((size_t)f->n * sizeof(int)), h = 0, t = 0; q[t++] = src; f->level[src] = 0;
    while (h < t) { int u = q[h++]; for (int e = f->head[u]; e >= 0; e = f->nxt[e]) if (f->cap[e] > 0 && f->level[f->to[e]] < 0) { f->level[f->to[e]] = f->level[u] + 1; q[t++] = f->to[e]; } }
    free(q); return f->level[dst] >= 0;
}
static ll dinic_dfs(Flow *f, int u, int dst, ll pushed) {
    if (u == dst) return pushed;
    for (int *e = &f->it[u]; *e >= 0; *e = f->nxt[*e]) {                    /* current-arc pointer: an arc that failed once is never retried in this phase */
        int v = f->to[*e];
        if (f->cap[*e] > 0 && f->level[v] == f->level[u] + 1) {
            ll d = dinic_dfs(f, v, dst, pushed < f->cap[*e] ? pushed : f->cap[*e]);
            if (d > 0) { f->cap[*e] -= d; f->cap[*e ^ 1] += d; return d; }
        }
    }
    return 0;
}
static ll dinic(Flow *f, int src, int dst, int *phases) {
    ll total = 0; *phases = 0;
    while (dinic_bfs(f, src, dst)) {
        memcpy(f->it, f->head, (size_t)f->n * sizeof(int)); (*phases)++;
        for (ll d; (d = dinic_dfs(f, src, dst, (ll)1 << 60)) > 0; ) total += d;
    }
    return total;
}

/* ---- brute-force min cut: enumerate every vertex set S containing src and not dst; max-flow = min over S of capacity leaving S ---- */
static ll brute_min_cut(int n, int m, const int (*ed)[3], int src, int dst) {
    ll best = -1;
    for (unsigned mask = 0; mask < (1u << n); mask++) {
        if (!(mask >> src & 1) || (mask >> dst & 1)) continue;
        ll cut = 0; for (int i = 0; i < m; i++) if ((mask >> ed[i][0] & 1) && !(mask >> ed[i][1] & 1)) cut += ed[i][2];
        if (best < 0 || cut < best) best = cut;
    }
    return best;
}

int main(void) {
    for (int round = 0; round < 2000; round++) {
        int n = 3 + (int)(rnd() % 8), m = 1 + (int)(rnd() % (unsigned)(3 * n)); int ed[40][3]; Flow a, b; fl_init(&a, n, m); fl_init(&b, n, m);
        for (int i = 0; i < m; i++) { int u = (int)(rnd() % (unsigned)n), v = (int)(rnd() % (unsigned)n); if (u == v) v = (u + 1) % n; int c = 1 + (int)(rnd() % 9); ed[i][0] = u; ed[i][1] = v; ed[i][2] = c; fl_add(&a, u, v, c); fl_add(&b, u, v, c); }
        long aug; int ph; ll ek = edmonds_karp(&a, 0, n - 1, &aug), dn = dinic(&b, 0, n - 1, &ph), cut = brute_min_cut(n, m, (const int (*)[3])ed, 0, n - 1);
        assert(ek == dn && dn == cut);                                      /* max-flow min-cut theorem, checked on 2000 networks */
        fl_free(&a); fl_free(&b);
    }
    printf("2000 random flow networks: Edmonds-Karp = Dinic = brute-force minimum cut (max-flow min-cut theorem)\n\n");

    /* bipartite matching as max flow: source -> left (cap 1), left -> right (cap 1), right -> sink (cap 1) */
    int L = 3000, R = 3000, deg = 4; Flow f; fl_init(&f, L + R + 2, L * deg + L + R); int src = L + R, dst = L + R + 1;
    char (*adj)[3000] = calloc((size_t)L, sizeof *adj);
    for (int u = 0; u < L; u++) fl_add(&f, src, u, 1);
    for (int v = 0; v < R; v++) fl_add(&f, L + v, dst, 1);
    for (int u = 0; u < L; u++) for (int k = 0; k < deg; k++) { int v = (int)(rnd() % (unsigned)R); if (!adj[u][v]) { adj[u][v] = 1; fl_add(&f, u, L + v, 1); } }
    int ph; double t0 = now(); ll match = dinic(&f, src, dst, &ph);
    printf("bipartite matching, %d x %d vertices, ~%d random edges per left vertex: maximum matching %lld (Dinic: %d phases, %.1f ms)\n", L, R, deg, match, ph, (now() - t0) * 1e3);

    /* Dinic vs Edmonds-Karp on a layered random network */
    printf("\n%-34s %10s %12s | %12s %10s | %14s %10s\n", "network", "max flow", "", "EK augments", "EK ms", "Dinic phases", "Dinic ms");
    int sizes[] = { 2000, 8000 };
    for (int si = 0; si < 2; si++) {
        int n = sizes[si], m = n * 6; Flow a, b; fl_init(&a, n, m); fl_init(&b, n, m);
        for (int i = 0; i < m; i++) { int u = (int)(rnd() % (unsigned)(n - 1)), v = u + 1 + (int)(rnd() % (unsigned)(n - 1 - u < 40 ? n - 1 - u : 40)); int c = 1 + (int)(rnd() % 100); fl_add(&a, u, v, c); fl_add(&b, u, v, c); }
        long aug; t0 = now(); ll ek = edmonds_karp(&a, 0, n - 1, &aug); double te = now() - t0;
        t0 = now(); ll dn = dinic(&b, 0, n - 1, &ph); double td = now() - t0; assert(ek == dn);
        printf("%-34s %10lld %12s | %12ld %10.0f | %14d %10.1f\n", si ? "DAG-like, 8000 vertices, 48k arcs" : "DAG-like, 2000 vertices, 12k arcs", ek, "", aug, te * 1e3, ph, td * 1e3);
        fl_free(&a); fl_free(&b);
    }
    fl_free(&f); free(adj);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined flow.c -o flow_asan && ./flow_asan | head -2 && gcc -std=gnu11 -O2 -Wall -Wextra flow.c -o flow && ./flow
2000 random flow networks: Edmonds-Karp = Dinic = brute-force minimum cut (max-flow min-cut theorem)

2000 random flow networks: Edmonds-Karp = Dinic = brute-force minimum cut (max-flow min-cut theorem)

bipartite matching, 3000 x 3000 vertices, ~4 random edges per left vertex: maximum matching 2937 (Dinic: 7 phases, 7.1 ms)

network                              max flow              |  EK augments      EK ms |   Dinic phases   Dinic ms
DAG-like, 2000 vertices, 12k arcs         191              |           96         38 |              5        4.0
DAG-like, 8000 vertices, 48k arcs         229              |          214        487 |             16       73.1
```

The first two lines are the sanitizer build: 2000 random networks (up to 10 vertices) where Edmonds–Karp, Dinic and the **brute-force minimum cut over all 2^n vertex subsets** all give the same value — the max-flow min-cut theorem verified rather than assumed. The second run (optimized) times a bipartite matching of 3000 + 3000 vertices — solved by Dinic in a handful of phases, a few milliseconds — and compares Edmonds–Karp with Dinic on layered random networks: Dinic needs far fewer phases than EK needs augmentations, and Dinic is roughly 7–10× faster than Edmonds–Karp on both networks here.

## Pitfalls

- **Union–find without both heuristics** degrades to linear-time operations on adversarial or sorted input. Always use union by rank/size *and* path compression (or path halving).
- **Recursive DFS for SCC/bridges** on deep graphs crashes — use the iterative forms.
- **Low-link subtleties**: for SCC use `index[v]` for edges to vertices still on the stack and ignore edges to finished components; for bridges use *edge ids* to skip the parent edge, otherwise parallel edges look like bridges.
- **Directed vs undirected**: bridges and articulation points are undirected concepts; for directed graphs use SCC and dominator trees.
- **Max-flow capacities** must be integers or handled with care; floating-point capacities cause termination and comparison problems. Overflow: capacity sums use 64-bit.
- **Kruskal needs the edges sorted**; sorting 100 million edges is the cost, not the union–find.
- **MST on disconnected graphs** produces a spanning *forest*; check the number of edges used against `V − 1`.
- **Dinic's DFS needs the current-arc pointer**, otherwise the same dead-end arcs are re-explored and the running time degrades.

## Key takeaways

1. Union–find with union by rank and path compression is practically O(1) per operation (inverse Ackermann); either heuristic alone already removes the pathological cases.
2. MST algorithms follow from the cut property: Kruskal (sort + union–find) and Prim (Dijkstra-like) agree on weight, and both were checked against exhaustive search.
3. Tarjan's one-pass SCC and bridge/articulation algorithms are low-link DFS computations; implement them iteratively.
4. Bridges and articulation points are a network's single points of failure — a graph algorithm answering a design question.
5. Max-flow equals min-cut; Edmonds–Karp is simple, Dinic's level graphs and blocking flows are fast in practice, and matching is flow with unit capacities.
6. Cross-check each algorithm against a brute-force oracle on small random inputs before trusting it at scale.
