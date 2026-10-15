---
kind: code
title: The countdown that never ends
module: Toolchain and Language Foundations
difficulty: Easy
tags:
  - unsigned
  - loops
  - compiler warnings
starter: c/starters/types/ex5-countdown.c
examples:
  - input: n = 3
    output: returns 3, out = {2, 1, 0}
  - input: n = 0
    output: returns 0
    why: For an unsigned n, `n - 1` wraps around to a huge number.
constraints:
  - '`n` is a `size_t` (unsigned); `out` has room for `n` entries.'
  - The result must be correct for `n == 0` as well.
hints:
  - '`i >= 0` is always true for an unsigned `i`: after 0 it wraps to the maximum value.'
  - Loop with `for (size_t i = n; i-- > 0; )` or test before decrementing.
---

`countdown(n, out)` should record every index from `n - 1` down to `0` in `out[]` and return how many were visited. The loop in the editor never ends on its own (a safety cap stops it after 5 steps) and the compiler warns about the comparison. Find out why and rewrite it.

## Explanation

`size_t` is unsigned, so `i >= 0` is true for every value; decrementing 0 wraps to a huge number instead of going negative, and the loop only stops because of the extra counter. `-Wextra` reports "comparison of unsigned expression in '>= 0' is always true". The idiom that works for any unsigned counter tests before it decrements:

`for (size_t i = n; i-- > 0; )` reads "while `i` is positive, decrement it and use the new value", so the body sees `n-1` down to `0`, and `n == 0` runs zero times.
