---
title: Heaps and Priority Queues
track: dsa
module: Trees
moduleOrder: 5
order: 3
level: intermediate
summary: "The array-embedded binary heap, why building one takes O(n), d-ary heaps and cache behavior (measured on two workloads), the indexed heap with decrease-key for Dijkstra, k-way merge, running median and streaming top-k."
prerequisites:
  - Dynamic Arrays
sources:
  - label: Williams — Algorithm 232, Heapsort (CACM 7(6), 1964)
  - label: Floyd — Algorithm 245, Treesort 3 (CACM 7(12), 1964)
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 6 (Heapsort) and Ch. 19 (Fibonacci heaps)
  - label: Knuth — The Art of Computer Programming, Vol. 3, §5.2.3 (Sorting by Selection)
  - label: Fredman & Tarjan — Fibonacci Heaps and Their Uses in Improved Network Optimization Algorithms (J. ACM 34(3), 1987)
  - label: Fredman, Sedgewick, Sleator, Tarjan — The Pairing Heap, a New Form of Self-Adjusting Heap (Algorithmica 1, 1986)
  - label: LaMarca & Ladner — The Influence of Caches on the Performance of Heaps (ACM JEA 1, 1996)
  - label: Linux kernel — include/linux/min_heap.h
    url: https://github.com/torvalds/linux/blob/master/include/linux/min_heap.h
---

A **priority queue** supports `insert`, `find-min` and `extract-min` (and often `decrease-key`). A **binary heap** implements it by storing a *complete* binary tree in an array and enforcing only a *local* order — each parent is no larger than its children. It is not sorted (the order between siblings and cousins is unspecified), which is exactly why it is cheap: you pay for the one relation you need, the minimum at the root.

## Layout and operations

For a node at index `i` (zero-based): parent `(i−1)/d`, children `d·i+1 … d·i+d`. With `d = 2` that is the classic `2i+1`, `2i+2`. Because the tree is complete, no pointers are stored, there is no per-node allocation, and the height is `⌈log_d(n(d−1)+1)⌉`.

- **push**: append at the end, then **sift up**: swap with the parent while smaller. Worst case `log n`; for random keys the average is O(1).
- **pop**: take the root, move the last element to the root, then **sift down**: swap with the *smaller* child while larger. Worst case `2 log n` comparisons (two per level).
- **build**: Floyd's algorithm sifts down every internal node from the last parent back to the root.

The code shifts elements along the path and writes the moving element once at the end (rather than swapping each step), which saves about two-thirds of the stores.

{{include dsa/heaps/heap.h}}

### Why build is O(n), not O(n log n)

Inserting `n` keys one at a time costs up to `n log n` (a key may sift all the way up). Floyd's bottom-up build is linear: a node at height `h` sifts down at most `h` levels, and at most `⌈n/2^{h+1}⌉` nodes have height `h`, so the total work is bounded by

`Σ_h (n / 2^{h+1}) · h  =  n · Σ h/2^{h+1}  ≤  n`,

i.e. at most `n` swaps and `2n` comparisons — most nodes are near the leaves and move almost nothing. The experiment confirms it and also shows what random pushes cost.

{{include dsa/heaps/build_cost.c}}

{{run dsa/heaps :: gcc -std=gnu11 -O2 -Wall -Wextra -fsanitize=address,undefined build_cost.c -o build_cost && ./build_cost}}

Floyd's build uses about 1.9 comparisons per element at every size — constant, below the `2n` bound. Building by repeated push costs about 2.3 per element on random input: also constant on average, but with a worst case of `n log n` — the last two lines show both extremes (ascending input never sifts; descending input sifts the full height, about `log2 n` = 20 levels for a million keys, giving ~18 comparisons per push). So prefer `heap_build` when you have all the data up front.

## Heapsort in one paragraph

Build the heap (O(n)), then pop `n` times, each `O(log n)`: total `O(n log n)` in-place with no worst-case degradation, and no extra memory when done in place with a max-heap (swap the root with the last element and shrink). It is the fallback inside introsort for exactly that guarantee. In practice it loses to quicksort and merge sort because sift-down touches memory in a pattern with poor locality and its comparisons are hard for the branch predictor — see the sorting notes for measurements.

## d-ary heaps and the cache

A heap of `d` children per node has height `log_d n`, so `sift_up` does fewer swaps (`log_d n`); but `sift_down` compares `d` children at each of those levels (`d·log_d n` comparisons). A larger `d` also puts all `d` children of a node adjacent in memory — one cache line for up to 16 four-byte keys — which cuts cache misses per level at no extra miss cost. LaMarca & Ladner showed that tuning the heap to the cache, not the comparison count, decides performance once the heap is bigger than the cache. The `heap.h` above supports `d` and optionally shifts the root so each block of `d` siblings starts on an aligned boundary (with a 64-byte-aligned allocation).

Whether a larger `d` helps depends on which operation dominates, so the benchmark below runs two workloads. In **A** the pop takes the minimum and the push inserts a *random* key; the heap's contents drift upward over time so the pushed key is usually small and sifts up to near the root (push-dominated). In **B**, an event queue, the push is `min + delay`, so the new key goes to the bottom and the pop's sift-down dominates.

{{include dsa/heaps/bench_d.c}}

