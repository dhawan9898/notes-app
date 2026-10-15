---
kind: code
title: Two sum in a sorted array
module: Getting Started
difficulty: Easy
tags:
  - two pointers
  - arrays
starter: dsa/starters/start-here/ex1-two-sum.c
examples:
  - input: a = {1, 3, 4, 7, 9, 12}, target = 16
    output: 1, for example i = 2, j = 5
    why: 4 + 12 = 16. Any valid pair is accepted, such as 7 + 9.
  - input: a = {1, 3, 4, 7, 9, 12}, target = 2
    output: '0'
    why: An element cannot pair with itself.
constraints:
  - '`0 <= n`; the array is sorted ascending and may contain duplicates.'
  - Target O(n) time and O(1) extra memory.
hints:
  - Keep two indices, one at each end.
  - If the sum is too small move the left index right; if too big move the right index left.
  - 'Stop when the indices meet: `i < j` guarantees different elements.'
---

Given an array `a` sorted in ascending order and a `target`, find two elements at **different** indexes that add up to `target`. Store their indexes (`i < j`) in `*i_out` and `*j_out` and return 1, or return 0 if there is no such pair.

Do better than checking every pair.

## Explanation

**Cost.** O(n) time, O(1) memory.

**Key idea.** if the current sum is too small, only moving the left index right can increase it, so no pair is skipped wrongly.
