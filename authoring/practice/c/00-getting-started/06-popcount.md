---
kind: code
title: Count the 1 bits
module: Getting Started
difficulty: Easy
tags:
  - bits
starter: c/starters/start-here/ex6-popcount.c
examples:
  - input: x = 255
    output: '8'
  - input: x = 0x80000001
    output: '2'
    why: The highest and the lowest bit are set.
  - input: x = 0xFFFFFFFF
    output: '32'
constraints:
  - '`x` is an unsigned 32-bit value: use unsigned shifts only.'
  - Result is between 0 and 32.
hints:
  - '`x & 1` is the lowest bit. Add it up, then shift `x` right by one until it is zero.'
  - 'Faster: `x &= x - 1` clears the lowest set bit, so the loop runs once per set bit.'
---

Write `int popcount(uint32_t x)` that returns the number of bits set to 1 in `x`.

## Explanation

Test the lowest bit with `x & 1`, add it to a counter, shift right, repeat until `x` is 0 (there are faster methods: see *Bit Manipulation*).
