---
title: "Shortest Paths: Dijkstra, Bellman-Ford, Floyd-Warshall and A*"
track: dsa
module: Graphs
moduleOrder: 6
order: 2
level: advanced
summary: "Relaxation and why each algorithm works, Dijkstra with an indexed heap vs lazy deletion (measured), Bellman-Ford and negative cycles, Floyd-Warshall (including the infinity bug), 0-1 BFS, A* on grids, and the OSPF SPF calculation with ECMP — cross-checked against each other on thousands of random graphs."
prerequisites:
  - Graph Representations and Traversals
  - Heaps and Priority Queues
sources:
  - label: "Dijkstra — A Note on Two Problems in Connexion with Graphs (Numerische Mathematik 1, 1959)"
  - label: "Bellman — On a Routing Problem (Quarterly of Applied Mathematics 16(1), 1958)"
  - label: "Floyd — Algorithm 97, Shortest Path (CACM 5(6), 1962)"
  - label: "Johnson — Efficient Algorithms for Shortest Paths in Sparse Networks (J. ACM 24(1), 1977)"
  - label: "Hart, Nilsson, Raphael — A Formal Basis for the Heuristic Determination of Minimum Cost Paths (IEEE Trans. SSC 4(2), 1968)"
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 22 (Single-Source Shortest Paths) and Ch. 23 (All-Pairs Shortest Paths)"
  - label: "Fredman & Tarjan — Fibonacci Heaps and Their Uses in Improved Network Optimization Algorithms (J. ACM 34(3), 1987)"
  - label: "Cherkassky, Goldberg, Radzik — Shortest Paths Algorithms, Theory and Experimental Evaluation (Math. Programming 73, 1996)"
  - label: "RFC 2328 — OSPF Version 2, §16.1 (Calculating the shortest-path tree for an area)"
    url: https://www.rfc-editor.org/rfc/rfc2328
  - label: "RFC 2453 — RIP Version 2"
    url: https://www.rfc-editor.org/rfc/rfc2453
---

Given a weighted graph, the **shortest-path problem** asks for minimum-weight paths — from one source to all vertices (**SSSP**), between one pair, or between all pairs (**APSP**). It is the computation inside every link-state routing protocol, every mapping application, and countless schedulers and planners. Which algorithm is right depends on three properties of the input: are the weights all equal (or 0/1), can any be negative, and is the graph dense or sparse?

| Situation | Algorithm | Time |
|---|---|---|
| Unweighted | BFS | `O(V + E)` |
| Weights 0 and 1 | 0-1 BFS (deque) | `O(V + E)` |
| Non-negative weights | Dijkstra | `O((V + E) log V)` with a binary heap |
| Negative weights, no negative cycle | Bellman–Ford | `O(V·E)` |
| Detect negative cycle | Bellman–Ford | `O(V·E)` |
| All pairs, dense or small `V` | Floyd–Warshall | `O(V³)` |
| All pairs, sparse, maybe negative edges | Johnson (Bellman–Ford once, then Dijkstra from every vertex) | `O(V·E log V)` |
| One pair, geometry/heuristic available | A* | much less than Dijkstra in practice |

## Relaxation: the one idea

Keep an upper bound `dist[v]` on the true distance (initially `∞`, and `0` for the source). **Relaxing** an edge `(u, v, w)` tightens it: `if dist[u] + w < dist[v] then dist[v] = dist[u] + w` and record `pred[v] = u`. Every algorithm here is just a different *order* of relaxations. Two facts make them correct: the **triangle inequality** (the true distances satisfy `δ(v) ≤ δ(u) + w(u,v)`) and **optimal substructure** (a prefix of a shortest path is itself a shortest path), so relaxing along the edges of a shortest path in order gives the exact distances however much other relaxation happens in between.

```c
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
```

## Dijkstra