{{run dsa/heaps :: gcc -std=gnu11 -O2 -Wall -Wextra bench_d.c -o bench_d && ./bench_d}}

The pattern to take from a single run (absolute numbers vary by machine and from run to run, especially for the biggest heaps where everything is limited by memory latency):

- **Workload A (sift-up heavy):** `d = 4` or `8` is faster than binary (by up to about 1.5× in this run) while the heap fits in the CPU caches — fewer levels to climb and sibling blocks in one line. Beyond the cache, all variants converge on memory latency.
- **Workload B (sift-down heavy):** binary is the best or tied: each level costs `d` comparisons, and at depth the extra comparisons do not buy fewer cache misses since a node's children are already close.
- Hence the conventional advice — a 4-ary heap is a good default when pushes and `decrease-key` dominate (Dijkstra, Prim), while pop-dominated queues gain little. Measure your own mix.

## The indexed heap: decrease-key

Dijkstra's and Prim's algorithms need "lower the priority of vertex `v`", but a plain heap can only find the minimum — locating `v` would cost `O(n)`. An **indexed priority queue** keeps a position array `pos[id]` kept in sync on every swap, so `decrease-key` is `pos` lookup + sift-up: `O(log n)`. The alternative used by many implementations is *lazy deletion*: push a duplicate entry with the smaller key and discard stale entries when popped, which costs extra memory and heap size but avoids the position array.

{{include dsa/heaps/indexed.h}}

## Classic applications

- **k-way merge.** Keep the current head of each of `k` sorted runs in a heap; pop the minimum, replace it with the next element of the same run. `O(N log k)` for `N` elements — the core of external sorting, LSM compaction and merging log files.
- **Running median.** A max-heap of the lower half and a min-heap of the upper half, kept within one element in size: O(log n) per insert and O(1) to read the median (the code negates keys to reuse the min-heap).
- **Streaming top-k.** A min-heap of size `k` holds the best `k` seen; a new item enters only if it beats the root. For random data the root is replaced about `k·ln(n/k)` times, so almost all items cost a single comparison.
- **Event simulation, schedulers and timers**: the next event is always the minimum timestamp. (The Linux kernel provides `min_heap` helpers and uses ordered trees for the high-resolution timer queue; network stacks and databases use hierarchical timing wheels or heaps for large timer sets.)

{{include dsa/heaps/applications.c}}

{{run dsa/heaps :: gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined applications.c -o applications -lm && ./applications}}

The first line is a randomized differential test of the indexed heap against a linear scan. The merge table shows heap comparisons per element growing like `log2 k` (about 15 per element at `k = 512`, versus up to 512 for scanning all heads), and the top-k line matches the `k·ln(n/k)` estimate closely: out of ten million items about 0.01 % ever touched the heap.

## Beyond the binary heap

| Structure | insert | extract-min | decrease-key | merge | Notes |
|---|---|---|---|---|---|
| Binary heap | O(log n) (O(1) avg) | O(log n) | O(log n) with index | O(n) | Best constants, cache-friendly |
| d-ary heap | O(log_d n) | O(d log_d n) | O(log_d n) | O(n) | Tune `d` to workload and cache |
| Binomial heap | O(log n) | O(log n) | O(log n) | O(log n) | Mergeable |
| Fibonacci heap | O(1) amortized | O(log n) amortized | O(1) amortized | O(1) | Best asymptotics for Dijkstra O(E + V log V); heavy constants, pointer-rich, rarely wins in practice |
| Pairing heap | O(1) | O(log n) amortized | o(log n) amortized (exact bound open) | O(1) | Simple, often fastest among pointer-based heaps |
| Radix / bucket queues | O(1) | O(C) | O(1) | – | For small integer priorities (BFS, Dial's algorithm) |

A practical rule: use an array-based binary or 4-ary heap unless you have measured a reason not to; the Fibonacci heap's better bound matters only for very dense graphs.

## Pitfalls

- **Heap order is not sorted order.** Iterating the array does not give sorted output; only repeated pops do (`O(n log n)`).
- **`std::priority_queue`-style APIs have no `decrease-key`.** Either build an indexed heap or use lazy deletion and tolerate stale entries.
- **Stability.** Heaps do not preserve insertion order among equal keys. Add a sequence number to the key if FIFO behavior is required (important for schedulers).
- **Off-by-one in index arithmetic** when switching between one-based and zero-based layouts; the last parent is `(n−2)/d`.
- **Comparator misuse**: for a max-heap with a min-heap implementation, negating keys can overflow at `INT_MIN`; pass a comparator instead.
- **Memory growth** with lazy deletion in Dijkstra on dense graphs: each relaxation may push a new entry.

## Key takeaways

1. A heap stores a complete tree in an array and enforces only parent ≤ child, giving `O(log n)` push/pop and `O(1)` peek with no pointers.
2. Floyd's bottom-up build is `O(n)` (about 1.9 comparisons per element measured), better than `n` pushes in the worst case.
3. A `d`-ary heap trades fewer levels for more comparisons per level and better cache use; d = 4 is a good default when pushes or decrease-key dominate, and binary is as good or better when pops dominate.
4. For Dijkstra and Prim use an indexed heap (position array) or lazy deletion for `decrease-key`.
5. Heaps power k-way merge, running medians, streaming top-k, timers and event simulation.
6. Heap order is not sorted order, and heaps are not stable.
