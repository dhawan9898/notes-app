---
title: Comparison Sorts — Insertion, Merge, Quick, Heap and Introsort
track: dsa
module: Searching and Sorting
moduleOrder: 3
order: 2
level: advanced
summary: Six sorting algorithms implemented and verified in C — exact comparison counts, a million-element benchmark over five input patterns, stability, the quadratic trap that caught one of our own implementations, and how production library sorts combine these ideas.
prerequisites:
  - Recursion, Divide and Conquer, and Loop Invariants
  - "Searching and Sorting for Beginners"
sources:
  - label: Knuth — The Art of Computer Programming, Vol. 3, Ch. 5 (Sorting)
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 2, 6, 7, 8.1 (comparison sort lower bound)
  - label: Musser — Introspective Sorting and Selection Algorithms (Software: Practice and Experience, 1997)
  - label: Sedgewick & Bentley — Quicksort is optimal (talk, 2002); Bentley & McIlroy — Engineering a Sort Function (SP&E, 1993)
  - label: Edelkamp & Weiß — BlockQuicksort - How Branch Mispredictions don't affect Quicksort (ESA 2016); Peters — Timsort (CPython listsort.txt)
    url: https://github.com/python/cpython/blob/main/Objects/listsort.txt
  - label: "Orson Peters — Pattern-defeating Quicksort (pdqsort), 2021"
    url: https://arxiv.org/abs/2106.05123
---

Sorting is the most studied problem in computer science and a daily tool: ordering enables binary search, deduplication, merging, grouping, and many algorithms. This note implements the classical comparison sorts in C, verifies them, and measures them. A **comparison sort** only learns about the data by asking "is `a < b`?".

## Properties to compare

| Property | Meaning | Why you care |
|---|---|---|
| **Time** (worst / average / best) | comparisons and data movement | guarantees vs typical speed |
| **Stable** | equal keys keep their original relative order | multi-key sorting (sort by name, then stable-sort by department); required by many APIs |
| **In place** | O(1) (or O(log n)) extra memory | memory-constrained systems |
| **Adaptive** | faster on nearly sorted input | real data is rarely random |
| **Cache behavior** | sequential vs scattered access | dominates real performance at large n |
| **Online** | can sort as items arrive | insertion sort; streaming |
| **Parallelizable** | independent subproblems | merge/quick sort split well |

**Lower bound**: any comparison sort needs at least `⌈log₂ n!⌉ ≈ n log₂ n − 1.44 n` comparisons in the worst case (decision-tree argument, complexity note). For n = 100,000 that is ≈ 1,516,704 comparisons; an algorithm within a few percent of it is essentially optimal in comparison count.

## The implementations

All algorithms below live in one header that routes every comparison through an `LT(a, b)` macro, so a counting build can measure comparisons exactly. They sort `int` arrays for clarity (real libraries are generic over element type and comparator).

{{include dsa/sorting/sorts.h}}

Validation: the test sorts arrays of length 0–300 in six patterns (random, sorted, reversed, few distinct, organ pipe, all equal) with all seven algorithms — including libc `qsort` — and compares each result with `qsort`'s, under AddressSanitizer/UBSan:

{{run dsa/sorting :: gcc -std=gnu11 -O1 -Wall -Wextra -fsanitize=address,undefined test_sorts.c -o test_sorts && ./test_sorts}}

## Algorithm by algorithm

### Insertion sort — Θ(n²) worst, Θ(n) on sorted input, stable, in place

Grows a sorted prefix: take the next element, shift larger ones right, drop it in. Invariant: `a[0..i)` is sorted (proved in the recursion note). Slow in general (20,000 random ints took ~96 ms; the same algorithm on a million would need minutes), but unbeatable for **small arrays** (n ≲ 16–32: no recursion, no function-call overhead, tiny constants) and for **nearly sorted** data (cost = number of inversions + n). Every serious sort uses it as the base case.

### Selection and bubble sort — Θ(n²) always (don't use)

Selection sort does n−1 swaps (useful only when writes are expensive) but is *not stable* in its swap-based form (the test shows it breaking order of equal keys). Bubble sort has no practical advantage over insertion sort.

### Merge sort — Θ(n log n) always, stable, O(n) extra space

Split in half, sort each, **merge**. The implementation copies only the left half to a temporary buffer, merges back, takes the left element on ties (stability), skips the merge when the halves are already ordered (`a[m-1] <= a[m]` makes sorted input O(n): 2 ms for a million sorted ints) and uses insertion sort below 17 elements. Properties: the best *worst-case comparison count* (≈ `n log₂ n − 0.9n`: 1,646,872 at n = 100,000 against the 1,516,704 minimum), sequential memory access (cache-friendly), naturally parallel, the basis of **external sorting** (merge sorted runs from disk/network) and of sorting **linked lists** (no random access needed). Costs: the auxiliary buffer and element copies. Bottom-up and natural merge sorts avoid recursion.

