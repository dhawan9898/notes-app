---
title: Binary Search and Its Variants
track: dsa
module: Searching and Sorting
moduleOrder: 3
order: 1
level: intermediate
summary: Getting binary search right — lower/upper bound from a loop invariant, duplicates, rotated arrays, searching the answer space and real functions, the overflow bug, and why cache layout (Eytzinger) can make it 5–7× faster — all fuzz-tested.
prerequisites:
  - Complexity Analysis and Amortized Analysis
  - "Searching and Sorting for Beginners"
sources:
  - label: Knuth — The Art of Computer Programming, Vol. 3, §6.2.1 (Searching an ordered table)
  - label: Bentley — Programming Pearls, 2nd ed., Column 4 (Writing Correct Programs)
  - label: "Bloch — Extra, Extra - Read All About It: Nearly All Binary Searches and Mergesorts are Broken (2006)"
    url: https://research.google/blog/extra-extra-read-all-about-it-nearly-all-binary-searches-and-mergesorts-are-broken/
  - label: Khuong & Morin — Array Layouts for Comparison-Based Searching (ACM JEA, 2017)
    url: https://arxiv.org/abs/1509.05053
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Exercise 2.3-6 and Ch. 12 (binary search trees as a search structure)
  - label: ISO C17 §7.22.5.1 bsearch; C++ std::lower_bound / std::partition_point
---

Binary search finds a value in a **sorted** array in O(log n) comparisons by halving the candidate range each step: a million elements need at most 20 probes. Jon Bentley reported that when he assigned it to professional programmers, about 90 % could not write a correct version in two hours — the details (inclusive vs exclusive bounds, `mid`, termination, duplicates) are where bugs live. The cure is to derive the loop from an invariant.

## The two fundamental forms

Everything below is built from one idea: a **monotone predicate** `P(i)` on indices — false for a prefix of the array, then true — and the search for the first index where it holds.

- `lower_bound(key)`: first index `i` with `a[i] >= key` (predicate `a[i] >= key`). If every element is smaller, the result is `n` (insertion point at the end).
- `upper_bound(key)`: first index with `a[i] > key`.

From them: **membership** is `lb < n && a[lb] == key`; the **count** of `key` is `ub − lb`; the **insertion point** that keeps the array sorted is `lb`; the **range of equal elements** is `[lb, ub)`; the **predecessor** is `lb − 1`; the **successor** is `ub`.

```c
size_t lower_bound(const int *a, size_t n, int key) {
    size_t lo = 0, hi = n;                    /* search interval [lo, hi) -- half-open */
    while (lo < hi) {                         /* invariant: a[0..lo) < key  and  a[hi..n) >= key */
        size_t mid = lo + (hi - lo) / 2;      /* lo <= mid < hi, never overflows */
        if (a[mid] < key) lo = mid + 1;       /* a[mid] and everything left of it is < key */
        else              hi = mid;           /* a[mid] >= key: the answer is at mid or to its left */
    }
    return lo;                                /* lo == hi: the boundary */
}
```

Why this shape is robust: the **half-open interval** `[lo, hi)` makes the empty range `lo == hi` natural (no `-1` index, no `size_t` underflow); each branch maintains the invariant; the interval shrinks every iteration (`mid < hi`, so `hi = mid` decreases and `lo = mid + 1` increases); at termination the invariant pins the answer exactly. Compare the popular `while (lo <= hi) ... hi = mid - 1` closed-interval version, which needs `long`/signed indices and special handling for "not found".

## Fuzz-tested variants

`bsearch.c` implements lower/upper bound, the rotated-array search, search on the answer, and bisection on reals, and tests the bounds against a linear scan on 5,000 random sorted arrays (including empty arrays, duplicates, absent keys on both sides):

{{include dsa/search/bsearch.c}}

{{run dsa/search :: gcc -std=gnu11 -O1 -Wall -Wextra -fsanitize=address,undefined bsearch.c -o bsearch -lm && ./bsearch}}

### Duplicates and `bsearch(3)`

The C library's `bsearch` returns *some* matching element when there are duplicates — the run found index 3 of `{1,2,2,2,2,3,5}`, where `lower_bound` says the run starts at 1 and `upper_bound` says it ends before 5. It also cannot return an insertion point for absent keys. Use your own `lower_bound` whenever you need the first/last occurrence, a count, or a position.

### Rotated sorted array

An ascending array rotated at an unknown pivot (`15 18 19 22 1 3 6 9 12`) still has one **sorted half** in every interval: compare `a[lo]` with `a[mid]` to find which, check whether the key lies inside that sorted half, and discard the other. O(log n) with distinct elements; with duplicates (`1 1 1 3 1`) the "which half is sorted" test can be ambiguous and the worst case degrades to O(n). The pivot itself (the minimum) is found with the predicate `a[i] <= a[n−1]`.

### Binary search on the answer (parametric search)

You don't need an array: any problem asking for the **smallest/largest value satisfying a monotone condition** can be binary searched, provided you can test a candidate quickly. Example: ship packages with weights `1…10` in order using the least ship capacity that finishes in at most `D` days. The feasibility check `days_needed(capacity) <= D` is monotone (a bigger ship never needs more days), the answer lies in `[max weight, total weight]`, and each test is O(n) — total O(n log(total)). The run prints capacity 15 for 5 days, 55 for one day (everything in one load), and 10 for ten days (one package a day). The same pattern solves: minimum time for workers to finish tasks, split array to minimize the largest subarray sum, Koko eating bananas, aggressive cows, smallest divisor, kth smallest in a sorted matrix, integer square root, and — in systems — finding the largest MTU that passes (PMTUD probing), the first bad commit (`git bisect` *is* binary search on history), and the maximum sustainable request rate.

