---
kind: code
title: Growable array of ints
module: Getting Started
difficulty: Medium
tags:
  - malloc
  - realloc
  - dynamic array
starter: c/starters/start-here/ex7-vec.c
examples:
  - input: |-
      IntVec v = {0};
      for (i = 0; i < 10; i++) vec_push(&v, i * i);
    output: v.size == 10, v.data[9] == 81
  - input: vec_free(&v);
    output: v.data == NULL, v.size == 0
constraints:
  - '`vec_push` returns 1 on success and 0 if memory runs out; on failure the vector must stay valid.'
  - Start from `cap == 0`; grow by doubling (first allocation of a small fixed size is fine).
  - Never lose the old pointer if `realloc` fails.
hints:
  - Assign the result of `realloc` to a temporary, check it for NULL, and only then store it in `v->data`.
  - 'The new capacity is `v->cap ? v->cap * 2 : 4`.'
  - After `free(v->data)` set `data = NULL` and `size = cap = 0`.
---

An `IntVec` has three fields: `data` (the elements), `size` (elements in use) and `cap` (elements allocated). A zero-initialised vector is a valid empty vector.

Implement `vec_push`, which appends one value and **doubles the capacity** with `realloc` when the vector is full, and `vec_free`, which releases the memory and leaves the vector empty.

## Explanation

Keep `data`, `size` and `cap`; when `size == cap`, `realloc` to double (starting from 4) and **check the result for `NULL` before overwriting your pointer**; doubling makes the average cost of a push constant (see *Dynamic Arrays* in the DSA track).
