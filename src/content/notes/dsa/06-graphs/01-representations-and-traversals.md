---
title: Graph Representations and Traversals
track: dsa
module: Graphs
moduleOrder: 6
order: 1
level: intermediate
summary: "Adjacency matrix vs list vs CSR (measured), BFS and DFS with edge classification, components, bipartite checks, topological sorting two ways, cycle reporting for dependency graphs, and why recursive DFS crashes on long paths."
prerequisites:
  - Stacks, Queues, Deques and Ring Buffers
  - "Trees and Graphs for Beginners"
sources:
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 20 (Elementary Graph Algorithms)"
  - label: "Tarjan — Depth-First Search and Linear Graph Algorithms (SIAM J. Computing 1(2), 1972)"
  - label: "Kahn — Topological Sorting of Large Networks (CACM 5(11), 1962)"
  - label: "Knuth — The Art of Computer Programming, Vol. 1, §2.3.4 (Basic Mathematical Properties of Trees) and §2.2.3 (Linked Allocation, topological sort)"
  - label: "Sedgewick — Algorithms in C, Part 5 Graph Algorithms, 3rd ed."
  - label: "systemd.unit(5) — Requires=, After= and ordering dependencies"
    url: https://www.freedesktop.org/software/systemd/man/latest/systemd.unit.html
---

A **graph** `G = (V, E)` is a set of vertices and a set of edges between them; edges are **directed** (arcs) or **undirected**, optionally carrying weights. Almost everything with relationships is a graph: networks (routers and links), dependency systems (packages, build targets, systemd units), call graphs, the heap seen by a garbage collector, state machines, maps. Two parameters matter for every algorithm: `V` and `E`, and the *density* `E / V²` (a **sparse** graph has `E ≈ V`; a **dense** one has `E ≈ V²`). Real graphs are almost always sparse.

## Representations

| Representation | Space | Edge `(u,v)`? | Iterate neighbours of `u` | Add edge | Best for |
|---|---|---|---|---|---|
| Adjacency matrix | `V²` bits/bytes | O(1) | O(V) | O(1) | Dense graphs, small `V`, bit-parallel algorithms (Floyd–Warshall) |
| Adjacency list (linked) | `V + E` nodes of 16 B+ | O(deg) | O(deg), pointer chasing | O(1) | Dynamic graphs |
| Adjacency list (dynamic arrays) | `V + E` words | O(deg) | O(deg), contiguous | amortized O(1) | General-purpose, mutable |
| **CSR** (compressed sparse row) | `V + 1 + E` words, two arrays | O(deg) (O(log deg) if sorted) | O(deg), contiguous | rebuild | **Static** graphs, performance |
| Edge list | `E` pairs | O(E) | O(E) | O(1) | Input format, Kruskal, sorting edges |

CSR stores all neighbours of all vertices in one array `adj`, with `off[u] … off[u+1]` delimiting vertex `u`'s slice. Building it is a counting sort: count degrees, prefix-sum them into offsets, scatter the edges. It is the format used by high-performance graph libraries, sparse-matrix code (it *is* the CSR sparse matrix) and graph databases' analytic engines.

```c
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
```

### Measuring the difference

BFS on a random undirected graph with one million vertices and eight million edges — 16 million directed arcs — on CSR and on two linked-list variants: nodes carved sequentially from one pool, and one `malloc` per arc.