Dijkstra's algorithm settles vertices in increasing order of distance: repeatedly extract the unsettled vertex with the smallest tentative distance, mark it final, and relax its outgoing edges. It is correct **because weights are non-negative**: when `u` is extracted, any other route to it would have to pass through some unsettled vertex whose distance is already at least `dist[u]`, and adding non-negative edges cannot make that route shorter. A single negative edge breaks the argument — a vertex already declared final could later be improved.

The cost is dominated by the priority queue. With a binary heap each extraction is `O(log V)` and each successful relaxation is a `decrease-key` (`O(log V)`): `O((V + E) log V)`. A Fibonacci heap improves this to `O(E + V log V)`, which rarely pays off in practice (heaps note). Two implementations are given:

- **Indexed heap with decrease-key**, reusing the structure from the heaps note: the heap never holds more than `V` entries.
- **Lazy deletion**: never decrease a key; push a *new* entry for every improvement and skip stale entries when popped (`top.d > dist[top.v]`). The heap can grow up to `E` entries, but the code is trivial and decrease-key's position bookkeeping disappears.

```c
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
```

### Cross-checking everything

The test runs 2000 random graphs (directed and undirected, up to 13 vertices) and from every source compares: Dijkstra (indexed), Dijkstra (lazy), Bellman–Ford, and Floyd–Warshall; 0-1 BFS against Dijkstra on 0/1 weights; and, with negative edge weights, Bellman–Ford against Floyd–Warshall, including a check that a reachable negative cycle is reported exactly when one exists.

```c
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
```

```text
$ gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined test_sssp.c -o test_sssp && ./test_sssp
2000 random graphs (15077 single-source runs): Dijkstra (indexed and lazy), Bellman-Ford and Floyd-Warshall agree; 0-1 BFS matches Dijkstra;
Bellman-Ford with negative edges matched Floyd-Warshall and flagged a reachable negative cycle in 664 graphs
```

**The test caught a real bug in the first draft of this code**: Floyd–Warshall with `INF` represented by a large number and *negative* edges. The update `d[i][k] + d[k][j]` with `d[k][j] = INF` gives `INF − 5`, which is smaller than `INF` and so looks like a (huge) real distance; unreachable pairs silently become "reachable at distance 1e18". The fix is the `dkj >= INF` check in `floyd` above (also skip `dik >= INF`). The general rule: when infinity is a sentinel, never add to it — or choose `INF = LLONG_MAX / 4` so that sums cannot overflow, **and** guard the comparison.

### Heap implementation matters more than theory suggests

```c
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
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra bench_sssp.c -o bench_sssp && ./bench_sssp
graph                         vertices       arcs | indexed ms   lazy ms | lazy: max heap size   
random denser (avg deg 16)     1000000   16000000 |      1693       929 |    1645553 (1.6x n) | farthest 185
random sparse (avg deg 8)      1000000    8000000 |      1177       737 |    1052386 (1.1x n) | farthest 387
1000x1000 grid (road-like)     1000000    3996000 |       130       110 |       2447 (0.0x n) | farthest 5218
```

Observe:

