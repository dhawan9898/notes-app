---
kind: code
title: Overflow-safe addition
module: Toolchain and Language Foundations
difficulty: Medium
tags:
  - undefined behaviour
  - integers
starter: c/starters/types/ex3-safe-add.c
examples:
  - input: a = 2000000000, b = -5
    output: true, *out = 1999999995
  - input: a = 2000000000, b = 2000000000
    output: 'false'
    why: The true sum 4000000000 is above INT_MAX.
  - input: a = INT_MIN, b = -1
    output: 'false'
constraints:
  - Any `int` values for `a` and `b`, including `INT_MIN` and `INT_MAX`.
  - 'Must not rely on undefined behaviour: the overflow check has to happen before the addition.'
hints:
  - Rewrite `a + b > INT_MAX` as `a > INT_MAX - b`, which cannot overflow when `b > 0`.
  - Treat positive and negative `b` separately; use `INT_MIN - b` for the lower bound.
  - >-
    Alternatively add in `long long` and compare with the limits (an `int` is narrower than a `long long` on common
    platforms).
---

Signed integer overflow is undefined behaviour in C, so you cannot add first and look at the result afterwards. Write `bool safe_add(int a, int b, int *out)` that stores `a + b` in `*out` and returns `true` when the true sum fits in an `int`; otherwise it returns `false` and leaves `*out` untouched.

Never perform an addition that overflows.

## Explanation

`safe_add` in the program above tests *before* adding: if `b > 0` the sum overflows exactly when `a > INT_MAX - b`; if `b < 0` it underflows when `a < INT_MIN - b`. The subtractions on the right cannot overflow because `b` has the opposite sign. GCC and Clang also provide `__builtin_add_overflow(a, b, out)`, and C23 adds `ckd_add` in `<stdckdint.h>`.
