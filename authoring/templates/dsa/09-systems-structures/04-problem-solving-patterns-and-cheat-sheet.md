---
title: Problem-Solving Patterns and the Complexity Cheat Sheet
track: dsa
module: Systems-Flavored Structures
moduleOrder: 9
order: 4
level: advanced
summary: "Seven reusable patterns (two pointers, sliding window, monotonic stack, prefix sums with hashing, interval merging, binary search on the answer, top-k) verified against brute force, a decision guide for choosing a data structure, a complexity table for everything built in this track, and a latency reality check."
sources:
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed. (appendices on summations and the complexity of each structure)
  - label: Skiena — The Algorithm Design Manual, 3rd ed., Part I (Practical Algorithm Design) and the Hitchhiker's Guide to algorithms
  - label: Bentley — Programming Pearls, 2nd ed., Columns 1-2, 8 and 13
  - label: Sedgewick & Wayne — Algorithms, 4th ed. (summary tables of symbol-table and priority-queue implementations)
  - label: Drepper — What Every Programmer Should Know About Memory (2007)
    url: https://people.freebsd.org/~lstewart/articles/cpumemory.pdf
  - label: Dean — Numbers Everyone Should Know (Stanford talk, 2009)
---

This last note of the track is about *selection*: given a problem, which structure or technique do you reach for? It collects the recurring patterns that turn quadratic brute force into linear or `n log n` solutions, a decision guide, a table of the complexity of everything built in the track, and a measurement of the machine-level costs that those big-O symbols leave out.

## Seven patterns that cover most problems

Each pattern is implemented below and checked against a brute-force version on 3000 random instances.

### 1. Two pointers

On a **sorted** array, a pair of indices moving inward (or two cursors moving forward) replaces a double loop. *Pair with a given sum:* start at both ends; if the sum is too small move the left pointer right, if too big move the right pointer left. Each step discards one element, so it is `O(n)` instead of `O(n²)`. Variants: three-sum (fix one, two-pointer the rest, `O(n²)`), merging two sorted sequences, partitioning (Dutch national flag), removing duplicates in place, and fast/slow pointers for linked-list cycles (see the linked lists note). *Precondition: sorted order, or a monotone structure.*

### 2. Sliding window

When the answer is a contiguous subarray or substring, keep a window `[l, r]` and the state needed to evaluate it; extend `r`, and advance `l` while the window violates the condition. Each index enters and leaves once: `O(n)`. *Longest substring without repeated characters*, minimum window containing all required characters, maximum sum of `k` consecutive items, longest run with at most `k` zeros. *Precondition: the property is monotone in the window* (shrinking a valid window keeps it valid, or growing an invalid one keeps it invalid). With negative numbers and "sum ≥ target" this fails — use prefix sums.

### 3. Monotonic stack (and deque)

Keep a stack whose elements are monotone; each new element pops everything it dominates. Each element is pushed and popped once: `O(n)`. *Next greater element* (below), largest rectangle in a histogram, stock-span, trapping rain water; the monotonic **deque** gives sliding-window maximum in `O(n)` (see the queues note).

### 4. Prefix sums with a hash map

`prefix[j] − prefix[i] = k` means the subarray `(i, j]` sums to `k`; so for each `j`, count earlier prefixes equal to `prefix[j] − k`. A hash map of prefix counts answers *"how many subarrays sum to k"* in `O(n)` — and unlike two pointers it works with **negative numbers**. The same idea gives subarrays with equal numbers of 0s and 1s, divisible sums (map prefix mod m), and range queries (previous note).

### 5. Interval merging / sweep

Sort intervals by start; scan, extending the current interval while the next one overlaps. `O(n log n)`, dominated by the sort. The sweep-line generalisation (sort events, process in order) solves meeting rooms, skyline, and "maximum overlap" problems (see greedy algorithms).

### 6. Binary search on the answer

If you can test "is `x` feasible?" in `O(f)` and feasibility is **monotone** (once true, true for all larger `x`), binary search the smallest feasible `x`: `O(f · log range)`. *Minimum ship capacity to deliver packages in `D` days* (below), minimum rate to finish tasks before a deadline, largest minimum distance when placing items, square roots and thresholds in floating point. This is the "parametric search" of the binary search note.

### 7. Top-k with a bounded heap

Keep a min-heap of size `k`; each new item that beats the root replaces it: `O(n log k)` time, `O(k)` memory, and works on streams (heaps note). For one-off selection on in-memory data, quickselect is `O(n)` average.

{{include dsa/patterns/patterns.c}}

{{run dsa/patterns :: gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined patterns.c -o patterns && ./patterns}}

## Choosing a data structure

Ask these questions in order:

