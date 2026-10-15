---
kind: code
title: Merge two sorted arrays
module: Getting Started
difficulty: Easy
tags:
  - merge
  - arrays
starter: dsa/starters/start-here/ex5-merge.c
examples:
  - input: a = {1, 3, 4, 7, 9, 12}, b = {2, 3, 10}
    output: '{1, 2, 3, 3, 4, 7, 9, 10, 12}'
  - input: a = {20, 30}, b = {}
    output: '{20, 30}'
constraints:
  - Either array can be empty.
  - 'O(na + nb) time: do not sort the result afterwards.'
hints:
  - Keep one index per input and always copy the smaller current element.
  - When one input runs out, copy the rest of the other.
---

Merge the sorted arrays `a` (length `na`) and `b` (length `nb`) into `out`, which has room for `na + nb` elements, in a single pass.

## Explanation

**Cost.** O(n + m), O(n + m) for the output.