```c
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include "graph.h"
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

static void bfs_list(const AList *g, int src, int *dist, int *queue) {
    for (int i = 0; i < g->n; i++) dist[i] = -1;
    int h = 0, t = 0; queue[t++] = src; dist[src] = 0;
    while (h < t) { int u = queue[h++]; for (const ANode *x = g->head[u]; x; x = x->next) if (dist[x->v] < 0) { dist[x->v] = dist[u] + 1; queue[t++] = x->v; } }
}
static void dfs_rec(const CSR *g, int u, char *seen) {
    seen[u] = 1;
    for (int k = g->off[u]; k < g->off[u + 1]; k++) if (!seen[g->adj[k]]) dfs_rec(g, g->adj[k], seen);
}

int main(void) {
    int n = 1000000; size_t m = 8000000;
    Edge *e = malloc(m * sizeof *e); for (size_t i = 0; i < m; i++) e[i] = (Edge){ (int)(rnd() % (unsigned)n), (int)(rnd() % (unsigned)n) };
    int *dist = malloc((size_t)n * sizeof(int)), *q = malloc((size_t)n * sizeof(int));
    printf("random undirected graph: %d vertices, %zu edges (%zu directed arcs), BFS from vertex 0\n\n", n, m, 2 * m);
    printf("%-34s %10s %10s %12s\n", "representation", "build ms", "BFS ms", "memory");
    double t0 = now(); CSR c = csr_build(n, e, m, 0); double tb = now() - t0;
    bfs(&c, 0, dist, q); t0 = now(); for (int r = 0; r < 3; r++) bfs(&c, 0, dist, q); double tr = (now() - t0) / 3;
    int reached = 0; for (int i = 0; i < n; i++) reached += dist[i] >= 0;
    printf("%-34s %10.0f %10.1f %9.1f MB\n", "CSR (2 arrays)", tb * 1e3, tr * 1e3, ((double)(n + 1) * 4 + (double)c.m * 4) / 1e6);
    t0 = now(); AList p = alist_build(n, e, m, 0, 1); tb = now() - t0;
    bfs_list(&p, 0, dist, q); t0 = now(); for (int r = 0; r < 3; r++) bfs_list(&p, 0, dist, q); tr = (now() - t0) / 3;
    printf("%-34s %10.0f %10.1f %9.1f MB\n", "linked list, pooled nodes", tb * 1e3, tr * 1e3, ((double)n * 8 + (double)(2 * m) * 16) / 1e6);
    alist_free(&p, 1);
    t0 = now(); AList l = alist_build(n, e, m, 0, 0); tb = now() - t0;
    bfs_list(&l, 0, dist, q); t0 = now(); for (int r = 0; r < 3; r++) bfs_list(&l, 0, dist, q); tr = (now() - t0) / 3;
    printf("%-34s %10.0f %10.1f %9.1f MB (+malloc overhead)\n", "linked list, malloc per edge", tb * 1e3, tr * 1e3, ((double)n * 8 + (double)(2 * m) * 16) / 1e6);
    alist_free(&l, 0);
    printf("%-34s %10s %10s %9.1f GB\n", "adjacency matrix (1 byte/pair)", "-", "-", (double)n * (double)n / 1e9);
    printf("BFS reached %d of %d vertices, max distance ", reached, n);
    int mx = 0; for (int i = 0; i < n; i++) if (dist[i] > mx) mx = dist[i]; printf("%d\n", mx);
    csr_free(&c); free(e); free(dist); free(q);

    /* recursion depth: a path graph 0-1-2-...-(L-1) */
    printf("\nrecursive DFS on a path graph with L vertices, default 8 MB stack (run in a child process):\n");
    for (int L = 10000; L <= 1000000; L *= 10) {
        Edge *pe = malloc((size_t)(L - 1) * sizeof *pe); for (int i = 0; i + 1 < L; i++) pe[i] = (Edge){ i, i + 1 };
        CSR pg = csr_build(L, pe, (size_t)(L - 1), 0); char *seen = calloc((size_t)L, 1);
        fflush(stdout); pid_t pid = fork();
        if (pid == 0) { dfs_rec(&pg, 0, seen); _exit(0); }
        int st; waitpid(pid, &st, 0);
        printf("  L = %8d: %s\n", L, WIFSIGNALED(st) ? "crashed (stack overflow, SIGSEGV)" : "ok");
        free(pe); free(seen); csr_free(&pg);
    }
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra bench_graph.c -o bench_graph && ./bench_graph
random undirected graph: 1000000 vertices, 8000000 edges (16000000 directed arcs), BFS from vertex 0

representation                       build ms     BFS ms       memory
CSR (2 arrays)                            305      352.0      68.0 MB
linked list, pooled nodes                 319     2655.9     264.0 MB
linked list, malloc per edge             1027     3678.7     264.0 MB (+malloc overhead)
adjacency matrix (1 byte/pair)              -          -    1000.0 GB
BFS reached 1000000 of 1000000 vertices, max distance 7

recursive DFS on a path graph with L vertices, default 8 MB stack (run in a child process):
  L =    10000: ok
  L =   100000: ok
  L =  1000000: crashed (stack overflow, SIGSEGV)
```

- **CSR needs about a quarter of the memory** (two integer arrays) and BFS runs several times faster (**roughly 6–9×** in typical runs), because neighbours of a vertex sit in consecutive cache lines instead of being scattered pool nodes. Building is also faster than one-`malloc`-per-edge.
- Linked lists with one `malloc` per edge are the slowest to build (allocator overhead) and add 16+ bytes of malloc metadata per node, not counted in the memory column.
- A **dense matrix** for a million vertices would need a terabyte even at one byte per pair, which is why matrices are only for small or truly dense graphs.
- The BFS visits every vertex and arc exactly once, `O(V + E)`; the speed difference is entirely memory behavior — the lesson of the linked-list note again.

## Breadth-first search

BFS explores in **layers**: the source, then all vertices one edge away, then two, and so on, using a FIFO queue. In an unweighted graph the layer number is the shortest-path distance (in edges). Each vertex is marked when it is first discovered — marking at *discovery* (when pushed), not when popped, is what keeps the queue to at most `V` entries and the running time linear.

