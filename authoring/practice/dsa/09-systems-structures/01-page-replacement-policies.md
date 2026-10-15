---
kind: code
title: 'Page replacement: FIFO, LRU, OPT'
module: Systems-Flavored Structures
difficulty: Hard
tags:
  - caching
  - simulation
  - operating systems
starter: dsa/starters/systems/ex1-page-faults.c
examples:
  - input: refs = 1 2 3 4 1 2 5 1 2 3 4 5, frames = 3
    output: FIFO 9, LRU 10, OPT 7
  - input: same refs, frames = 4
    output: FIFO 10, LRU 8, OPT 6
    why: FIFO gets worse with more frames (Belady’s anomaly).
constraints:
  - Every first access to a page is a miss.
  - '`n` up to a few dozen references.'
hints:
  - Keep the frames in an array and a timestamp (load time for FIFO, last use for LRU).
  - For OPT, scan forward from the current position to find each cached page’s next use.
---

Simulate a cache of `frames` pages (at most 16, initially empty) on a sequence of page references (page numbers 0 to 99) and count the **misses** for three policies: FIFO (evict the oldest loaded page), LRU (evict the least recently used) and OPT (evict the page used furthest in the future, or never).

## Explanation

Two surprises. First, **FIFO with 4 frames faults more often (10) than with 3 frames (9)**: this is **Belady's anomaly**. More memory should never hurt, but FIFO ignores recency and the extra frame changes *which* page gets evicted at a bad moment. LRU and OPT never show it, because they are **stack algorithms**: the set of pages cached with *n* frames is always a subset of the set with *n+1* frames. Second, with 3 frames LRU (10 misses) is worse than FIFO (9) on this particular string: no policy wins on every access pattern, and this one is crafted to cycle through more pages than the cache holds, the worst case for LRU. **OPT** gives the lower bound (7, 6 and 5 misses): it needs to know the *future* references, so it cannot be implemented online, but it is the benchmark against which real policies are measured; with 5 frames the whole working set fits and all three policies tie at 5 compulsory misses. Real systems approximate LRU cheaply (CLOCK, two-queue, ARC) because exact LRU needs bookkeeping on every hit.
