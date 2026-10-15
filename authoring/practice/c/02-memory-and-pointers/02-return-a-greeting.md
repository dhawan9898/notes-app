---
kind: code
title: Return a greeting safely
module: Memory and Pointers
difficulty: Medium
tags:
  - strings
  - dangling pointer
  - malloc
starter: c/starters/pointers/ex2-greeting.c
examples:
  - input: make_greeting_buf("Ada", buf, 32)
    output: buf = "Hello, Ada"
  - input: make_greeting_buf("Grace", buf, 8)
    output: buf = "Hello, "
    why: Seven characters plus the terminating NUL fill the 8-byte buffer.
  - input: make_greeting_heap("Linus")
    output: '"Hello, Linus" (heap)'
constraints:
  - The buffer version must never write past `cap` bytes and must always NUL-terminate when `cap > 0`.
  - The heap version must allocate exactly enough for any name length.
hints:
  - '`snprintf(out, cap, "Hello, %s", name)` truncates and terminates for you.'
  - For the heap version, `snprintf(NULL, 0, ...)` returns the length you need.
---

A function that builds `"Hello, <name>"` in a local array and returns its address returns a **dangling pointer**. Implement the two correct replacements:

1. `make_greeting_buf(name, out, cap)` writes into a buffer the caller supplies and truncates if it does not fit.
2. `make_greeting_heap(name)` returns a newly allocated string that the caller frees, or `NULL` if memory runs out.

## Explanation

The compiler warns "function returns address of local variable". The array lives in the function's stack frame, which is gone after the return; using the pointer is undefined behaviour. In practice GCC even replaces the returned address with `NULL`, so the program below crashes in `strlen`:

Fix 1 (`make_greeting_buf`): the caller owns the buffer and passes its size. Fix 2 (`make_greeting_heap`): allocate with `malloc`, document that the caller must `free`, and handle `NULL`. A third option is a `static` buffer, which is not re-entrant or thread-safe and is usually a poor choice.
