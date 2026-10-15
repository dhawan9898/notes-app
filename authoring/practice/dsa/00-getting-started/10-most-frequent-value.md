---
kind: code
title: Most frequent value
module: Getting Started
difficulty: Easy
tags:
  - counting
  - arrays
starter: dsa/starters/start-here/ex10-mode.c
examples:
  - input: '{4, 7, 4, 2, 7, 7, 9}'
    output: '7'
  - input: '{3, 1, 3, 1}'
    output: '1'
    why: 'Tie between 1 and 3: the smaller wins.'
  - input: '{0, 0, 5}'
    output: '0'
constraints:
  - '`1 <= n`; every value is in `0..99`.'
  - O(n + 100) time.
hints:
  - A counting array of 100 `int`s, indexed by the value.
  - Scan the counters in increasing order and replace the best only on a strictly larger count.
---

Given `n > 0` values between 0 and 99, return the value that occurs most often. On a tie, return the **smallest** of the most frequent values.

## Explanation

**Cost.** O(n), O(1) (a 100-entry table).
