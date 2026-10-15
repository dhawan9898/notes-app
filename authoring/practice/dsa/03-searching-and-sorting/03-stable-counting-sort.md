---
kind: code
title: Stable counting sort
module: Searching and Sorting
difficulty: Medium
tags:
  - counting sort
  - stability
starter: dsa/starters/sorting/ex3-counting-sort.c
examples:
  - input: 7a 3b 10c 3d 0e 7f 7g 1h 10i 3j
    output: 0e 1h 3b 3d 3j 7a 7f 7g 10c 10i
constraints:
  - '`0 <= age <= 10`.'
  - O(n + k) time with k = 11.
hints:
  - Count each age, then turn the counts into starting positions (a running sum).
  - Walk the input from the first element to the last, placing each person at the next free position of its age.
---

Sort `in[0..n)` by `age` (0 to 10) into `out[0..n)` without comparing elements. People with equal ages must keep their input order (the sort is **stable**).

## Explanation

Step 1 builds a **histogram** (how many of each value); step 2 turns it into **prefix sums**, so `count[v]` becomes the number of items less than or equal to `v`, which is the position just after the last `v` in the output; step 3 places each item at `--count[value]`. Scanning the input **backwards** puts the *last* of equal items in the *last* slot, so equal keys keep their original order: the sort is stable, which is what makes counting sort usable as the building block of radix sort. The cost is O(n + k) time and space, with k the number of possible values: linear, and it beats the O(n log n) comparison-sort lower bound because it **never compares** items, it uses the key as an array index. That only works when k is small (a range of ten million values needs a ten-million-entry table).