### Quicksort — Θ(n log n) expected, Θ(n²) worst, in place, not stable

Pick a **pivot**, partition into `< pivot` and `> pivot`, recurse. Fastest in practice for random in-memory data: sequential partition scans, in place, few data movements.

- **Partitioning**: *Lomuto* (one scan, simple, many swaps, degrades with duplicates) vs *Hoare* (two scans meeting in the middle, fewer swaps, **both scans stop on keys equal to the pivot**, which splits arrays of equal keys evenly). The code uses Hoare.
- **Pivot choice**: first/last element is quadratic on sorted input; the **median of three** (first, middle, last) handles sorted, reversed and organ-pipe inputs (all fast above) but still has adversarial inputs ("median-of-3 killer" sequences, McIlroy 1999); a **random** pivot makes the *expected* time O(n log n) for every input; **ninther** (median of medians of three) for large arrays.
- **Stack depth**: recurse into the smaller partition and loop on the larger: depth O(log n) always, even in bad cases (otherwise sorted input could overflow the stack).
- **Duplicates**: 3-way partitioning (`< pivot`, `== pivot`, `> pivot`; Dijkstra's Dutch national flag) makes inputs with few distinct keys O(n): 24 ms vs 45 ms for plain Hoare on 8 distinct values.
- **A bug worth remembering.** Our first 3-way implementation used the middle element as pivot, passed every correctness test, and then ran **for minutes** on the "organ pipe" input (`0,1,…,n/2,…,1,0`) because there the middle element is *always the maximum*, so each partition peels off two elements: Θ(n²). Correctness tests do not detect performance cliffs; benchmark across input patterns (sorted, reversed, organ pipe, few distinct, sawtooth, adversarial) before trusting a sort. The fix — a random pivot — appears in the code above.
- **Cutoff**: stop at ~16 elements and finish with one insertion-sort pass.

### Heapsort — Θ(n log n) worst case, in place, not stable

**What is a heap?** A (max-)heap is an array read as a binary tree: the children of the item at index `i` sit at `2i+1` and `2i+2`, and the rule is that every parent is **at least as large as its children**, so the largest item is always at index 0. *Sifting down* means swapping an item with its larger child until the rule holds again. Heaps get their own note, *Heaps and Priority Queues*, later in the track; this paragraph is all you need for heapsort.

Build a max-heap in O(n), then repeatedly swap the maximum to the end and sift down. Guarantees O(n log n) with O(1) extra space — but it is typically 1.5–2× slower than quicksort/merge sort on large arrays because sift-down jumps around memory (poor locality) and does two comparisons per level. Its role is as the **fallback** that makes quicksort safe.

### Introsort — quicksort + heapsort + insertion sort

David Musser's introspective sort (used by C++ `std::sort`, .NET, and GNU libstdc++) runs quicksort but counts recursion depth; if it exceeds `2·log₂ n` the partitioning is degenerating, so it switches to heapsort for that subarray, guaranteeing **Θ(n log n) worst case** while keeping quicksort's typical speed; small subarrays go to insertion sort.

## Measured: comparisons

With the counting build (`COUNT_CMPS`) on 100,000 elements (every comparison is counted; the exact numbers come from the program):

{{include dsa/sorting/count_cmps.c}}

{{run dsa/sorting :: gcc -std=gnu11 -O2 -Wall -Wextra count_cmps.c -o count_cmps -lm && ./count_cmps}}

- Merge sort is within ~9 % of the information-theoretic minimum on random input and needs only **n−1 comparisons on sorted input**.
- Quicksort uses ~18 % more comparisons than merge sort on random data (but fewer data movements and no auxiliary buffer).
- Heapsort makes about **2 n log₂ n** comparisons (two per level) — roughly double the minimum.
- 3-way quicksort roughly doubles the comparison count on distinct keys (each element is compared with the pivot twice) — worthwhile only with many duplicates.

## Measured: time on one million integers

Five input patterns × six algorithms (milliseconds; the machine is a shared VM so absolute numbers are noisy, relative ones are informative):

{{include dsa/sorting/bench_sorts.c}}

{{run dsa/sorting :: gcc -std=gnu11 -O2 -Wall -Wextra bench_sorts.c -o bench_sorts && ./bench_sorts}}

Reading the table:

- **Random**: merge sort, median-of-3 quicksort and introsort are fastest and close; heapsort and libc `qsort` (comparison through a function pointer per call) are slower.
- **Sorted / reversed**: merge sort with the "already ordered" check finishes in a couple of milliseconds; the Hoare quicksorts do ~20 ms (balanced partitions thanks to median-of-3); heapsort and 3-way are slower. A first-element-pivot quicksort would be quadratic here.
- **Few distinct values**: 3-way quicksort wins (every pass groups all equal keys); heapsort and `qsort` are worst.
- **Organ pipe**: the median-of-3 Hoare versions slow to 100–150 ms (more unbalanced partitions) but remain O(n log n); merge sort is unaffected.
- No algorithm is best everywhere. Library sorts therefore **detect patterns** and mix strategies.

## Stability

Stable sorts keep the relative order of equal keys; the test records the original index in each element and checks it: insertion sort is **stable**, swap-based selection sort is **not** (as are standard quicksort and heapsort). Use a stable sort — merge sort, insertion sort, Timsort, `std::stable_sort` — when sorting by several keys in passes, or add the original index as a tie-breaker to make any sort stable. `qsort` makes **no stability guarantee**.

## How production sorts work

| Library | Algorithm | Notes |
|---|---|---|
| C `qsort` (glibc) | merge sort with a temporary buffer when memory allows; implementation details have changed across versions | no stability guarantee; a function-pointer call per comparison; for performance write a type-specialized sort or use a macro/generic header |
| C++ `std::sort` | introsort | `std::stable_sort` is merge sort |
| Python `list.sort`, Java object sort, Android | **Timsort**: detects natural ascending/descending runs, extends short runs with insertion sort, merges runs with galloping | stable; O(n) on sorted/reverse-sorted data; O(n log n) worst case |
| Java primitives (`Arrays.sort(int[])`) | dual-pivot quicksort | |
| Go, Rust (unstable) | **pdqsort** / ipnsort (pattern-defeating quicksort), Rust `sort` uses driftsort | adapts to sorted runs, many duplicates, defeats adversarial patterns, branchless partitioning |
| Linux kernel `sort()` / `list_sort()` | heapsort (no recursion, no allocation) / bottom-up merge sort for lists | the kernel prefers bounded stack and no allocation |
| Database engines | external merge sort + replacement selection | data larger than memory |

Techniques in modern implementations: **branchless partitioning** (BlockQuicksort) to avoid mispredictions; **sorting networks** (fixed compare-exchange sequences) for ≤ 16 elements; SIMD sorting (AVX-512 `vqsort`, `x86-simd-sort`); parallel merge/quick sort; and **radix sort** for integer keys (next note) which beats all comparison sorts on large integer arrays.

## Choosing a sort

1. Sorting a small array (≤ 32)? Insertion sort.
2. Need a stable sort or sorting a linked list or data on disk? Merge sort / Timsort.
3. Need the speed of in-place sorting and stability is irrelevant? Introsort / pdqsort (use the language's built-in unstable sort).
4. Need a hard O(n log n) worst-case guarantee and O(1) memory? Heapsort (or introsort).
5. Integers or fixed-length strings in bulk? Radix sort.
6. Many duplicates? 3-way quicksort / pdqsort.
7. Only need the top k or the median? Don't sort: use a heap or quickselect (next note).

## Implementation pitfalls

- **Comparator bugs**: a comparison that is not a *strict weak ordering* (inconsistent, `<=` instead of `<`, floating-point `NaN`, subtraction overflow) can make sorts loop forever, read out of bounds (unguarded scans `while (a[i] < pivot) i++` rely on the pivot acting as a sentinel) or return garbage. Use `(a > b) - (a < b)`.
- **Quadratic cliffs** on specific inputs (fixed pivots, adversarial data). Never use a fixed-pivot quicksort on untrusted input.
- **Recursion depth** (always recurse into the smaller half).
- **Stability assumptions** about `qsort`.
- **Sorting large records** moves a lot of data: sort an array of indices/pointers or keys with tags instead, then permute once.
- **Cache effects**: for n beyond cache, merge-like sequential passes beat random-access algorithms; for n that fits in cache, quicksort variants win.

## Key takeaways

- Comparison sorting costs Ω(n log n); merge sort (stable, sequential, ≈ optimal comparisons), quicksort (fastest in memory, needs good pivots) and heapsort (worst-case guarantee, slower) all meet it; introsort combines them.
- Insertion sort is the right base case and the best algorithm for tiny or nearly sorted inputs (n−1 comparisons for sorted input in merge sort with an order check).
- Pivot selection and input patterns decide quicksort's fate: our middle-element 3-way version went quadratic on organ-pipe input although it passed every correctness test — benchmark across patterns, use random or median-of-3/ninther pivots, and always recurse on the smaller side.
- Stability must be requested explicitly; `qsort` doesn't provide it.
- Real libraries are hybrids (Timsort, pdqsort, introsort); radix sort beats them on integer keys.
