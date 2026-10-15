---
kind: code
title: Fix the memory leak
module: Memory and Pointers
difficulty: Easy
tags:
  - malloc
  - free
  - ownership
starter: c/starters/pointers/ex3-leak.c
examples:
  - input: Student *s = student_new("Ada", 3);
    output: 3 blocks are allocated
  - input: student_free(s);
    output: 0 blocks are still allocated
constraints:
  - Free each owned block exactly once, then the struct itself, which must be freed last.
  - 'No use after free: do not read `s` after releasing it.'
hints:
  - Count the `malloc`/`calloc` calls in `student_new`.
  - Release the members first (`name`, `scores`), then the struct they live in.
---

`student_new` allocates a `Student` (a struct holding a name, an array of scores and a count) and two more blocks owned by it. `student_free` only releases one of them, so memory leaks.

Rewrite `student_free` so that **every** block `student_new` allocated is released. The tests count live allocations.

## Explanation

The buggy version (frees only the name):

Two allocations are lost: the `Student` struct itself (24 bytes, the *direct* leak) and the `scores` array it points to (12 bytes, an *indirect* leak, because the only pointer to it was inside the leaked struct). The corrected program frees all three, **children before the parent**:

A clean run with exit status 0 means LeakSanitizer found nothing.
