---
kind: code
title: Number of islands
module: Graphs
difficulty: Medium
tags:
  - graph
  - DFS
  - BFS
  - grid
starter: dsa/starters/graphs/ex1-islands.c
examples:
  - input: |-
      11000111
      11000101
      00100000
      00011100
      10011100
      10000001
    output: 6 islands, largest 6
  - input: |-
      10
      01
    output: 2 islands
    why: Diagonal cells do not connect.
  - input: '000'
    output: 0 islands
constraints:
  - The grid has `rows` strings of `cols` characters, at most `MAXC` columns.
  - Avoid unbounded recursion on large islands if you can (an explicit stack works).
hints:
  - Scan every cell; when you find unvisited land start a flood fill and count its cells.
  - Mark visited land as `0` (or any other character) so it is not counted twice.
---

A grid contains `1` (land) and `0` (water). Count the islands, groups of land cells connected **up, down, left or right** (not diagonally), and the size of the largest. You may modify the grid, for example to mark visited cells.

## Explanation

Scan every cell; each time you meet an unvisited land cell you have found a *new* island, so count it and flood-fill (DFS with an explicit stack, or BFS) to mark all connected land as visited. The program marks cells by overwriting `1` with `0`, which **destroys the input**; use a separate visited array if the grid is needed later. Each cell is pushed at most once (it is marked when pushed, not when popped, which prevents duplicates on the stack), so the cost is O(rows × columns). The explicit stack avoids deep recursion: a large all-land grid would otherwise overflow the call stack. The island sizes are 4, 5, 1, 6, 2 and 1.
