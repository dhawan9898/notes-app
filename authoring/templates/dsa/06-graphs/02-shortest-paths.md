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
  - label: Dijkstra — A Note on Two Problems in Connexion with Graphs (Numerische Mathematik 1, 1959)
  - label: Bellman — On a Routing Problem (Quarterly of Applied Mathematics 16(1), 1958)
  - label: Floyd — Algorithm 97, Shortest Path (CACM 5(6), 1962)
  - label: Johnson — Efficient Algorithms for Shortest Paths in Sparse Networks (J. ACM 24(1), 1977)
  - label: Hart, Nilsson, Raphael — A Formal Basis for the Heuristic Determination of Minimum Cost Paths (IEEE Trans. SSC 4(2), 1968)
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 22 (Single-Source Shortest Paths) and Ch. 23 (All-Pairs Shortest Paths)
  - label: Fredman & Tarjan — Fibonacci Heaps and Their Uses in Improved Network Optimization Algorithms (J. ACM 34(3), 1987)
  - label: Cherkassky, Goldberg, Radzik — Shortest Paths Algorithms, Theory and Experimental Evaluation (Math. Programming 73, 1996)
  - label: RFC 2328 — OSPF Version 2, §16.1 (Calculating the shortest-path tree for an area)
    url: https://www.rfc-editor.org/rfc/rfc2328
  - label: RFC 2453 — RIP Version 2
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

{{include dsa/paths/wgraph.h}}

## Dijkstra

Dijkstra's algorithm settles vertices in increasing order of distance: repeatedly extract the unsettled vertex with the smallest tentative distance, mark it final, and relax its outgoing edges. It is correct **because weights are non-negative**: when `u` is extracted, any other route to it would have to pass through some unsettled vertex whose distance is already at least `dist[u]`, and adding non-negative edges cannot make that route shorter. A single negative edge breaks the argument — a vertex already declared final could later be improved.

The cost is dominated by the priority queue. With a binary heap each extraction is `O(log V)` and each successful relaxation is a `decrease-key` (`O(log V)`): `O((V + E) log V)`. A Fibonacci heap improves this to `O(E + V log V)`, which rarely pays off in practice (heaps note). Two implementations are given:

- **Indexed heap with decrease-key**, reusing the structure from the heaps note: the heap never holds more than `V` entries.
- **Lazy deletion**: never decrease a key; push a *new* entry for every improvement and skip stale entries when popped (`top.d > dist[top.v]`). The heap can grow up to `E` entries, but the code is trivial and decrease-key's position bookkeeping disappears.

{{include dsa/paths/sssp.h}}

### Cross-checking everything

The test runs 2000 random graphs (directed and undirected, up to 13 vertices) and from every source compares: Dijkstra (indexed), Dijkstra (lazy), Bellman–Ford, and Floyd–Warshall; 0-1 BFS against Dijkstra on 0/1 weights; and, with negative edge weights, Bellman–Ford against Floyd–Warshall, including a check that a reachable negative cycle is reported exactly when one exists.

{{include dsa/paths/test_sssp.c}}

{{run dsa/paths :: gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined test_sssp.c -o test_sssp && ./test_sssp}}

**The test caught a real bug in the first draft of this code**: Floyd–Warshall with `INF` represented by a large number and *negative* edges. The update `d[i][k] + d[k][j]` with `d[k][j] = INF` gives `INF − 5`, which is smaller than `INF` and so looks like a (huge) real distance; unreachable pairs silently become "reachable at distance 1e18". The fix is the `dkj >= INF` check in `floyd` above (also skip `dik >= INF`). The general rule: when infinity is a sentinel, never add to it — or choose `INF = LLONG_MAX / 4` so that sums cannot overflow, **and** guard the comparison.

### Heap implementation matters more than theory suggests

{{include dsa/paths/bench_sssp.c}}

{{run dsa/paths :: gcc -std=gnu11 -O2 -Wall -Wextra bench_sssp.c -o bench_sssp && ./bench_sssp}}

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

{{include dsa/paths/astar.c}}

{{run dsa/paths :: gcc -std=gnu11 -O2 -Wall -Wextra astar.c -o astar && ./astar}}

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

{{include dsa/paths/spf.c}}

{{run dsa/paths :: gcc -std=gnu11 -O2 -Wall -Wextra spf.c -o spf && ./spf}}

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
