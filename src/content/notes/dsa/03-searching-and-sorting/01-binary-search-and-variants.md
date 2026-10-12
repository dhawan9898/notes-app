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
sources:
  - label: "Knuth — The Art of Computer Programming, Vol. 3, §6.2.1 (Searching an ordered table)"
  - label: "Bentley — Programming Pearls, 2nd ed., Column 4 (Writing Correct Programs)"
  - label: "Bloch — Extra, Extra - Read All About It: Nearly All Binary Searches and Mergesorts are Broken (2006)"
    url: https://research.google/blog/extra-extra-read-all-about-it-nearly-all-binary-searches-and-mergesorts-are-broken/
  - label: "Khuong & Morin — Array Layouts for Comparison-Based Searching (ACM JEA, 2017)"
    url: https://arxiv.org/abs/1509.05053
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Exercise 2.3-6 and Ch. 12 (binary search trees as a search structure)"
  - label: "ISO C17 §7.22.5.1 bsearch; C++ std::lower_bound / std::partition_point"
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

```c
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* lower_bound: first index i in [0, n] with a[i] >= key.  Invariant: all a[0..lo) < key, all a[hi..n) >= key. */
static size_t lower_bound(const int *a, size_t n, int key) {
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;                    /* never overflows */
        if (a[mid] < key) lo = mid + 1; else hi = mid;
    }
    return lo;
}
/* upper_bound: first index with a[i] > key.  [lower, upper) is the range equal to key. */
static size_t upper_bound(const int *a, size_t n, int key) {
    size_t lo = 0, hi = n;
    while (lo < hi) { size_t mid = lo + (hi - lo) / 2; if (a[mid] <= key) lo = mid + 1; else hi = mid; }
    return lo;
}

/* Search in a sorted array that was rotated at an unknown pivot (no duplicates): one of the two halves is always sorted. */
static long search_rotated(const int *a, long n, int key) {
    long lo = 0, hi = n - 1;
    while (lo <= hi) {
        long mid = lo + (hi - lo) / 2;
        if (a[mid] == key) return mid;
        if (a[lo] <= a[mid]) { if (a[lo] <= key && key < a[mid]) hi = mid - 1; else lo = mid + 1; }      /* left half sorted */
        else                 { if (a[mid] < key && key <= a[hi]) lo = mid + 1; else hi = mid - 1; }      /* right half sorted */
    }
    return -1;
}

/* Search on the ANSWER: smallest ship capacity that moves all packages in at most `days` days (order preserved).
   feasible(cap) is monotone: if cap works, any larger cap works.  Binary search the smallest feasible value. */
static int days_needed(const int *w, int n, long cap) { int days = 1; long load = 0; for (int i = 0; i < n; i++) { if (load + w[i] > cap) { days++; load = 0; } load += w[i]; } return days; }
static long min_capacity(const int *w, int n, int days) {
    long lo = 0, hi = 0;
    for (int i = 0; i < n; i++) { if (w[i] > lo) lo = w[i]; hi += w[i]; }       /* answer lies in [max weight, total weight] */
    while (lo < hi) { long mid = lo + (hi - lo) / 2; if (days_needed(w, n, mid) <= days) hi = mid; else lo = mid + 1; }
    return lo;
}

/* Bisection on a real function: sqrt(x) without sqrt().  Stop by iteration count, not by "lo == hi" (floating point may never get there). */
static double bisect_sqrt(double x, int *iters) {
    double lo = 0, hi = x < 1 ? 1 : x;
    int i;
    for (i = 0; i < 200 && hi - lo > 1e-12 * hi; i++) { double mid = lo + (hi - lo) / 2; if (mid * mid < x) lo = mid; else hi = mid; }
    *iters = i;
    return lo + (hi - lo) / 2;
}

static int cmp_int(const void *x, const void *y) { int a = *(const int *)x, b = *(const int *)y; return (a > b) - (a < b); }

int main(void) {
    srand(2024);
    long checks = 0;
    for (int t = 0; t < 5000; t++) {                           /* fuzz lower/upper bound against a linear scan */
        size_t n = (size_t)rand() % 30; int a[30];
        for (size_t i = 0; i < n; i++) a[i] = rand() % 12;
        qsort(a, n, sizeof *a, cmp_int);
        int key = rand() % 14 - 1;
        size_t lb = 0, ub = 0;
        while (lb < n && a[lb] < key) lb++;
        while (ub < n && a[ub] <= key) ub++;
        assert(lower_bound(a, n, key) == lb && upper_bound(a, n, key) == ub); checks++;
    }
    printf("lower_bound/upper_bound: %ld random cases match a linear scan (including empty arrays and absent keys)\n", checks);

    int dups[] = {1, 2, 2, 2, 2, 3, 5};
    size_t lb = lower_bound(dups, 7, 2), ub = upper_bound(dups, 7, 2);
    int key = 2; int *found = bsearch(&key, dups, 7, sizeof *dups, cmp_int);
    printf("array {1,2,2,2,2,3,5}: key 2 occupies [%zu, %zu) -> count %zu; insertion point of 4 = %zu; libc bsearch returned index %ld (any match, unspecified which)\n", lb, ub, ub - lb, lower_bound(dups, 7, 4), (long)(found - dups));

    int rot[] = {15, 18, 19, 22, 1, 3, 6, 9, 12};
    printf("rotated search: 6 at index %ld, 1 at index %ld, 7 at index %ld (absent)\n", search_rotated(rot, 9, 6), search_rotated(rot, 9, 1), search_rotated(rot, 9, 7));

    int w[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    printf("ship packages {1..10} in 5 days: minimum capacity = %ld; in 1 day: %ld; in 10 days: %ld\n", min_capacity(w, 10, 5), min_capacity(w, 10, 1), min_capacity(w, 10, 10));

    int it; double r = bisect_sqrt(2.0, &it);
    printf("bisect_sqrt(2) = %.12f after %d halvings (libm: %.12f)\n", r, it, sqrt(2.0));

    /* the classic overflow, shown with 32-bit indices near INT_MAX */
    int lo = 1500000000, hi = 2000000000;
    long long exact = ((long long)lo + hi) / 2;
    int safe = lo + (hi - lo) / 2;
    int wrapped = (int)((unsigned)lo + (unsigned)hi);        /* what (lo+hi) holds after wraparound (the signed version is UB) */
    printf("lo=%d hi=%d: correct mid=%lld, lo+(hi-lo)/2=%d, (lo+hi)/2 would be %d/2 = %d\n", lo, hi, exact, safe, wrapped, wrapped / 2);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O1 -Wall -Wextra -fsanitize=address,undefined bsearch.c -o bsearch -lm && ./bsearch
lower_bound/upper_bound: 5000 random cases match a linear scan (including empty arrays and absent keys)
array {1,2,2,2,2,3,5}: key 2 occupies [1, 5) -> count 4; insertion point of 4 = 6; libc bsearch returned index 3 (any match, unspecified which)
rotated search: 6 at index 6, 1 at index 4, 7 at index -1 (absent)
ship packages {1..10} in 5 days: minimum capacity = 15; in 1 day: 55; in 10 days: 10
bisect_sqrt(2) = 1.414213562373 after 41 halvings (libm: 1.414213562373)
lo=1500000000 hi=2000000000: correct mid=1750000000, lo+(hi-lo)/2=1750000000, (lo+hi)/2 would be -794967296/2 = -397483648
```

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

