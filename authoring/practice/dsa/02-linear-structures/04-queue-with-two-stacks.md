---
kind: code
title: Queue from two stacks
module: Linear Structures
difficulty: Medium
tags:
  - queue
  - stack
  - amortised analysis
starter: dsa/starters/lists/ex4-queue-two-stacks.c
examples:
  - input: enqueue 10, 20, 30, 40; dequeue twice
    output: 10, 20
  - input: then enqueue 50, 60; dequeue four times
    output: 30, 40, 50, 60
constraints:
  - Each item is pushed at most twice (once per stack).
  - At most 64 items are stored at once.
hints:
  - Enqueue always pushes onto `in`.
  - Dequeue pops from `out`; only when `out` is empty, move **all** of `in` onto it.
---

Build a FIFO queue out of two LIFO stacks, `in` and `out`, using only `spush` and `spop`. Each operation must be **O(1) amortised**: items must not be shuffled back and forth on every call. `dequeue` returns -1 when the queue is empty.

## Explanation

Enqueue pushes onto the `in` stack. Dequeue pops from `out`; only when `out` is empty does it pour *all* of `in` into `out`, which reverses the order so the oldest item ends up on top. The crucial rule is to refill **only when `out` is empty**; refilling earlier would put new items ahead of old ones. Each item is pushed to `in` once, moved to `out` once and popped once (the counter shows 6 moves for 6 items), so any sequence of *n* operations costs O(n) in total: amortised O(1) each, though a single `dequeue` may take O(n).
