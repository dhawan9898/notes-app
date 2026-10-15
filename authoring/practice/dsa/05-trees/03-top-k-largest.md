---
kind: code
title: Top k largest values
module: Trees
difficulty: Medium
tags:
  - heap
  - priority queue
starter: dsa/starters/trees/ex3-top-k.c
examples:
  - input: data = {42, 7, 99, 15, 3, 88, 61, 27, 54, 70, 12, 95}, k = 4
    output: '{70, 88, 95, 99}'
  - input: '{5, 9, 9, 1, 9}, k = 3'
    output: '{9, 9, 9}'
    why: Duplicates count separately.
  - input: '{1, 2, 3, 4, 5, 6}, k = 6'
    output: '{1, 2, 3, 4, 5, 6}'
constraints:
  - '`1 <= k <= n <= 12` in the tests.'
  - O(n log k) time.
hints:
  - 'The root of a min-heap is the smallest of the k kept values: replace it when a bigger value arrives.'
  - At the end, repeatedly remove the root to get ascending order.
---

Write the `k` largest values of `data[0..n)` into `out[0..k)`, smallest first, using **O(k) extra memory**: keep a min-heap of size `k` instead of sorting everything. `k <= n`.

## Explanation

You need a **min-heap of size k**, which seems backwards but is the point: its root is the *smallest of the current top k*, i.e. the item most likely to be displaced. A new value only matters if it beats that root; then the root is popped and the new value pushed. Each of the *n* items costs at most one pop and one push, O(log k), so the total is **O(n log k)** time and **O(k)** space, and it works on a stream you cannot store. Sorting everything would take O(n log n) time and O(n) memory. (For one-off selection from an array in memory, quickselect is O(n) on average.)