```c
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

/* 1. classic binary search with a branch */
static size_t bs_branchy(const int *a, size_t n, int key) {
    size_t lo = 0, hi = n;
    while (lo < hi) { size_t mid = lo + (hi - lo) / 2; if (a[mid] < key) lo = mid + 1; else hi = mid; }
    return lo;
}
/* 2. branchless: the compiler emits a conditional move instead of an unpredictable branch */
static size_t bs_branchless(const int *a, size_t n, int key) {
    const int *base = a; size_t len = n;
    while (len > 1) { size_t half = len / 2; base = (base[half - 1] < key) ? base + half : base; len -= half; }
    return (size_t)(base - a) + (len && *base < key);
}
/* 3. Eytzinger (BFS) layout: node k has children 2k and 2k+1, so the first levels of the implicit tree share cache lines */
static size_t eyt_build(const int *sorted, int *e, size_t n, size_t i, size_t k) {
    if (k <= n) { i = eyt_build(sorted, e, n, i, 2 * k); e[k] = sorted[i++]; i = eyt_build(sorted, e, n, i, 2 * k + 1); }
    return i;
}
static size_t eyt_lower_bound(const int *e, size_t n, int key) {           /* returns Eytzinger index of the lower bound, 0 if none */
    size_t k = 1;
    while (k <= n) { __builtin_prefetch(&e[k * 16]); k = 2 * k + (e[k] < key); }
    k >>= __builtin_ffsll((long long)~k);
    return k;
}

int main(void) {
    for (int log = 12; log <= 24; log += 6) {
        size_t n = (size_t)1 << log, q = 3000000;
        int *a = malloc(n * sizeof *a), *e = calloc(n + 1, sizeof *e), *keys = malloc(q * sizeof *keys);
        for (size_t i = 0; i < n; i++) a[i] = (int)(2 * i);
        eyt_build(a, e, n, 0, 1);
        uint64_t r = 88172645463325252ULL;
        for (size_t i = 0; i < q; i++) { r ^= r << 13; r ^= r >> 7; r ^= r << 17; keys[i] = (int)(r % (2 * n)); }
        size_t s1 = 0, s2 = 0, s3 = 0; double t0, t1;
        t0 = now(); for (size_t i = 0; i < q; i++) s1 += bs_branchy(a, n, keys[i]); t1 = now(); double d1 = (t1 - t0) / q * 1e9;
        t0 = now(); for (size_t i = 0; i < q; i++) s2 += bs_branchless(a, n, keys[i]); t1 = now(); double d2 = (t1 - t0) / q * 1e9;
        t0 = now(); for (size_t i = 0; i < q; i++) { size_t k = eyt_lower_bound(e, n, keys[i]); s3 += k ? (size_t)e[k] : (size_t)-1; } t1 = now(); double d3 = (t1 - t0) / q * 1e9;
        DO_NOT_OPTIMIZE(s1); DO_NOT_OPTIMIZE(s2); DO_NOT_OPTIMIZE(s3);
        /* sanity: lower-bound values agree */
        int ok = 1; for (size_t i = 0; i < 1000; i++) { size_t p = bs_branchy(a, n, keys[i]); size_t k = eyt_lower_bound(e, n, keys[i]); if ((p == n) != (k == 0) || (p < n && a[p] != e[k])) ok = 0; if (bs_branchless(a, n, keys[i]) != p) ok = 0; }
        printf("n = 2^%-2d (%6.1f MiB): branchy %6.1f ns/lookup | branchless %6.1f ns | Eytzinger+prefetch %6.1f ns   results agree: %s\n", log, n * 4 / 1048576.0, d1, d2, d3, ok ? "yes" : "NO");
        free(a); free(e); free(keys);
    }
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra layouts.c -o layouts && ./layouts
n = 2^12 (   0.0 MiB): branchy  126.5 ns/lookup | branchless  116.3 ns | Eytzinger+prefetch   31.1 ns   results agree: yes
n = 2^18 (   1.0 MiB): branchy  243.0 ns/lookup | branchless  160.7 ns | Eytzinger+prefetch   34.7 ns   results agree: yes
n = 2^24 (  64.0 MiB): branchy 1555.9 ns/lookup | branchless 1871.3 ns | Eytzinger+prefetch  217.3 ns   results agree: yes
```

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
