---
title: Non-Comparison Sorts and Selection
track: dsa
module: Searching and Sorting
moduleOrder: 3
order: 3
level: advanced
summary: Beating the n log n bound by exploiting key structure (counting, radix, bucket sort — 7–10× faster than qsort on 32-bit keys), and not sorting at all when you only need the k-th element or the top k (quickselect, median of medians, bounded heaps), with external sorting for data larger than memory.
sources:
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 8 (Sorting in Linear Time) and Ch. 9 (Medians and Order Statistics)
  - label: Blum, Floyd, Pratt, Rivest, Tarjan — Time Bounds for Selection (JCSS, 1973)
  - label: Hoare — Algorithm 65: Find (CACM, 1961)
  - label: Knuth — The Art of Computer Programming, Vol. 3, §5.2.5 (Sorting by distribution) and §5.4 (External sorting)
  - label: Terdiman — Radix Sort Revisited (2000); Satish et al. — Designing Efficient Sorting Algorithms for Manycore GPUs (IPDPS 2009)
  - label: Vitter — External Memory Algorithms and Data Structures (ACM Computing Surveys, 2001)
---

The Ω(n log n) lower bound applies only to algorithms that learn about the data **exclusively through comparisons**. When keys are integers, short strings or numbers in a known range, an algorithm can *use the key's value directly* — as an array index, a digit, a bucket number — and sort in linear time. And often you do not need a full sort at all: finding the median, the k-th smallest or the top-10 can be done faster than ordering everything.

## Counting sort — O(n + K), stable

For keys in `[0, K)`: count occurrences of each key, take **prefix sums** so that `count[k]` becomes the first output position of key `k`, then place each element (scanning the input in order to stay stable):

```c
for (i = 0; i < n; i++) count[in[i] + 1]++;                 /* histogram */
for (k = 0; k < K; k++) count[k + 1] += count[k];           /* prefix sums = starting positions */
for (i = 0; i < n; i++) out[count[in[i]]++] = in[i];        /* stable placement */
```

Time O(n + K), space O(n + K). Excellent when `K = O(n)` (ages, grades, byte values, small IDs, characters); useless when `K` is huge (sorting 32-bit keys directly needs a 4-billion-entry table). Counting sort is the building block of radix sort, and the "histogram + prefix sum" pattern is ubiquitous (partitioning, histograms in image processing, parallel scan, GPU sorting).

## Radix sort — O(w · (n + b)) for w digits in base b

Sort by one **digit** at a time with a **stable** counting sort. **LSD** (least significant digit first) processes digits from the lowest to the highest: because each pass is stable, after the pass on digit `j` the array is sorted by the lowest `j+1` digits, so the final pass leaves it fully sorted. For 32-bit keys with 8-bit digits: four passes, a 256-entry histogram each.

Design points:

- **Digit size** trades passes against histogram/cache footprint: 8 bits (4 passes, 256 counters fit in L1) is the usual choice; 11 bits (3 passes, 2,048 counters) can be faster for large arrays; 16 bits (2 passes) thrashes the cache on random scatter.
- **Ping-pong buffers**: each pass reads one array and writes the other (the code swaps pointers; after four passes the data is back in the original array).
- **Signed integers**: flip the sign bit before sorting and after (`x ^ 0x80000000`), so negative numbers order before positives in unsigned comparison; **IEEE 754 floats** sort as integers after the transform "if the sign bit is set, flip all bits; else flip only the sign bit".
- **Strings** (variable length): MSD radix sort (recursive on the first character, switching to insertion sort for small buckets) or multikey quicksort (Bentley–Sedgewick); LSD works for fixed-length keys.
- **Skipping passes**: compute all four histograms in one initial scan, and skip any pass where a single digit value holds all elements (e.g. all keys < 2²⁴).
- **Parallel and GPU** radix sorts are the fastest known general sorts for integers (the pattern: histogram, parallel prefix sum, scatter).

## Bucket sort — expected O(n) for uniform data

Distribute `n` values from a known uniform distribution into `n` buckets by `floor(v · n)`, sort each (tiny) bucket with insertion sort, concatenate. Expected O(n) when the distribution is uniform; O(n²) when everything lands in one bucket. Variants adapt bucket boundaries to the data's quantiles (sample sort), which is how parallel and distributed sorts (MapReduce TeraSort, database partitioning) balance load.

## Verified and measured

`radix.c` implements counting sort (with the prefix-sum placement), LSD radix sort for `uint32_t` and `int32_t`, and bucket sort for doubles in `[0,1)`. It checks them against `qsort` on 300 random arrays (signed and unsigned) and then compares radix sort with `qsort` on random 32-bit keys:

{{include dsa/radix/radix.c}}

{{run dsa/radix :: gcc -std=gnu11 -O2 -Wall -Wextra radix.c -o radix && ./radix}}

LSD radix sort is **7–10× faster than `qsort`** for 32-bit keys: it performs four sequential reads and four predictable-pattern scatters per element, with no data-dependent branches, versus ~20 hard-to-predict comparisons through a function pointer. The advantage grows with key *count* and shrinks with key *width* (64-bit keys need eight passes; the crossover with comparison sorts shifts to larger n). Its weaknesses: extra O(n) memory, cache-unfriendly scatter for very large n or large radix, and it is limited to keys decomposable into digits (not arbitrary comparator-defined orders).

