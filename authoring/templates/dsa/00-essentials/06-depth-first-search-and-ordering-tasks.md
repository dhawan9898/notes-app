---
title: "Depth-First Search and Ordering Tasks"
track: dsa
module: DSA Essentials
moduleOrder: 0.5
order: 6
level: beginner
summary: "Depth-first search explained with a worked trace: going deep, backing up, why it needs a visited marker, how it finds separate components, and how the order in which vertices finish gives a topological ordering of tasks, with a compiled C program."
prerequisites:
  - "Trees and Graphs for Beginners"
  - "More Control Flow: switch, Loop Control and Recursion"
sources:
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., §20.3 (Depth-first search) and §20.4 (Topological sort)
  - label: Sedgewick & Wayne — Algorithms, 4th ed., §4.1 (Depth-first search) and §4.2 (Topological sort)
  - label: Tarjan, R. — Depth-first search and linear graph algorithms (SIAM J. Computing, 1972)
---

*Trees and Graphs for Beginners* introduced breadth-first search (BFS), which explores a graph in rings, nearest first. **Depth-first search (DFS)** is its mirror image: follow one path as far as it goes, and only when you hit a dead end, back up and try the next branch. It is the basis of cycle detection, finding connected pieces, ordering tasks, and solving mazes and puzzles by backtracking.

## The idea: go deep, then back up

Imagine exploring a maze with a ball of string. At every junction you take an unexplored corridor and keep going; at a dead end (or a place you have already been) you retrace your steps to the last junction with an unexplored corridor. In code, the retracing is free: a **recursive function** that calls itself for each unvisited neighbour remembers where to resume, using the call stack (see *More Control Flow* for how recursion uses the stack).

{{include dsa/essentials/dfs_topo.c}}

{{run dsa/essentials :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o dfs_topo dfs_topo.c && ./dfs_topo; rm -f dfs_topo}}

## Reading the trace

The graph has arrows A→B, A→C, B→D, C→D, D→E and, separately, F→G. The program prints each vertex when it is first reached, indented by its depth:

1. Start at **A**. Its first neighbour is B, so go deeper to **B**; B's neighbour is D, so go to **D**, then **E**.
2. E has no outgoing edges: dead end. Back up to D (nothing else), back to B (nothing else), back to A.
3. A's next neighbour is **C**. C's neighbour D is already visited, so C is a dead end too. Back at A there is nothing left, so DFS from A is finished.

Two details are essential:

- **The visited marker.** `visited[]` stops the search from entering D a second time (from C), and, in graphs with cycles, from walking around the cycle forever. Mark a vertex the moment you enter it.
- **Unreached vertices.** F and G were never visited because no path leads to them from A: they are a different **connected component**. To cover a whole graph, loop over every vertex and start a new DFS from each one that is still unvisited; the number of fresh starts is the number of components. That loop appears in the second half of the program.

## BFS or DFS?

| Question | Better choice |
|---|---|
| Shortest path measured in edges | BFS |
| Is there any path / what is reachable? | either |
| Cycle detection, topological order, connected components | DFS |
| Exploring all possibilities (puzzles, mazes, backtracking) | DFS |
| Memory use on wide graphs | DFS (stack depth ≈ path length) |
| Memory use on deep graphs | BFS (DFS recursion can overflow the stack) |

Both visit each vertex and each edge once: O(V + E) with adjacency lists (O(V²) with the matrix used here).

## From DFS to an ordering of tasks

Many real problems are "do these things, but some must come before others": compiling files that depend on headers, installing packages, taking courses with prerequisites. Model the tasks as a graph with an arrow *before → after*. A valid order, with every prerequisite before the task that needs it, is a **topological order**, and DFS finds one almost for free:

- Record each vertex **after** all the vertices it points to have been finished (the `order[pos++] = v` line placed *after* the loop over neighbours).
- **Reverse** that finishing order.

Why it works: a vertex finishes only after everything it points to has finished, so in the finishing order every vertex appears *after* its successors; reversing puts it *before* them. The program's last line prints one valid order, `F G A C B D E`: A before B and C, B and C before D, D before E, and F before G. Several valid orders exist (the earlier BFS-based Kahn algorithm in *Practice: Graph Algorithms* would give another), and any of them is correct.

**It only works on a DAG** (a directed acyclic graph). If tasks depend on each other in a circle ("A needs B needs A"), no order exists. DFS reveals this: during the search, if you meet a vertex that is *currently on the active path* (entered but not yet finished), you have found a cycle. Production code tracks three states per vertex (unvisited, in progress, finished) to detect it.

## Pitfalls

- **Missing visited marker**: infinite recursion on any graph with a cycle.
- **Recursion depth**: a path of a million vertices overflows the stack; use an explicit stack (as in the flood-fill exercise) for large inputs.
- **Starting only from one vertex** and assuming it covered the graph.
- **Forgetting to reverse** the finishing order for a topological sort.
- **Calling a cyclic graph's order "valid"**: check for cycles first.
- **Assuming the order is unique**: tests should check that every prerequisite comes first, not compare with one expected list.

## Key takeaways

1. DFS goes as deep as possible, then backs up; recursion remembers where to resume, and a visited marker prevents repeats and endless loops.
2. Restarting DFS from each unvisited vertex finds every connected component.
3. Use BFS for shortest paths by edge count and DFS for cycles, components, topological order and exhaustive search.
4. Reversing DFS finishing order gives a topological order of a DAG; meeting a vertex that is still in progress means there is a cycle.
