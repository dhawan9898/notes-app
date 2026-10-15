---
kind: code
title: Count inversions
module: Searching and Sorting
difficulty: Hard
tags:
  - merge sort
  - divide and conquer
starter: dsa/starters/sorting/ex4-inversions.c
examples:
  - input: '{8, 4, 2, 1, 7, 3, 9, 5}'
    output: '13'
  - input: '{2, 2, 1}'
    output: '2'
    why: 'Equal keys are not inversions: (2, 1) twice.'
  - input: 2000 values in descending order
    output: '1999000'
constraints:
  - '`n <= 2000` in the tests; the count needs a `long long`.'
  - A quadratic double loop is too slow for large inputs.
hints:
  - >-
    While merging, every time you take an element from the right half, all elements still left in the left half are
    inversions with it.
  - Take from the left when `left <= right` so equal keys do not count.
---

An inversion is a pair `i < j` with `a[i] > a[j]`. Count the inversions of an array in **O(n log n)** by extending merge sort. `tmp` is scratch space for `n` ints and you may reorder `a`.

## Explanation

During the merge, when the next item comes from the **right** half, it is smaller than *every* element still waiting in the left half (they are sorted and all larger), so it forms an inversion with each of them: add `mid - i` at once instead of counting one by one. Inversions inside the halves were counted recursively, and the merge counts the pairs that cross the halves, so every pair is counted exactly once. The total of 13 matches the brute-force double loop, which is the right way to test such code (compare a clever algorithm against an obviously correct slow one on small inputs). The cost is O(n log n). Inversions measure **how unsorted** an array is: 0 for sorted input, n(n−1)/2 for reversed input, and insertion sort's running time is exactly n plus the number of inversions, which is why it excels on nearly sorted data.
