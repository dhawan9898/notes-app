---
kind: question
title: Ranges
module: Toolchain and Language Foundations
difficulty: Medium
---

Without running anything, write the value of `INT_MAX`, `INT_MIN` and `UINT_MAX` for a 32-bit `int`. Then print them and `sizeof` of `char`, `short`, `int`, `long`, `long long` and a pointer on your machine.

## Explanation

`INT_MAX = 2147483647`, `INT_MIN = -2147483648`, `UINT_MAX = 4294967295` (print them with `%zu` and `sizeof`: 1, 2, 4, 8, 8 and 8 on LP64 Linux).
