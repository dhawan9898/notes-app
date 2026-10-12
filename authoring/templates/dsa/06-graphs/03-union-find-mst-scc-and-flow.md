---
title: "Union-Find, Spanning Trees, Connectivity and Max-Flow"
track: dsa
module: Graphs
moduleOrder: 6
order: 3
level: expert
summary: "Disjoint sets with path compression and union by rank (measured), Kruskal and Prim, Tarjan's strongly connected components, bridges and articulation points, and max-flow/min-cut with Edmonds-Karp, Dinic and bipartite matching — each verified against brute force."
prerequisites:
  - Graph Representations and Traversals
sources:
  - label: Tarjan — Efficiency of a Good But Not Linear Set Union Algorithm (J. ACM 22(2), 1975)
  - label: Kruskal — On the Shortest Spanning Subtree of a Graph and the Traveling Salesman Problem (Proc. AMS 7(1), 1956)
  - label: Prim — Shortest Connection Networks and Some Generalizations (Bell System Technical Journal 36, 1957)
  - label: Tarjan — Depth-First Search and Linear Graph Algorithms (SIAM J. Computing 1(2), 1972)
  - label: Ford & Fulkerson — Maximal Flow Through a Network (Canadian J. Mathematics 8, 1956)
  - label: Edmonds & Karp — Theoretical Improvements in Algorithmic Efficiency for Network Flow Problems (J. ACM 19(2), 1972)
  - label: Dinitz — Algorithm for Solution of a Problem of Maximum Flow in a Network with Power Estimation (Soviet Math. Doklady 11, 1970)
  - label: Hopcroft & Karp — An n^(5/2) Algorithm for Maximum Matchings in Bipartite Graphs (SIAM J. Computing 2(4), 1973)
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 19 (Data Structures for Disjoint Sets), Ch. 21 (Minimum Spanning Trees), Ch. 22.5 (Strongly connected components), Ch. 24 (Maximum Flow)
---

This note collects four graph toolkits that sit on top of traversal: **disjoint sets** (a tiny structure that answers "are these in the same group?" in practically constant time), **minimum spanning trees** (the cheapest way to connect everything), **connectivity analysis** (strongly connected components, bridges, articulation points) and **network flow** (how much can be sent through a capacitated network, and what is the bottleneck).

## Union–find (disjoint-set union)

The structure maintains a partition of `{0 … n−1}` into sets under two operations: `find(x)` returns a canonical representative of x's set; `union(a, b)` merges two sets. Each set is a tree stored with parent pointers; the root is the representative. Two cheap heuristics make it nearly constant-time:

- **Union by rank (or size):** hang the shallower tree beneath the deeper one, so tree depth stays `O(log n)`.
- **Path compression:** during `find`, repoint every node on the search path directly at the root, flattening the tree for later queries.

With both, a sequence of `m` operations on `n` elements costs `O(m · α(n))`, where `α` is the inverse Ackermann function — below 5 for any `n` that fits in the universe (Tarjan, 1975, who also proved this is optimal). The code lets the heuristics be switched on and off so their effect can be measured.

{{include dsa/mst/dsu.h}}

{{include dsa/mst/bench_dsu.c}}

{{run dsa/mst :: gcc -std=gnu11 -O2 -Wall -Wextra bench_dsu.c -o bench_dsu && ./bench_dsu}}

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

{{include dsa/mst/mst.c}}

{{run dsa/mst :: gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined mst.c -o mst_asan && ./mst_asan | head -1 && gcc -std=gnu11 -O2 -Wall -Wextra mst.c -o mst && ./mst}}

The first line comes from the sanitizer build, which runs the random small-graph test: 1500 graphs where Kruskal and Prim are both compared with the **exhaustive minimum over all spanning trees** (every subset of `n − 1` edges is tried). The table is the optimized build on a million-vertex random graph: the two algorithms are within a small factor of each other, each limited by the same random memory access, with the sort dominating Kruskal on dense inputs.

**Networking note.** The *Spanning Tree: STP, RSTP and MSTP* note solves a different problem despite the name: STP elects a root bridge and builds a *shortest-path tree toward it* (by path cost) so that a Layer-2 network is loop-free; it does not minimise total link cost. MST algorithms are what you use to *design* a network (cheapest set of links connecting all sites), in clustering (single-linkage clustering is a Kruskal run), and as an approximation for Steiner-tree and travelling-salesman problems.