- On the random graphs the **lazy heap is about 1.5–1.8× faster** than the indexed heap with decrease-key, even though it processes up to `1.6×` more heap entries. A plain array-based heap has far better constants than one that must keep a position array in sync on every swap, and most relaxations in a random graph do not improve a vertex that is already queued often enough for decrease-key to pay. The size column shows the price: the lazy heap grows to a little over `V` entries.
- On the **grid ("road-like") graph** the frontier is small (a few thousand entries), the heap stays in cache, and the two variants finish within the same ballpark (the heap size column shows only a few thousand live entries, so heap work is a small part of the total).
- Both finish a million vertices and several million edges in around a second or less — fast enough that for most uses the simple lazy version is the right default. Decrease-key matters when the number of improvements per vertex is large (dense graphs, other problems like Prim's algorithm on dense inputs).

## Bellman–Ford and negative weights

Bellman–Ford relaxes **every edge**, `V − 1` times. After pass `i` all shortest paths with at most `i` edges are correct, and a simple path has at most `V − 1` edges. If a further pass still improves some distance, a **negative cycle** is reachable from the source and shortest paths are undefined (you can loop forever lowering the cost). It stops early when a pass changes nothing.

It is slower (`O(V·E)`) but handles negative weights, and — more importantly for networking — it is the algorithm that *distributes*. In a **distance-vector** protocol such as RIP (see *Distance-Vector Routing and RIP*), each router repeatedly relaxes "my cost to destination `d` via neighbour `n` is `n`'s advertised cost plus the link": exactly one Bellman–Ford pass over the network per update interval. Its known failure mode is also Bellman–Ford's: without negative edges but with a topology change, costs can "count to infinity", which RIP bounds by declaring 16 hops unreachable (RFC 2453).

Negative edges are rare in practice, but they appear in currency arbitrage detection (log-transformed exchange rates produce negative cycles), difference-constraint systems in schedulers and static analysis, and as reduced edge weights inside Johnson's algorithm.

## Floyd–Warshall and Johnson

Floyd–Warshall computes all-pairs distances with three nested loops: for each intermediate vertex `k`, try improving every `d[i][j]` via `k`. The invariant after iteration `k` is that `d[i][j]` is the shortest path using only intermediate vertices from `{0 … k}`. It is `O(V³)` time and `O(V²)` memory, but the inner loop is a tight, branch-light, vectorizable scan, which makes it competitive for dense graphs up to a few thousand vertices. A negative value on the diagonal afterwards signals a negative cycle. For large sparse graphs, **Johnson's algorithm** reweights edges with potentials from one Bellman–Ford run (making all weights non-negative without changing which paths are shortest) and then runs Dijkstra from every vertex: `O(V·E log V)`.

## 0-1 BFS

When weights are only 0 or 1, a double-ended queue replaces the heap: relaxing a 0-edge pushes the vertex to the **front**, a 1-edge to the **back**, which keeps the deque sorted by distance. Linear time, and common in practice for "minimum number of obstacles/switches/turns" problems. It is cross-checked with Dijkstra in the test above.

## A*: shortest path with a sense of direction

Dijkstra expands vertices in all directions in order of distance from the source. If you know roughly where the target is, **A\*** orders expansion by `f(v) = g(v) + h(v)`, where `g` is the distance so far and `h(v)` is a heuristic *lower bound* on the remaining distance. If `h` is **admissible** (never overestimates) the first time the target is extracted its path is optimal, and if it is also **consistent** (`h(u) ≤ w(u,v) + h(v)`) no vertex is ever expanded twice. Manhattan distance is a consistent heuristic on a 4-connected unit-cost grid. `h = 0` is Dijkstra; an overestimating `h` is faster but no longer exact (the idea behind weighted A*).

The program compares BFS (exact for unit costs), Dijkstra and A* on a 2000×2000 grid (four million cells) with increasing random obstacle density, asserting that all three find the same optimal length wherever a path exists, and counting expanded cells.

```c
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

/* Shortest path on a 4-connected grid with obstacles: BFS vs Dijkstra-with-heap vs A* (Manhattan heuristic).
 * All unit-cost moves, so BFS is exact; the point is HOW MANY cells each algorithm has to expand. */
typedef struct { int f, cell; } Item;
static void up(Item *h, size_t i) { Item x = h[i]; while (i && h[(i - 1) / 2].f > x.f) { h[i] = h[(i - 1) / 2]; i = (i - 1) / 2; } h[i] = x; }
static void down(Item *h, size_t n, size_t i) {
    Item x = h[i];
    for (;;) { size_t c = 2 * i + 1; if (c >= n) break; if (c + 1 < n && h[c + 1].f < h[c].f) c++; if (x.f <= h[c].f) break; h[i] = h[c]; i = c; }
    h[i] = x;
}

static int W, H; static char *wall;
static int manhattan(int a, int b) { return abs(a % W - b % W) + abs(a / W - b / W); }

/* weighted search with heuristic weight hw: hw = 0 -> Dijkstra, hw = 1 -> A* (consistent heuristic -> optimal) */
static int search(int src, int dst, int hw, size_t *expanded) {
    int N = W * H; int *g = malloc((size_t)N * sizeof *g); char *closed = calloc((size_t)N, 1); for (int i = 0; i < N; i++) g[i] = 1 << 30;
    Item *heap = malloc((size_t)N * 4 * sizeof *heap);
    size_t n = 0;
    *expanded = 0;
    g[src] = 0; heap[n++] = (Item){ hw * manhattan(src, dst), src };
    static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    int ans = -1;
    while (n) {
        Item top = heap[0]; heap[0] = heap[--n]; if (n) down(heap, n, 0);
        if (closed[top.cell]) continue;
        closed[top.cell] = 1;
        (*expanded)++;
        if (top.cell == dst) {
            ans = g[dst];
            break;
        }
        int x = top.cell % W, y = top.cell / W;
        for (int d = 0; d < 4; d++) {
            int nx = x + dx[d], ny = y + dy[d]; if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            int v = ny * W + nx; if (wall[v] || closed[v]) continue;
            if (g[top.cell] + 1 < g[v]) { g[v] = g[top.cell] + 1; heap[n] = (Item){ g[v] + hw * manhattan(v, dst), v }; up(heap, n++); }
        }
    }
    free(g); free(closed); free(heap); return ans;
}
static int bfs(int src, int dst, size_t *expanded) {
    int N = W * H; int *dist = malloc((size_t)N * sizeof *dist), *q = malloc((size_t)N * sizeof *q); for (int i = 0; i < N; i++) dist[i] = -1;
    int h = 0, t = 0; q[t++] = src; dist[src] = 0; *expanded = 0; int ans = -1;
    static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    while (h < t) {
        int u = q[h++]; (*expanded)++; if (u == dst) { ans = dist[u]; break; }
        for (int d = 0; d < 4; d++) { int nx = u % W + dx[d], ny = u / W + dy[d]; if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue; int v = ny * W + nx; if (!wall[v] && dist[v] < 0) { dist[v] = dist[u] + 1; q[t++] = v; } }
    }
    free(dist); free(q); return ans;
}

int main(void) {
    W = H = 2000; wall = calloc((size_t)W * H, 1); srand(5);
    printf("%dx%d grid, source top-left, target bottom-right; each row is a different obstacle density\n\n", W, H);
    printf("%-10s %-10s | %9s | %12s %8s | %12s %8s | %12s %8s\n", "obstacles", "", "path len", "BFS cells", "ms", "Dijkstra", "ms", "A*", "ms");
    int densities[] = { 0, 20, 30, 35 };
    for (int di = 0; di < 4; di++) {
        for (int i = 0; i < W * H; i++) wall[i] = rand() % 100 < densities[di];
        wall[0] = wall[1] = wall[W] = wall[W * H - 1] = wall[W * H - 2] = wall[W * H - 1 - W] = 0;   /* keep the endpoints' neighbours open */
        size_t e1, e2, e3; double t0;
        t0 = now(); int a = bfs(0, W * H - 1, &e1); double t1 = now() - t0;
        t0 = now(); int b = search(0, W * H - 1, 0, &e2); double t2 = now() - t0;
        t0 = now(); int c = search(0, W * H - 1, 1, &e3); double t3 = now() - t0;
        assert(a == b && b == c);                                         /* the same optimal length, found three ways */
        printf("%8d %% %-10s | %9d | %12zu %8.0f | %12zu %8.0f | %12zu %8.0f\n", densities[di], "", a, e1, t1 * 1e3, e2, t2 * 1e3, e3, t3 * 1e3);
    }
    printf("\n(path length -1 means the target is unreachable; equal lengths in every row were asserted)\n");
    free(wall); return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra astar.c -o astar && ./astar
2000x2000 grid, source top-left, target bottom-right; each row is a different obstacle density

obstacles             |  path len |    BFS cells       ms |     Dijkstra       ms |           A*       ms
       0 %            |      3998 |      4000000      118 |      4000000      435 |      1429328      171
      20 %            |      3998 |      3193230      173 |      3193230      482 |       396957       53
      30 %            |      4006 |      2752269      172 |      2752269      354 |       391138       42
      35 %            |        -1 |      2465165      152 |      2465165      340 |      2465165      300

(path length -1 means the target is unreachable; equal lengths in every row were asserted)
```

- With **no obstacles**, BFS and Dijkstra expand all four million cells, since the target is in the far corner; A* expands about a third as many (the Manhattan heuristic ties along every monotone path, so it still explores a diagonal band — breaking ties toward larger `g` shrinks this further).
- With 20–30 % obstacles, A* expands about **seven to eight times fewer cells** and is roughly 6–10× faster than Dijkstra, because the heuristic points the search through the open area toward the goal instead of flooding the whole map.
- When there is **no path** (the 35 % row reports length −1), A* is no help at all: it must exhaust the entire reachable component before it can say so, like the others. Heuristics speed up success, not failure.
- Dijkstra is slower than BFS here although both expand the same cells: it pays for heap operations that BFS replaces with a queue. When every move costs the same, use BFS or A*.

Real systems extend this: **bidirectional search**, **ALT** (A* with landmarks and the triangle inequality for the heuristic), and **contraction hierarchies** (precomputed shortcuts) are how road-network routers answer continent-scale queries in milliseconds.

## Link-state routing: SPF in OSPF and IS-IS

Every router in an OSPF area (and every IS-IS level) holds the same link-state database, builds the graph, and runs Dijkstra with itself as the root — the "SPF calculation" in RFC 2328 §16.1 (see *OSPFv2 Operation: Neighbors, DR/BDR, Flooding and SPF*). Link costs are the non-negative interface metrics, so Dijkstra applies directly. Two additions matter for routers:

- Besides the distance, each vertex records its **next hop(s)** — the first-hop neighbours of the root on the shortest paths. When several paths tie, all first hops are kept: **ECMP**, which the forwarding table then load-balances over.
- The result is a tree of next hops, installed in the RIB/FIB; the SPF is rerun (after a throttle delay) whenever the LSDB changes.

The program below computes the SPF tree for a seven-router topology with the classic array-based `O(V²)` Dijkstra (as in the RFC's pseudocode), merging next-hop sets on equal-cost ties.

```c
#include <stdio.h>
#include <string.h>

/* The SPF calculation of a link-state protocol (OSPF/IS-IS) on a small topology: Dijkstra from the local router, tracking for each
 * destination the SET of first-hop neighbours on ALL equal-cost shortest paths (ECMP).  O(V^2) array version, as in RFC 2328 Sec. 16.1. */
#define N 7
static const char *name[N] = { "R1", "R2", "R3", "R4", "R5", "R6", "R7" };
static const int cost[N][N] = {                       /* symmetric link costs, 0 = no link */
    /*R1*/ { 0, 10, 10,  0,  0,  0,  0 },
    /*R2*/ {10,  0,  0, 10,  0,  0,  0 },
    /*R3*/ {10,  0,  0, 10,  0,  0,  0 },
    /*R4*/ { 0, 10, 10,  0, 10,  0,  0 },
    /*R5*/ { 0,  0,  0, 10,  0, 20, 10 },
    /*R6*/ { 0,  0,  0,  0, 20,  0, 10 },
    /*R7*/ { 0,  0,  0,  0, 10, 10,  0 } };

int main(void) {
    int dist[N], done[N] = {0}; unsigned nexthops[N] = {0};            /* bitmask of first-hop neighbours of R1 */
    for (int i = 0; i < N; i++) dist[i] = 1 << 30;
    dist[0] = 0;
    for (int iter = 0; iter < N; iter++) {
        int u = -1; for (int i = 0; i < N; i++) if (!done[i] && (u < 0 || dist[i] < dist[u])) u = i;
        if (u < 0 || dist[u] >= 1 << 30) break;
        done[u] = 1;
        for (int v = 0; v < N; v++) {
            if (!cost[u][v] || done[v]) continue;
            int nd = dist[u] + cost[u][v];
            unsigned via = u == 0 ? 1u << v : nexthops[u];             /* a neighbour of the root is its own first hop; others inherit */
            if (nd < dist[v]) { dist[v] = nd; nexthops[v] = via; }
            else if (nd == dist[v]) nexthops[v] |= via;                /* equal-cost path: ECMP, union the first hops */
        }
    }
    printf("SPF tree rooted at R1\n%-6s %-6s %s\n", "dest", "cost", "next hop(s)");
    for (int i = 1; i < N; i++) {
        printf("%-6s %-6d", name[i], dist[i]);
        for (int v = 0; v < N; v++) if (nexthops[i] >> v & 1) printf(" %s", name[v]);
        printf("\n");
    }
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra spf.c -o spf && ./spf
SPF tree rooted at R1
dest   cost   next hop(s)
R2     10     R2
R3     10     R3
R4     20     R2 R3
R5     30     R2 R3
R6     50     R2 R3
R7     40     R2 R3
```

`R4` is reachable at cost 20 through both `R2` and `R3` (two equal-cost paths through different neighbours), so R1 installs both next hops for `R4` and everything behind it. `R6` shows a subtlety: its best cost 50 is reached both via `R5` (30 + 20) and via `R7` (40 + 10); both routes share the same first hops so the next-hop set is the same, but a router implementing per-path accounting would keep two distinct paths — as would multipath-aware weights. BGP, by contrast, is not a shortest-path algorithm: it is *path-vector* with policy (see the BGP notes).

## Pitfalls

- **Dijkstra with negative edges** gives wrong answers silently — validate input weights, or use Bellman–Ford.
- **Infinity arithmetic**: `INF + w` overflow, or `INF + negative` looking finite (the Floyd–Warshall bug above). Guard comparisons; use a sentinel with headroom.
- **Stale heap entries** in lazy Dijkstra: forgetting the `top.d > dist[top.v]` skip makes it re-expand vertices and can turn linear work into quadratic.
- **Overflow of path sums**: use 64-bit distances for graphs with large weights or many edges.
- **A\* with an inadmissible heuristic** (for example Euclidean distance on a graph whose edge costs are smaller than the geometric distance) returns suboptimal paths without any warning.
- **Zero-weight cycles** are fine for Dijkstra and Bellman–Ford but make "number of shortest paths" counting infinite.
- **Equal-cost paths**: decide whether you need one path, all next hops (ECMP), or a canonical tie-break for determinism.

## Key takeaways

1. All shortest-path algorithms are orders of relaxation; correctness rests on the triangle inequality and optimal substructure.
2. Choose by weights: BFS for equal weights, 0-1 BFS for 0/1, Dijkstra for non-negative, Bellman–Ford for negative edges and cycle detection, Floyd–Warshall or Johnson for all pairs.
3. Dijkstra needs non-negative weights; in practice a binary heap with lazy deletion often beats decrease-key.
4. Bellman–Ford is also the algorithm distance-vector protocols (RIP) run in distributed form; link-state protocols (OSPF, IS-IS) run Dijkstra on the LSDB and keep ECMP next-hop sets.
5. A* with an admissible, consistent heuristic is optimal and can expand an order of magnitude fewer vertices, but gives no advantage when the target is unreachable.
6. Test against an independent algorithm on random small graphs: the infinity-plus-negative bug only shows up there.
