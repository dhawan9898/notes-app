---
kind: code
title: Largest element
module: Getting Started
difficulty: Easy
tags:
  - arrays
  - loops
starter: c/starters/start-here/ex4-max.c
examples:
  - input: v = {4, 9, -2, 7}
    output: '9'
  - input: v = {-5, -1, -7}
    output: '-1'
    why: 'All elements are negative: starting the running maximum at 0 would give the wrong answer.'
constraints:
  - '`count >= 1`.'
  - Elements can be any `int`, including negative values.
  - The largest element may be the last one, or the only one.
hints:
  - Start the running maximum with `v[0]`, not with 0.
  - Loop from index 1 and replace the maximum when `v[i]` is larger.
---

Write `int max_of(const int *v, int count)` that returns the largest element of the array `v`.

## Explanation

Start with the first element as the best, scan the rest and replace the best when you see a larger one.
