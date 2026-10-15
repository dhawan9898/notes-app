---
kind: code
title: Dijkstra’s shortest distances
module: Graphs
difficulty: Hard
tags:
  - graph
  - Dijkstra
  - shortest path
starter: dsa/starters/graphs/ex4-dijkstra.c
examples:
  - input: edges 0->1 (4), 0->2 (1), 2->1 (2), 1->3 (1), 2->3 (5), 3->4 (3); src = 0
    output: dist = {0, 3, 1, 4, 7}
  - input: same graph; src = 3
    output: dist[4] = 3, dist[0] = INF
constraints:
  - '`N` vertices, weights `>= 0`.'
  - 'Careful: `INF + weight` must not overflow or look like a real distance.'
hints:
  - 'Repeat N times: pick the unvisited vertex with the smallest distance and relax its outgoing edges.'
  - Skip edges equal to `INF` and stop early if the best remaining vertex is `INF`.
---

`w[u][v]` is the weight of the edge `u -> v` (non-negative) or `INF` if there is none. Fill `dist[v]` with the length of the shortest path from `src` to `v`, or `INF` if `v` is unreachable. The code in the editor only looks at direct edges.

## Explanation

Dijkstra repeatedly finalises the unfinished vertex with the smallest known distance and *relaxes* its outgoing edges. The direct edge 0→1 costs 4 but going 0→2→1 costs 1 + 2 = 3, so the distances are 0, 3, 1, 4 and 7. This version scans all vertices to find the minimum, so it is O(V²); with a binary heap (see the heap exercise) it is O((V + E) log V). The correctness argument needs **non-negative weights**: once a vertex is finalised, no later path can be shorter, because every extra edge only adds cost. A negative edge could make a longer-looking path cheaper after the vertex was already finalised, so use **Bellman-Ford** (O(VE), also detects negative cycles) instead.