## Strongly connected components

In a directed graph, `u` and `v` are in the same **strongly connected component** (SCC) if each can reach the other. Collapsing each SCC into one vertex yields the **condensation**, always a DAG — which is why SCC is the first step in analyses such as: cycle detection in dependency graphs and call graphs (a component of size > 1 is a cycle), reachability for 2-SAT, dead-code and recursion analysis in compilers, and garbage-collection unreachability in cyclic structures.

**Tarjan's algorithm** finds all SCCs in one DFS. Each vertex gets a discovery index and a **low-link**: the smallest index reachable from its DFS subtree using tree edges followed by at most one edge to a vertex still on the component stack. When a vertex's low-link equals its own index it is the root of an SCC: pop the stack down to it. **Kosaraju's algorithm** does two passes (DFS for finish order, then DFS on the transposed graph in decreasing finish order) and is simpler to prove. The components come out in **reverse topological order** of the condensation — a useful property the test below verifies.

## Bridges and articulation points

In an undirected graph, a **bridge** is an edge whose removal disconnects the graph, and an **articulation point** (cut vertex) is a vertex whose removal does. These are the *single points of failure*: the link or router that, if it dies, partitions the network. The same DFS low-link idea finds them in `O(V + E)`: an edge `(p, u)` is a bridge if nothing in `u`'s subtree has a back edge to `p` or above (`low[u] > tin[p]`), and a non-root `p` is an articulation point if some child's subtree cannot bypass it (`low[u] ≥ tin[p]`); the DFS root is one iff it has two or more children. Edge identifiers, not endpoint comparisons, are used to skip the parent edge so parallel links are handled correctly (two parallel links are not a bridge).

Network engineers use this directly: audit a topology for bridges and cut vertices (and then add redundant links), or check that an L2 fabric survives any single failure (see the *Resilience* module notes on FHRP and BFD).

{{include dsa/mst/scc.c}}

{{run dsa/mst :: gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined scc.c -o scc_asan && ./scc_asan}}

All three DFS-based algorithms are **iterative** with explicit stacks and cursors — the last line runs a directed cycle of two million vertices, where a recursive DFS would overflow the stack (previous note). Correctness is checked on 3000 random small graphs: SCC membership equals mutual reachability from a transitive closure; bridges and articulation points equal the brute-force answer "delete it and recount the components"; and every arc leads to an equal-or-lower component index.

## Maximum flow and minimum cut

A **flow network** has a capacity on each arc, a source `s` and a sink `t`. A **flow** respects capacities and conserves flow at every other vertex. The **max-flow min-cut theorem** (Ford & Fulkerson) says the largest flow equals the capacity of the smallest **cut** (a partition of the vertices with `s` on one side and `t` on the other, counting only arcs leaving `s`'s side). The proof is also the algorithm: while a path with spare capacity exists, push flow along it; the **residual graph** records what is left, and also lets later steps *undo* earlier flow by pushing along reverse arcs. When no augmenting path remains, the vertices still reachable from `s` define a minimum cut.

- **Ford–Fulkerson** with arbitrary augmenting paths can be slow (even non-terminating with irrational capacities).
- **Edmonds–Karp** always augments along the path with the fewest arcs (BFS), bounding augmentations by `O(VE)` and time by `O(VE²)`.
- **Dinic** builds a *level graph* by BFS and sends a **blocking flow** through it with DFS, repeating until `t` is unreachable: `O(V²E)` in general and `O(E√V)` on unit-capacity networks such as bipartite matching. It is the usual choice in practice.
- **Push–relabel** (Goldberg–Tarjan) and, more recently, almost-linear-time algorithms are asymptotically faster.

**Applications**: bipartite matching (source → left vertices → right vertices → sink, all capacities 1: the flow value is the maximum matching size, and **Hopcroft–Karp** is the specialised `O(E√V)` algorithm); assignment and scheduling; image segmentation (min-cut separates foreground from background); network capacity and survivability (the min-cut between two sites is the number of edge-disjoint paths — Menger's theorem); and project selection.

{{include dsa/mst/flow.c}}

{{run dsa/mst :: gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined flow.c -o flow_asan && ./flow_asan | head -2 && gcc -std=gnu11 -O2 -Wall -Wextra flow.c -o flow && ./flow}}

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
