---
kind: code
title: Remove duplicates in place
module: Getting Started
difficulty: Easy
tags:
  - two pointers
  - in place
starter: dsa/starters/start-here/ex6-dedupe.c
examples:
  - input: a = {1, 1, 2, 2, 2, 3, 5, 5}
    output: returns 4, a starts with {1, 2, 3, 5}
  - input: a = {7, 7, 7}
    output: returns 1, a starts with {7}
  - input: n = 0
    output: returns 0
constraints:
  - The array is sorted; `n >= 0`.
  - O(n) time, O(1) extra memory.
hints:
  - 'A write index trails a read index: copy `a[r]` only when it differs from the last value written.'
  - Handle `n == 0` before touching `a[0]`.
---

Remove duplicates from the sorted array `a` **in place** and return the new length. The first `new length` elements must hold each distinct value once, in order.

## Explanation

**Cost.** O(n), O(1).
