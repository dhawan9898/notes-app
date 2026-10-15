---
kind: question
title: Find the bug
module: Systems-Level C
difficulty: Hard
---

`uint32_t rotl(uint32_t x, unsigned r) { return (x << r) | (x >> (32 - r)); }` works for `r = 1..31`. What breaks for `r = 0`, and what does UBSan say?

## Explanation

For `r = 0` the second shift is `x >> 32`, which is undefined for a 32-bit type: the CPU may use only the low 5 bits of the count and return `x` itself, producing `x | x` (accidentally correct on x86), or the compiler may assume it cannot happen. UBSan reports "shift exponent 32 is too large for 32-bit type". The safe form is `r &= 31; return r ? (x << r) | (x >> (32 - r)) : x;`, or `(x << r) | (x >> (-r & 31))`, which compilers recognise as a single rotate instruction.
