---
kind: code
title: Swap two integers
module: Getting Started
difficulty: Easy
tags:
  - pointers
  - pass by value
starter: c/starters/start-here/ex3-swap.c
examples:
  - input: |-
      a = 1, b = 2
      swap_ints(&a, &b);
    output: a = 2, b = 1
  - input: |-
      x = -7, y = 7
      swap_ints(&x, &y);
    output: x = 7, y = -7
constraints:
  - Both pointers are valid and point to two different `int` variables.
  - Use a temporary variable; no helper functions or globals.
hints:
  - Assigning to the parameter `a` changes only the local copy of the pointer.
  - 'To change the variable the pointer refers to, write through it: `*a = ...`.'
---

C passes every argument **by value**, so a function cannot change a caller’s variable through a copy. `swap_ints` receives the **addresses** of two `int` variables and must exchange the values stored there.

The code in the editor makes the classic mistake: it swaps its own copies of the two pointers and leaves the caller’s ints alone. Fix it.

## Explanation

The function receives *copies* of the two numbers; it swaps its copies and the caller's variables are untouched. Pass addresses: `void swap_ints(int *a, int *b) { int t = *a; *a = *b; *b = t; }` and call `swap_ints(&x, &y)`.
