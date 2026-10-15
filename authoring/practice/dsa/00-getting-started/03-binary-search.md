---
kind: code
title: Binary search
module: Getting Started
difficulty: Easy
tags:
  - binary search
  - arrays
starter: dsa/starters/start-here/ex3-binary-search.c
examples:
  - input: a = {1, 3, 4, 7, 9, 12}, key = 9
    output: '4'
  - input: a = {1, 3, 4, 7, 9, 12}, key = 8
    output: '-1'
  - input: a = {5}, n = 0, key = 5
    output: '-1'
    why: The empty array contains nothing.
constraints:
  - '`a` is sorted ascending, distinct values.'
  - O(log n) time. Compute the middle as `lo + (hi - lo) / 2` to avoid overflow.
hints:
  - Keep a half-open range `[lo, hi)` and shrink it until it is empty.
  - The first and the last element are the classic places where the loop condition goes wrong.
---

Return the index of `key` in the sorted array `a`, or -1 if it is not there.

## Explanation

**Cost.** O(log n), O(1).

**Key idea.** `mid = lo + (hi - lo) / 2` avoids overflow of `lo + hi`.