### Bisection on real numbers

For continuous functions (`sqrt`, root finding with a sign change), halve the interval until it is small enough. Stop on a **fixed iteration count** or a **relative tolerance**, never on `lo == hi` (floating-point midpoints may stall before the endpoints meet; for doubles ~60 iterations reach full precision). The run computed `√2` to 12 digits in 41 halvings. For speed use Newton–Raphson (quadratic convergence) with bisection as the safe fallback.

## The overflow bug

`mid = (lo + hi) / 2` is wrong when `lo + hi` exceeds the integer range. It sat in textbooks for decades and was found in the JDK's `Arrays.binarySearch` (shipped since 1997) in 2006, when arrays above 2³⁰ elements became possible. With 32-bit `int`s the sum wraps (signed overflow: undefined behavior in C):

```text
lo=1500000000 hi=2000000000: correct mid=1750000000, lo+(hi-lo)/2=1750000000,
(lo+hi)/2 would be -794967296/2 = -397483648        <- negative index
```

Always write `lo + (hi - lo) / 2` (or use unsigned indices and `(lo + hi) >> 1` only when the range is bounded). The related lesson from the recursion note applies: prove the algorithm *and* the arithmetic.

## Other classic pitfalls

- **Unsorted input.** Binary search has no way to detect it; results are arbitrary. Assert sortedness in debug builds. Sort order must match the comparator: searching with a different comparison function (case-insensitive vs `strcmp`) is a bug.
- **Comparator mismatch**: `bsearch` needs the same ordering `qsort` used; compare with `(a > b) - (a < b)`, never subtraction (overflow).
- **Floating-point keys**: `NaN` breaks ordering; compare with `<` consistently.
- **Off-by-one on closed intervals**: prefer half-open ranges and `lower_bound` formulations.
- **Equal-key stability**: `lower_bound` finds the first duplicate; search for "the last element ≤ key" with `upper_bound − 1`.
- **Linked lists**: binary search on a list is O(n) because indexing is O(n); use skip lists or arrays.
- **Interpolation search** (guess the position from key values) is O(log log n) on uniformly distributed keys but O(n) on skewed data.

## Performance: the memory layout matters more than the algorithm

On modern hardware, binary search on a large sorted array is dominated by **cache misses and branch mispredictions**: each of the ~20 probes jumps to a distant cache line, the first ~10 of which are at far-apart addresses, and the comparison branch is unpredictable (50/50 for random keys). Three implementations over 3 million random lookups (`layouts.c`): the classic branchy loop, a branch-free (conditional-move) version, and the **Eytzinger layout** — store the implicit balanced tree in BFS order (`node k` has children `2k`, `2k+1`) so that the frequently visited top levels of every search are packed into a few cache lines, plus a software prefetch of the grandchildren-of-grandchildren:

{{include dsa/search/layouts.c}}

{{run dsa/search :: gcc -std=gnu11 -O2 -Wall -Wextra layouts.c -o layouts && ./layouts}}

The sorted-array versions cost ~120–1,500 ns per lookup depending on size (the machine is a shared virtual machine, so absolute values are inflated), while the Eytzinger layout is **4–7× faster** at every size — because it uses the cache better and, with the prefetch, overlaps memory latency across levels. The branch-free variant gave little here: the benefit depends on the compiler emitting a conditional move for the data-dependent select, and on the CPU. Takeaways: (1) measure; (2) for read-mostly lookup tables that are searched millions of times, a cache-conscious layout (Eytzinger, B-tree-like blocked layouts, van Emde Boas) beats plain sorted arrays by a large factor; (3) when keys are integers drawn from a bounded range, a hash table, a direct-address table or a radix/trie structure may be better still; (4) for dynamic data, use a B-tree (next modules) — which is the same idea (wide nodes, few cache lines per level) applied to a mutable structure.

## Binary search in the wild

| Place | Use |
|---|---|
| `git bisect` | find the first bad commit among n in log₂ n tests |
| Kernel | `bsearch()` for sorted tables (e.g. ACPI, ID tables), `radix tree`/`rb-tree` for dynamic |
| Databases | B-tree page search is a binary search within a page |
| Routing | longest-prefix match tables (sorted ranges, tries) |
| Standard libraries | `std::lower_bound`, `Arrays.binarySearch`, Python `bisect` |
| Autoscaling / tuning | binary search on a monotone metric (max throughput before latency SLO breaks) |

## Checklist for writing it

1. Define the monotone predicate and what the answer means when it is never/always true.
2. Use half-open `[lo, hi)` with `hi = n` and `mid = lo + (hi - lo) / 2`.
3. Write the invariant as a comment, make both branches maintain it, ensure progress.
4. Test with arrays of length 0, 1, 2, 3, all-equal, keys below/between/above all elements, and fuzz against a linear scan.
5. Check overflow in the *predicate* too (e.g. `mid * mid <= n` with large `n` needs 64-bit math).

## Key takeaways

- Binary search is "find the boundary of a monotone predicate": `lower_bound` and `upper_bound` give membership, counts, ranges and insertion points; `bsearch(3)` returns an arbitrary duplicate.
- Derive the loop from the invariant on a half-open interval; compute `mid` as `lo + (hi - lo) / 2` (the `(lo + hi) / 2` bug lived in the JDK for nine years).
- The same pattern searches answer spaces (capacity, time, threshold) and real intervals (bisection with a fixed iteration count).
- Rotated arrays: one half is always sorted.
- For large read-mostly data, layout dominates: the Eytzinger layout with prefetching was 4–7× faster than the classic sorted array in the measurements.