1. **What are the operations, and which dominate?** Lookup by key, ordered iteration, min/max, range queries, insertion at ends, membership only?
2. **Is the data static or dynamic?** Static data is almost always best in a *sorted array* or a *perfect hash*, built once.
3. **Do you need order?** If not, a hash table. If yes, a sorted array (static), B-tree/skip list/balanced tree (dynamic), or heap (only min/max).
4. **How big is it relative to the cache?** Below a few thousand elements, a plain array scanned linearly often beats everything; above the cache sizes, memory layout dominates.
5. **Is exactness required?** If not, probabilistic structures (Bloom, Count-Min, HyperLogLog) trade accuracy for orders of magnitude less memory.
6. **Does it need to be concurrent?** Shard first; use lock-free only if you need the progress guarantee.

| Problem | Reach for | Why |
|---|---|---|
| "Have I seen this?" exactly | hash set (open addressing) | O(1) expected |
| "Have I seen this?" in tiny memory | Bloom filter | ~10 bits per key |
| Count distinct items in a huge stream | HyperLogLog | 16 KB for ~1 % error |
| Frequent items in a stream | Count-Min + heap | sublinear memory |
| Ordered map with updates | B+ tree, skip list, red-black tree | O(log n), range scans |
| Static ordered lookups | sorted array + binary search (or Eytzinger layout) | fastest, no pointers |
| Smallest/largest repeatedly | binary or 4-ary heap | O(log n), cache-friendly |
| Prefix / longest-prefix queries | trie, radix tree, DIR-24-8 for IPv4 | O(key length) |
| Range sums with point updates | Fenwick tree | smallest and fastest |
| Range min/max, static | sparse table | O(1) query |
| Dynamic connectivity (only merges) | union–find | ~O(1) amortised |
| Shortest paths | BFS / 0-1 BFS / Dijkstra / Bellman–Ford | depends on weights |
| Dependency ordering | topological sort | O(V+E), reports cycles |
| Many substrings/patterns | Aho–Corasick; `memmem` for one | O(n) scan |
| Fixed-size objects, heavy churn | pool allocator | ~10x `malloc` |
| Bounded cache of recent results | LRU (hash + list) / CLOCK | O(1) |
| Write-heavy ordered store on disk | LSM tree | sequential I/O |
| Optimisation with overlapping subproblems | dynamic programming | polynomial |
| NP-hard, small `n` | backtracking + pruning, bitmask DP | exact |
| Producer/consumer between two threads | SPSC ring buffer | no locks, no CAS |

## Complexity of the structures built in this track

(`n` = elements, `h` = tree height, `k` = key length, `B` = node fan-out, `α` = inverse Ackermann; "amort." = amortised, "exp." = expected.)

| Structure | Search | Insert | Delete | Notes / where |
|---|---|---|---|---|
| Dynamic array | O(n) / O(1) by index | O(1) amort. at end | O(n) | dynamic arrays |
| Singly / doubly linked list | O(n) | O(1) at known node | O(1) at known node | linked lists |
| Stack / queue / ring buffer | – | O(1) | O(1) | stacks and queues |
| Sorted array | O(log n) | O(n) | O(n) | binary search |
| Hash table (open addressing) | O(1) exp. | O(1) amort. | O(1) exp. | hash tables |
| Bloom / Count-Min / HyperLogLog | O(k hashes) | O(k hashes) | – (counting variants) | probabilistic structures |
| Binary search tree | O(h) | O(h) | O(h) | h = n worst case |
| AVL / red-black tree | O(log n) | O(log n) | O(log n) | self-balancing trees |
| Treap / skip list | O(log n) exp. | O(log n) exp. | O(log n) exp. | randomised |
| Binary heap | O(n) find | O(log n) (O(1) avg) | O(log n) extract-min | build O(n) |
| Trie / radix tree | O(k) | O(k) | O(k) | independent of n |
| B-tree / B+ tree | O(log_B n) | O(log_B n) | O(log_B n) | page-oriented |
| LSM tree | O(levels) with Bloom filters | O(1) amort. (append) | tombstone | write-optimised |
| Fenwick tree | O(log n) prefix | O(log n) point update | – | invertible operations |
| Segment tree (lazy) | O(log n) range | O(log n) range update | – | any associative op |
| Sparse table | O(1) range min | rebuild | – | O(n log n) memory |
| Union–find | O(α(n)) | O(α(n)) | – | with both heuristics |
| Adjacency list / CSR | O(deg) edge test | O(1) / rebuild | – | graph representations |
| LRU cache (hash + list) | O(1) | O(1) | O(1) evict | caches |
| Boundary-tag allocator | O(free blocks) fit search | – | O(1) coalescing | allocators |
| Michael–Scott queue | – | O(1) with CAS retries | O(1) with CAS retries | concurrent structures |

