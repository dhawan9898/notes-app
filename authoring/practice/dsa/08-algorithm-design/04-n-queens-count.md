---
kind: code
title: Count N-Queens placements
module: Algorithm Design
difficulty: Hard
tags:
  - backtracking
  - recursion
starter: dsa/starters/design/ex4-queens.c
examples:
  - input: n = 4
    output: '2'
  - input: n = 8
    output: '92'
  - input: n = 2
    output: '0'
constraints:
  - '`1 <= n <= 10` in the tests (724 solutions for n = 10).'
  - Prune as soon as a placement is illegal.
hints:
  - Place one queen per row; track used columns and both diagonals (`r + c` and `r - c + n`).
  - Undo the placement after the recursive call (backtrack).
---

Count the ways to place `n` queens on an `n x n` board so that no two attack each other (same row, column or diagonal).

## Explanation

The counts for n = 4 to 8 are 2, 10, 4, 40 and 92. The search places one queen per row; before recursing, it skips any column or diagonal already attacked. That **pruning** abandons a whole subtree of placements as soon as one conflict appears: for n = 8 there are about 17 million ways to put one queen in each row (8^8), but far fewer partial placements survive the checks. The three boolean arrays make the test O(1): a queen at (row, col) occupies column `col`, diagonal `row + col` and anti-diagonal `row - col`. After exploring a choice you must **undo** it (the last line of the loop), otherwise the marks of an abandoned branch would block later, unrelated branches: this choose-explore-unchoose rhythm is the signature of backtracking.
