---
title: Graph Representations and Traversals
track: dsa
module: Graphs
moduleOrder: 6
order: 1
level: intermediate
summary: "Adjacency matrix vs list vs CSR (measured), BFS and DFS with edge classification, components, bipartite checks, topological sorting two ways, cycle reporting for dependency graphs, and why recursive DFS crashes on long paths."
sources:
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 20 (Elementary Graph Algorithms)
  - label: Tarjan — Depth-First Search and Linear Graph Algorithms (SIAM J. Computing 1(2), 1972)
  - label: Kahn — Topological Sorting of Large Networks (CACM 5(11), 1962)
  - label: Knuth — The Art of Computer Programming, Vol. 1, §2.3.4 (Basic Mathematical Properties of Trees) and §2.2.3 (Linked Allocation, topological sort)
  - label: Sedgewick — Algorithms in C, Part 5 Graph Algorithms, 3rd ed.
  - label: systemd.unit(5) — Requires=, After= and ordering dependencies
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

{{include dsa/graphs/graph.h}}

### Measuring the difference

BFS on a random undirected graph with one million vertices and eight million edges — 16 million directed arcs — on CSR and on two linked-list variants: nodes carved sequentially from one pool, and one `malloc` per arc.

{{include dsa/graphs/bench_graph.c}}

{{run dsa/graphs :: gcc -std=gnu11 -O2 -Wall -Wextra bench_graph.c -o bench_graph && ./bench_graph}}

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

{{include dsa/graphs/algos.c}}

{{run dsa/graphs :: gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined algos.c -o algos && ./algos}}

The small graph shows the signature difference: BFS visits `0 1 2 3 4 5` (by layer), DFS goes `0 1 3 5 4 2` (deep first). Then 300 random graphs on up to 15 vertices are checked against brute force: components and distances against Floyd–Warshall reachability, bipartiteness against trying all `2^n` colourings, and both topological sorts against each other and against the definition (every edge goes forward in the produced order).

## Dependency resolution

Topological sorting is the algorithm behind `make`, package managers, build tools (Bazel, Ninja, Cargo), spreadsheet recalculation and **systemd's start-up ordering**: `After=` and `Before=` add ordering edges between units, `Requires=` / `Wants=` add pulling edges, and systemd computes a transaction by ordering the units; a cycle in ordering dependencies is reported and systemd breaks it by dropping jobs. The example below reads `target: dependency…` lines and either prints a valid order or finds and prints the actual cycle by walking DFS parent links from the back edge — the diagnostic a user actually needs.

{{include dsa/graphs/deps.c}}

{{run dsa/graphs :: gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined deps.c -o deps && ./deps}}

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
