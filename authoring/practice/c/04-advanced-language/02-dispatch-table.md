---
kind: code
title: Dispatch table of function pointers
module: Advanced Language
difficulty: Easy
tags:
  - function pointers
  - tables
starter: c/starters/callbacks/ex2-dispatch.c
examples:
  - input: table["add"].op(6, 3)
    output: '9'
  - input: table["sub"].op(6, 3)
    output: '3'
  - input: table["mul"].op(6, 3)
    output: '18'
constraints:
  - The three entries stay in the order add, sub, mul.
  - 'Entries hold the function name without parentheses: `{"add", add}`.'
hints:
  - A function name by itself decays to a pointer to that function.
  - Replace each `NULL` in the table with the matching function.
---

A `Command` pairs a name with a function pointer `int (*op)(int, int)`. Implement `add`, `sub` and `mul` and fill the table so that each entry points at its function. The tests find a command by name and call it through the pointer: no `switch`.

## Explanation

The table pairs a name with a function pointer; `table[i].op(6, 3)` calls through it. Adding a command means adding one table row, not editing a `switch`. The type `int (*op)(int, int)` reads "pointer to a function taking two ints and returning int".
