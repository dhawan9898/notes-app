---
title: "Practice: Graph Algorithms"
track: dsa
module: Graphs
moduleOrder: 6
order: 4
level: advanced
summary: "Four exercises on the graphs module: counting islands in a grid with flood fill, the shortest path through a maze with breadth-first search, ordering dependent tasks with a topological sort that detects cycles, and Dijkstra's algorithm on a small weighted graph, each with a verified C answer."
prerequisites:
  - "Graph Representations and Traversals"
  - "Shortest Paths: Dijkstra, Bellman-Ford, Floyd-Warshall and A*"
  - "Union-Find, Spanning Trees, Connectivity and Max-Flow"
sources:
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 20 (Elementary Graph Algorithms) and Ch. 22 (Single-Source Shortest Paths)
  - label: Kahn, A. B. — Topological sorting of large networks (CACM, 1962)
  - label: Skiena — The Algorithm Design Manual, 3rd ed., Ch. 7-8
---

Many "different" problems are the same graph problem in disguise: a grid is a graph, a set of tasks with prerequisites is a graph, a road map is a graph. Each exercise below asks you to recognise which graph algorithm applies. (Answers were also cross-checked against an independent Python implementation.)

## Exercises

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Islands.** A 6×8 grid contains `1` (land) and `0` (water). Count the islands (groups of land cells connected up, down, left or right) and the size of the largest. Which traversal do you use, and what happens to your marks if you reuse the grid afterwards?

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/graphs/ex1-islands.c}}

</details>

**2. Shortest path in a maze.** `S` is the start, `E` the exit, `#` a wall and `.` a free cell; moves go up, down, left or right at cost 1. Find the number of steps of the shortest path. Why does BFS give the shortest path here and DFS does not?

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/graphs/ex2-maze.c}}

</details>

**3. Dependency order.** Six morning tasks have prerequisites (wake before shower before dress before leave, wake before coffee before breakfast before leave). Output a valid order with a topological sort. What does your algorithm do if you add "leave before wake"?

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/graphs/ex3-topo.c}}

</details>

**4. Weighted shortest paths.** For the 5-vertex weighted directed graph in the answer program, compute the shortest distance from vertex 0 to every vertex with Dijkstra's algorithm. Why does Dijkstra fail on graphs with negative edge weights?

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/graphs/ex4-dijkstra.c}}

</details>

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

{{include dsa/practice/p6_grid.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p6_grid p6_grid.c && ./p6_grid; rm -f p6_grid}}

Scan every cell; each time you meet an unvisited land cell you have found a *new* island, so count it and flood-fill (DFS with an explicit stack, or BFS) to mark all connected land as visited. The program marks cells by overwriting `1` with `0`, which **destroys the input**; use a separate visited array if the grid is needed later. Each cell is pushed at most once (it is marked when pushed, not when popped, which prevents duplicates on the stack), so the cost is O(rows × columns). The explicit stack avoids deep recursion: a large all-land grid would otherwise overflow the call stack. The island sizes are 4, 5, 1, 6, 2 and 1.

**2.**

{{include dsa/practice/p6_maze.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p6_maze p6_maze.c && ./p6_maze; rm -f p6_maze}}

BFS explores in rings: every cell at distance *d* is dequeued before any cell at distance *d + 1*, so the first time a cell is reached it is by a shortest path, and `dist[][] >= 0` doubles as the visited marker. DFS dives down one path to the end and may reach `E` by a long detour first, so its first success is not minimal. BFS finds shortest paths only when every edge has the **same cost**; with different costs use Dijkstra. Time and space are O(cells).

**3.**

{{include dsa/practice/p6_topo.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p6_topo p6_topo.c && ./p6_topo; rm -f p6_topo}}

Kahn's algorithm keeps an *in-degree* (number of unmet prerequisites) per task. It repeatedly takes any task with in-degree 0, outputs it, and decrements its successors' counts. If the output ends up with fewer tasks than the graph has, the remaining ones wait on each other: a **cycle**, so no valid order exists (the second run reports it). A topological order exists exactly when the graph is a DAG (directed acyclic), and it is usually not unique: this run put `coffee` before `dress`, but `dress` first is equally valid. Time is O(V + E).

**4.**

{{include dsa/practice/p6_dijkstra.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p6_dijkstra p6_dijkstra.c && ./p6_dijkstra; rm -f p6_dijkstra}}

Dijkstra repeatedly finalises the unfinished vertex with the smallest known distance and *relaxes* its outgoing edges. The direct edge 0→1 costs 4 but going 0→2→1 costs 1 + 2 = 3, so the distances are 0, 3, 1, 4 and 7. This version scans all vertices to find the minimum, so it is O(V²); with a binary heap (see the heap exercise) it is O((V + E) log V). The correctness argument needs **non-negative weights**: once a vertex is finalised, no later path can be shorter, because every extra edge only adds cost. A negative edge could make a longer-looking path cheaper after the vertex was already finalised, so use **Bellman-Ford** (O(VE), also detects negative cycles) instead.

</details>

## Key takeaways

1. Recognise the model first: grids and task lists are graphs; flood fill finds connected components in O(cells).
2. BFS gives shortest paths when all edges cost the same; Dijkstra when costs are non-negative; Bellman-Ford when negative edges exist.
3. Kahn's topological sort orders a DAG in O(V + E) and detects cycles by emitting fewer than V nodes.
4. Mark nodes as visited when you *enqueue or push* them, not when you remove them, to avoid duplicate work.