Uses: shortest hop counts (RIP's metric, traceroute-style reachability), shortest paths on grids and mazes, level-order traversal, connected components, bipartite testing, and the multi-source variant (start with all sources in the queue) used for "distance to the nearest X".

## Depth-first search

DFS follows an edge as deep as possible before backing up, using a stack (explicitly or via recursion). It computes **discovery** and **finish** times, which classify every edge `(u, v)` visited from `u`:

| Edge type | When `v` is… | Meaning |
|---|---|---|
| Tree | undiscovered | part of the DFS forest |
| Back | an ancestor of `u` (still "grey") | closes a **cycle** |
| Forward | a finished descendant of `u` | directed graphs only |
| Cross | finished, neither ancestor nor descendant | directed graphs only |

In an *undirected* graph only tree and back edges occur. The key facts: a directed graph is acyclic **iff** DFS finds no back edge; and listing vertices by **decreasing finish time** gives a topological order. DFS is the engine of strongly-connected components, articulation points and bridges (covered later), and most other advanced graph algorithms.

The iterative form below keeps a per-vertex cursor to the next unexamined edge, so it visits vertices in **exactly** the same order as the recursive one while using an explicit, heap-allocated stack.

## Components, bipartite graphs, topological order

- **Connected components** (undirected): run BFS or DFS from each unvisited vertex; each run labels one component. `O(V + E)`. (Union–find solves the same problem incrementally — next-but-one note.)
- **Bipartite** (two-colourable) graphs have no odd cycle. Two-colour during BFS; an edge between two vertices of the same colour proves an odd cycle. Applications: matching problems, scheduling with two resource classes, and (fittingly) checking that a graph can be drawn with two groups.
- **Topological sort** orders the vertices of a **DAG** so that every edge goes from earlier to later. Two linear-time algorithms: **Kahn's** removes vertices with in-degree 0 and decrements their neighbours — if some vertices are never freed, they lie on or behind a cycle; **DFS-based** emits vertices in reverse finish order and detects cycles through grey-vertex back edges. Both are implemented and are checked against each other.

```c
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
```

```text
$ gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined algos.c -o algos && ./algos
BFS from 0: order 0 1 2 3 4 5  distances 0 1 1 2 2 3
DFS from 0 (recursive): 0 1 3 5 4 2 
DFS from 0 (iterative): 0 1 3 5 4 2 

300 random graphs: components, bipartiteness, Kahn/DFS topological sort and BFS distances match brute force
```

The small graph shows the signature difference: BFS visits `0 1 2 3 4 5` (by layer), DFS goes `0 1 3 5 4 2` (deep first). Then 300 random graphs on up to 15 vertices are checked against brute force: components and distances against Floyd–Warshall reachability, bipartiteness against trying all `2^n` colourings, and both topological sorts against each other and against the definition (every edge goes forward in the produced order).

## Dependency resolution

Topological sorting is the algorithm behind `make`, package managers, build tools (Bazel, Ninja, Cargo), spreadsheet recalculation and **systemd's start-up ordering**: `After=` and `Before=` add ordering edges between units, `Requires=` / `Wants=` add pulling edges, and systemd computes a transaction by ordering the units; a cycle in ordering dependencies is reported and systemd breaks it by dropping jobs. The example below reads `target: dependency…` lines and either prints a valid order or finds and prints the actual cycle by walking DFS parent links from the back edge — the diagnostic a user actually needs.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A tiny dependency resolver, the core of make, systemd's job ordering, package managers and build systems.
 * Input lines:  "target: dep1 dep2 ..."   (target needs deps first).  Output: a valid build order, or the cycle that blocks one. */
#define MAXN 64
static char name[MAXN][32]; static int nn;
static int adj[MAXN][MAXN], deg[MAXN];                  /* edge dep -> target: "dep must come before target" */

static int id(const char *s) { for (int i = 0; i < nn; i++) if (!strcmp(name[i], s)) return i; strcpy(name[nn], s); return nn++; }

static int colour[MAXN], parent[MAXN];
/* DFS over edges dep -> target; on a back edge u -> v (v is grey) walk parent[] from u back to v to print the cycle */
static int find_cycle(int u) {
    colour[u] = 1;
    for (int k = 0; k < deg[u]; k++) {
        int v = adj[u][k];
        if (colour[v] == 1) {
            printf("cycle: %s", name[v]);
            int path[MAXN], len = 0; for (int x = u; x != v; x = parent[x]) path[len++] = x;
            while (len) printf(" -> %s", name[path[--len]]);
            printf(" -> %s\n", name[v]); return 1;
        }
        if (colour[v] == 0) { parent[v] = u; if (find_cycle(v)) return 1; }
    }
    colour[u] = 2; return 0;
}

static void resolve(const char *title, const char *spec[], int lines) {
    nn = 0; memset(deg, 0, sizeof deg); memset(colour, 0, sizeof colour);
    for (int i = 0; i < lines; i++) {
        char buf[128]; strcpy(buf, spec[i]); char *colon = strchr(buf, ':'); *colon = 0;
        int t = id(buf);
        for (char *tok = strtok(colon + 1, " "); tok; tok = strtok(NULL, " ")) { int d = id(tok); adj[d][deg[d]++] = t; }
    }
    printf("== %s ==\n", title);
    int indeg[MAXN] = {0}, order[MAXN], h = 0, t = 0;
    for (int u = 0; u < nn; u++) for (int k = 0; k < deg[u]; k++) indeg[adj[u][k]]++;
    for (int u = 0; u < nn; u++) if (!indeg[u]) order[t++] = u;
    while (h < t) { int u = order[h++]; for (int k = 0; k < deg[u]; k++) if (--indeg[adj[u][k]] == 0) order[t++] = adj[u][k]; }
    if (t == nn) { printf("order:"); for (int i = 0; i < nn; i++) printf(" %s", name[order[i]]); printf("\n"); }
    else { printf("no valid order (%d of %d units can be ordered); ", t, nn); for (int u = 0; u < nn; u++) if (!colour[u] && find_cycle(u)) break; }
    printf("\n");
}

int main(void) {
    const char *ok[] = { "app: libnet libui", "libui: libgfx libfont", "libnet: libc", "libgfx: libc", "libfont: libc", "libc:" };
    resolve("acyclic build graph", ok, 6);
    const char *bad[] = { "network-online: network", "network: dbus", "dbus: network-online", "app: network-online" };
    resolve("with a dependency cycle", bad, 4);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined deps.c -o deps && ./deps
== acyclic build graph ==
order: libc libnet libgfx libfont libui app

== with a dependency cycle ==
no valid order (0 of 4 units can be ordered); cycle: network-online -> dbus -> network -> network-online
```

Edges point from a dependency to the thing that needs it ("must come before"), so the cycle prints as an ordering chain: `network-online` before `dbus` before `network` before `network-online`, which can never be satisfied. Note that Kahn's algorithm alone says only *that* a cycle exists (and that none of the four units could be ordered, including `app`, which is merely downstream of the cycle); a DFS pass finds the cycle itself.

## Recursion depth

Recursive DFS uses stack space proportional to the longest path, not to `V`. The benchmark's last block runs recursive DFS on a path graph (vertices `0—1—2—…`) in a child process with the default 8 MB stack: ten thousand and a hundred thousand vertices are fine; a million vertices overflows the stack and the process dies with a segmentation fault. The same graph traversed with the explicit-stack DFS needs only an array of `V` integers. Any code that traverses user-controlled or large graphs (dependency trees, file-system trees, parse trees, object graphs) should be iterative, or run on a thread with a stack sized for the worst case.

## Pitfalls

- **Marking visited too late.** In BFS, mark when you enqueue, not when you dequeue; otherwise vertices enter the queue many times and the algorithm can go quadratic or exponential on dense graphs.
- **Undirected edges stored once.** An undirected edge is two arcs in an adjacency structure; storing it once silently makes the graph directed. The CSR builder above counts both directions.
- **Self-loops and multi-edges.** They are legal inputs in most real data; decide what they mean for your algorithm (a self-loop is a cycle of length 1; multi-edges matter for flow and counting paths).
- **Dense-graph memory.** `V²` entries explode quickly; check before choosing a matrix.
- **Unstable vertex IDs.** Algorithms want vertices `0 … V−1`; map strings and sparse IDs through a hash table once, then work with integers.
- **Mutating while iterating.** Adding or removing edges during traversal invalidates iterators and CSR offsets; collect changes and apply them afterward.
- **Disconnected graphs.** A single BFS/DFS from one source reaches only its component; loops over all start vertices are required for "all vertices" answers.

## Key takeaways

1. Choose the representation from density and mutability: CSR for static sparse graphs (smallest, fastest), dynamic arrays for mutable ones, a matrix only for small or dense graphs.
2. BFS and DFS both run in `O(V + E)`; BFS gives layers and shortest unweighted paths, DFS gives edge classification, cycle detection and finish orders.
3. Mark a vertex when it is first discovered; always loop over all start vertices.
4. A directed graph is acyclic iff DFS has no back edge; Kahn's algorithm and reverse-finish-order DFS both produce topological orders.
5. A graph is bipartite iff it has no odd cycle; BFS two-colouring decides it in linear time.
6. Use an explicit stack for deep graphs: recursive DFS dies on a million-vertex path.
