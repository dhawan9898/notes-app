---
kind: code
title: Fix the off-by-one sum
module: Getting Started
difficulty: Easy
tags:
  - debugging
  - loops
  - arrays
starter: c/starters/start-here/ex8-sum.c
examples:
  - input: v = {1, 2, 3, 4, 99}, n = 4
    output: '10'
    why: Only v[0] .. v[3] count; the 99 must not be added.
  - input: v = {1, 2, 3, 4, 99}, n = 0
    output: '0'
constraints:
  - '`0 <= n <= length of v`.'
  - 'Never read `v[n]`: on a full-length array that is out of bounds.'
hints:
  - n elements means indexes 0 to n - 1.
  - 'Look at the loop condition: `<=` versus `<`.'
---

`sum_first_n(v, n)` should return the sum of the **first `n` elements** of `v`. It has an off-by-one bug that reads one element too many. Find and fix it.

## Explanation

The loop condition `i <= n` runs `n + 1` times and reads one element past the end (`v[n]`): an **off-by-one** error. With `i < n` it is correct. AddressSanitizer reports `stack-buffer-overflow ... READ of size 4` and points at the line.