| Algorithm | Time | Space | Note |
|---|---|---|---|
| Quicksort / mergesort / heapsort | O(n log n) avg / O(n log n) / O(n log n) | O(log n) / O(n) / O(1) | comparison sorts |
| Counting / radix sort | O(n + k) / O(d(n + k)) | O(n + k) | non-comparison sorts |
| Quickselect / median of medians | O(n) avg / O(n) worst | O(1) | selection |
| BFS / DFS / topological sort | O(V + E) | O(V) | traversals |
| Dijkstra (binary heap) | O((V + E) log V) | O(V) | non-negative weights |
| Bellman–Ford / Floyd–Warshall | O(VE) / O(V³) | O(V) / O(V²) | negative edges / all pairs |
| Kruskal / Prim | O(E log E) / O(E log V) | O(V) | MST |
| Tarjan SCC, bridges | O(V + E) | O(V) | one DFS |
| Dinic max-flow | O(V²E) | O(V + E) | unit networks O(E√V) |
| KMP / Z / Aho–Corasick | O(n + m) / O(n + m) / O(n + Σm) | O(m) | string matching |
| Levenshtein DP | O(nm) (banded O(nk)) | O(min(n, m)) | edit distance |
| Knapsack DP | O(nW) | O(W) | pseudo-polynomial |
| Held–Karp TSP | O(2ⁿn²) | O(2ⁿn) | bitmask DP |
| Miller–Rabin | O(k log² n) | O(1) | 12 bases deterministic for 64 bits |

## A reality check: what the machine charges

Big-O counts operations; real cost per operation spans a factor of fifty depending on where the data lives. The program measures *dependent-load latency* (each load's address depends on the previous result, so nothing can be overlapped) for working sets from 16 KiB up to 256 MiB, and sequential read bandwidth.

{{include dsa/patterns/latency.c}}

{{run dsa/patterns :: gcc -std=gnu11 -O2 -Wall -Wextra latency.c -o latency && ./latency}}

A random access costs a few nanoseconds when the data sits in the L1 cache, tens when it spills to L2 or L3, and **well over a hundred nanoseconds** when it has to come from main memory — the figures above are from a virtual machine and vary by hardware, but the shape is universal. Sequential scanning, which the hardware prefetcher can run ahead of, delivers gigabytes per second. Consequences you have measured throughout this track:

- A pointer-based linked list or binary tree pays the *latency* for every node; an array, CSR graph or B-tree node pays it once per cache line — hence the 6–10× BFS speedup, the sorted-array win over balanced trees, and wide B-tree nodes.
- A hash lookup into a table larger than the cache costs one miss, so ~100 ns rather than the "O(1)" you pictured; a Bloom filter that fits in cache turns a disk read into a few nanoseconds.
- Algorithms with **few passes over contiguous data** (radix sort, segmented sieve, scans with prefetching) beat algorithms with fewer total operations but random access.
- At a working set that fits in L1, a linear scan of 100 elements beats most "better" structures; below a few hundred elements, prefer the simplest array code.

## How to approach a new problem

1. **Restate it and extract the constraints**: `n`, value ranges, time limit. Constraints suggest the target complexity (`n ≤ 20` → exponential/bitmask; `n ≤ 5000` → `O(n²)`; `n ≤ 10⁶` → `O(n log n)`; `n ≥ 10⁷` → `O(n)` or sublinear).
2. **Write the brute force** and keep it as the oracle (every program in this track does).
3. **Look for structure**: sorted? monotone? contiguous? overlapping subproblems? a graph in disguise? small state?
4. **Match a pattern** from the sections above and the decision table.
5. **Check edge cases**: empty input, one element, all equal, duplicates, negative values, overflow, maximum size.
6. **Test against the oracle** on thousands of small random inputs, then measure at real scale.

## Pitfalls

- **Optimising before measuring.** Big-O guides the choice; a profiler and a benchmark decide it.
- **Using a pattern outside its precondition** (sliding window with negative numbers, two pointers on unsorted data, binary search on a non-monotone predicate): the code runs and returns wrong answers.
- **Ignoring constants and memory hierarchy**: the better asymptotic structure loses at small `n` or with poor locality.
- **Stopping at the first working complexity**: check whether the constraints permit a simpler `O(n log n)` solution before building a complicated `O(n)` one.
- **Complexity of building**: sparse tables, tries and suffix structures have large construction costs and memory; count them when queries are few.
- **Forgetting output size**: an algorithm that must print `O(n²)` results cannot be faster than that.

## Key takeaways

1. Most "hard" array problems are solved by one of a small set of patterns: two pointers, sliding window, monotonic stack, prefix sums with a hash map, interval merging, binary search on the answer, and a bounded heap — each with a precondition you must check.
2. Pick data structures by operation mix, staticness, ordering needs, size relative to cache, and exactness; use the decision table and the complexity tables as a starting point.
3. Constants and cache behavior reorder the rankings that big-O suggests: a main-memory access costs on the order of a hundred nanoseconds, a cache hit a few.
4. Always keep a brute-force oracle and test against it on small random inputs.
5. Simple, contiguous data layouts win below the cache sizes; sophisticated structures pay off as data grows and as access patterns demand them.
