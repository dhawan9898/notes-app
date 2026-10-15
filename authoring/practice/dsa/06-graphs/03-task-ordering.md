---
kind: code
title: Order tasks with prerequisites
module: Graphs
difficulty: Medium
tags:
  - graph
  - topological sort
  - cycle
starter: dsa/starters/graphs/ex3-topo.c
examples:
  - input: wake -> shower -> dress -> leave; wake -> coffee -> breakfast -> leave
    output: 1, e.g. wake coffee breakfast shower dress leave
  - input: the same plus leave -> wake
    output: 0 (cycle)
constraints:
  - Any order that respects every edge is accepted.
  - Do not modify the caller’s `edge` matrix.
hints:
  - 'Kahn’s algorithm: repeatedly take a task with no remaining prerequisites.'
  - If fewer than N tasks were taken, there is a cycle.
---

`edge[a][b] == 1` means task `a` must happen before task `b`. Fill `order[0..N)` with a valid order and return 1, or return 0 if the prerequisites contain a **cycle** and no order exists. There are `N = 6` tasks.

## Explanation

Kahn's algorithm keeps an *in-degree* (number of unmet prerequisites) per task. It repeatedly takes any task with in-degree 0, outputs it, and decrements its successors' counts. If the output ends up with fewer tasks than the graph has, the remaining ones wait on each other: a **cycle**, so no valid order exists (the second run reports it). A topological order exists exactly when the graph is a DAG (directed acyclic), and it is usually not unique: this run put `coffee` before `dress`, but `dress` first is equally valid. Time is O(V + E).
