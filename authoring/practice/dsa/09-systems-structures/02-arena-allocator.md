---
kind: code
title: Arena allocator
module: Systems-Flavored Structures
difficulty: Medium
tags:
  - allocator
  - alignment
starter: dsa/starters/systems/ex2-arena.c
examples:
  - input: 100 allocations of a 16-byte node
    output: used = 100 * sizeof(Node) (no padding between equal-sized, aligned objects)
  - input: arena_alloc(a, 1, 1); arena_alloc(a, 4, 4)
    output: the int starts 4 bytes after the char
  - input: arena_alloc(a, 5000, 1) on a 4096-byte arena
    output: 'NULL'
constraints:
  - '`align` is a power of two; align the **address**, not just the offset.'
  - Never read or write outside `cap` bytes; guard against overflow in `used + size`.
hints:
  - '`aligned = (addr + align - 1) & ~(align - 1)` on the address `base + used`.'
  - Convert via `uintptr_t`, then compute the new `used` as an offset from `base`.
  - Reset is just `used = 0`.
---

An `Arena` has a block of memory `base`, its size `cap` and the bytes handed out so far, `used`. Implement `arena_alloc(a, size, align)`, which returns `size` bytes aligned to `align` (a power of two) or `NULL` if they do not fit, and `arena_reset`, which releases everything at once.

## Explanation

Allocation is **pointer bumping**: round the current offset up to the requested alignment with `(used + (align - 1)) & ~(align - 1)` (valid for powers of two), check that the object fits, advance `used`, and return `base + offset`. That is a handful of instructions with no search and no per-object header, so allocation is O(1) and objects allocated together sit next to each other in memory (excellent cache behaviour). The 100 nodes use 1,600 bytes (16 per node, as `sizeof(Node)` shows) with no `malloc` calls, and `arena_reset` releases all of them in O(1); there is no per-object `free`, so there is nothing to forget. After allocating a single `char`, the next `int` lands 4 bytes later and is correctly aligned (the padding is visible in the offset). Use arenas for data with a **common lifetime**: a request in a server, one compiler pass, a parser's syntax tree, a game frame. Do not use them for objects that need to be freed individually at different times, and never keep a pointer into an arena after reset. The allocator itself must also be sized (or chained) to avoid returning `NULL` unexpectedly.