## Selection: order statistics without sorting

**Problem**: find the k-th smallest element (the median is `k = n/2`; the minimum and maximum are the easy cases). Sorting solves it in O(n log n), but the answer needs only O(n).

### Quickselect (Hoare's Find) — expected O(n)

Partition around a pivot like quicksort, but recurse into only the side containing index `k`: the expected sizes are `n + n/2 + n/4 + … = 2n` (also the `T(n) = T(n/2) + n` recurrence in the Master theorem). With a **random** pivot the expected time is linear on every input; worst case O(n²) with astronomically small probability. A 3-way partition makes it robust to duplicates. It is the implementation behind `std::nth_element`, NumPy's `partition`, and "median" functions.

### Median of medians (BFPRT) — worst-case O(n)

Blum, Floyd, Pratt, Rivest and Tarjan (1973) showed selection needs no randomness: split into groups of five, take each group's median, recursively find the median of those medians, and use it as the pivot. This pivot is guaranteed to discard at least ~30 % of the elements, giving `T(n) ≤ T(n/5) + T(7n/10) + O(n) = O(n)`. The constant factor is large, so it is used as a **fallback** (introselect: quickselect with a depth limit, switching to median-of-medians) when worst-case guarantees matter.

### Top-k with a bounded heap — O(n log k), one pass, O(k) memory

To get the k smallest (or largest) elements — the 100 slowest requests, the top 10 scores, the closest 5 neighbors — keep a **max-heap of size k**: while scanning, replace the heap's root whenever a smaller element arrives. It works on **streams** that do not fit in memory.

Verified and measured (`select.c`): the three methods agree with sorting on 500 random arrays (including heavy duplicates), and on 5 million random integers:

{{include dsa/radix/select.c}}

{{run dsa/radix :: gcc -std=gnu11 -O2 -Wall -Wextra select.c -o select && ./select | tail -n 2}}

Finding the median by sorting took ~1.3 s; quickselect found the same value in ~110 ms (12× faster) and median-of-medians in ~250 ms (still 5× faster, with a *worst-case* guarantee); collecting the 100 smallest values with a heap took 5 ms (one pass over the data, O(100) memory) against 1.3 s for the sort.

| Need | Use | Time |
|---|---|---|
| Min / max | one scan | O(n) |
| k-th smallest, one query | quickselect (introselect) | O(n) expected |
| k-th smallest, worst-case guarantee | median of medians | O(n) |
| Top-k, streaming | bounded heap | O(n log k) |
| Many rank queries | sort once, or order-statistic tree / Fenwick tree | O(n log n) + O(log n) each |
| Median of a stream | two heaps (heap note) | O(log n) per item |
| Approximate quantiles on huge streams | t-digest, GK sketch, KLL | O(1/ε) memory |

## External sorting: data larger than memory

When the data does not fit in RAM, I/O dominates, and the algorithm must minimize passes over the disk:

1. **Run generation**: read as much as fits (M items), sort in memory, write a sorted **run** to disk. (*Replacement selection* with a heap produces runs averaging 2M on random data.)
2. **k-way merge**: merge up to `k` runs at once using a min-heap (or tournament/loser tree) holding the current head of each run; output sequentially. With `k` as large as memory buffers allow, the number of merge passes is `⌈log_k(n/M)⌉` — typically 1 or 2.

Cost: `O((n/B) · log_{M/B}(n/B))` block transfers for block size `B` — the optimal external-memory bound. Sequential I/O and large buffers matter far more than comparison counts. This is how `sort(1)`, database `ORDER BY` / index builds and MapReduce shuffles work (`sort -S 2G --parallel=8 -T /tmp`). Parallel/distributed variants sample the keys to choose splitters (sample sort / TeraSort) so each machine sorts an independent key range.

## When to use which

| Situation | Choice |
|---|---|
| Arbitrary comparator, generic data | comparison sort (introsort/Timsort) |
| 32/64-bit integer or fixed-width keys, large n | LSD radix sort (consider skipping empty passes) |
| Small key range (bytes, small ints) | counting sort |
| Uniformly distributed reals | bucket sort or sample sort |
| Strings | MSD radix / multikey quicksort / library sort with `strcmp` |
| Only the median / k-th / top-k | selection or a heap — never a full sort |
| Larger than memory | external merge sort |
| Needs stability | counting/radix (stable) or merge sort |

## Key takeaways

- Counting, radix and bucket sort break the n log n barrier by using key values directly: O(n + K) for small ranges, O(w·n) for w digits. Verified against `qsort` and ~7–10× faster on random 32-bit keys.
- LSD radix sort needs a stable inner sort, ping-pong buffers, a sign-bit flip for signed ints, and chooses digit width by cache size (8 bits is the common default).
- Selection is easier than sorting: quickselect is expected O(n) (12× faster than sort-and-index for the median of 5 M ints), median of medians is worst-case O(n), and a bounded heap gives top-k in a single pass with O(k) memory.
- Data larger than memory needs external merge sort: sorted runs plus a k-way merge, optimizing disk passes rather than comparisons.
