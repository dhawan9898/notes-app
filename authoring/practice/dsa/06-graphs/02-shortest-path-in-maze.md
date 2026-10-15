---
kind: code
title: Shortest path in a maze
module: Graphs
difficulty: Medium
tags:
  - graph
  - BFS
  - grid
starter: dsa/starters/graphs/ex2-maze.c
examples:
  - input: |-
      S.#......
      .##.####.
      ....#....
      .####.##.
      ......#.E
    output: '16'
  - input: SE
    output: '1'
  - input: S#E
    output: '-1'
constraints:
  - Exactly one `S` and one `E`.
  - Grids are small (at most a few hundred cells).
hints:
  - Breadth-first search explores cells in order of distance, so the first time you reach `E` is the shortest path.
  - Keep a distance (or visited) array and a queue of cells.
---

`S` is the start, `E` the exit, `#` a wall and `.` a free cell. Moves go up, down, left or right at cost 1. Return the number of steps of the shortest path from `S` to `E`, or -1 if `E` cannot be reached.

## Explanation

BFS explores in rings: every cell at distance *d* is dequeued before any cell at distance *d + 1*, so the first time a cell is reached it is by a shortest path, and `dist[][] >= 0` doubles as the visited marker. DFS dives down one path to the end and may reach `E` by a long detour first, so its first success is not minimal. BFS finds shortest paths only when every edge has the **same cost**; with different costs use Dijkstra. Time and space are O(cells).
