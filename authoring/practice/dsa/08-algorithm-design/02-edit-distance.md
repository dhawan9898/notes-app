---
kind: code
title: Edit distance
module: Algorithm Design
difficulty: Hard
tags:
  - dynamic programming
  - strings
starter: dsa/starters/design/ex2-edit-distance.c
examples:
  - input: '"kitten", "sitting"'
    output: '3'
  - input: '"intention", "execution"'
    output: '5'
  - input: '"", "abc"'
    output: '3'
constraints:
  - Either string may be empty.
  - O(|a| * |b|) time; a table of `(|a| + 1) x (|b| + 1)` entries (or two rows) is enough.
hints:
  - '`d[i][j]` is the distance between the first i characters of a and the first j of b.'
  - If `a[i-1] == b[j-1]` take `d[i-1][j-1]`; otherwise 1 + the minimum of the three neighbours.
  - The first row and column are `0..|b|` and `0..|a|`.
---

The edit distance between two strings is the minimum number of single-character **insertions, deletions and substitutions** that turn one into the other. Implement `edit_distance(a, b)`.

## Explanation

`d[i][j]` is the edit distance between the first `i` characters of `a` and the first `j` of `b`. The base row and column are the cost of deleting or inserting everything. Each cell takes the cheapest of three moves: delete `a[i-1]` (from `d[i-1][j]`), insert `b[j-1]` (from `d[i][j-1]`), or substitute (from `d[i-1][j-1]`, free when the characters match). `kitten` to `sitting` is 3 (substitute k→s, substitute e→i, insert g) and `intention` to `execution` is 5. The table has O(n·m) cells, each O(1); keeping only two rows reduces memory to O(m) (but then you cannot recover the edit script).
