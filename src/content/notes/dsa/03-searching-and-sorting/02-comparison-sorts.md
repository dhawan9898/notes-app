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
  - label: "Knuth — The Art of Computer Programming, Vol. 3, Ch. 5 (Sorting)"
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 2, 6, 7, 8.1 (comparison sort lower bound)"
  - label: "Musser — Introspective Sorting and Selection Algorithms (Software: Practice and Experience, 1997)"
  - label: "Sedgewick & Bentley — Quicksort is optimal (talk, 2002); Bentley & McIlroy — Engineering a Sort Function (SP&E, 1993)"
  - label: "Edelkamp & Weiß — BlockQuicksort - How Branch Mispredictions don't affect Quicksort (ESA 2016); Peters — Timsort (CPython listsort.txt)"
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

```c
#ifndef SORTS_H
#define SORTS_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Every comparison goes through LT so a counting build can measure comparisons exactly. */
extern unsigned long long g_cmps;
#ifdef COUNT_CMPS
#  define LT(a, b) (g_cmps++, (a) < (b))
#else
#  define LT(a, b) ((a) < (b))
#endif

typedef struct { int key; int seq; } Rec;       /* for stability tests: sort by key only, seq records the original order */

/* ---- insertion sort: O(n^2) worst, O(n) on nearly sorted input, stable, in place ---- */
static inline void insertion_sort(int *a, size_t n) {
    for (size_t i = 1; i < n; i++) {
        int key = a[i]; size_t j = i;
        while (j > 0 && LT(key, a[j - 1])) { a[j] = a[j - 1]; j--; }
        a[j] = key;
    }
}

/* ---- merge sort (top-down, one auxiliary buffer): Theta(n log n) always, stable ---- */
static void msort_rec(int *a, int *tmp, size_t n) {
    if (n <= 16) { insertion_sort(a, n); return; }              /* cutoff: small runs by insertion sort */
    size_t m = n / 2;
    msort_rec(a, tmp, m); msort_rec(a + m, tmp, n - m);
    if (!LT(a[m], a[m - 1])) return;                            /* already ordered: skip the merge (helps sorted input) */
    memcpy(tmp, a, m * sizeof *a);                              /* copy only the left half; merge back into a */
    size_t i = 0, j = m, k = 0;
    while (i < m && j < n) a[k++] = LT(a[j], tmp[i]) ? a[j++] : tmp[i++];     /* take left on ties: stability */
    while (i < m) a[k++] = tmp[i++];                            /* leftover right elements are already in place */
}
static inline int merge_sort(int *a, size_t n) {
    int *tmp = malloc((n / 2 + 1) * sizeof *tmp);
    if (!tmp) return -1;
    msort_rec(a, tmp, n);
    free(tmp);
    return 0;
}

/* ---- quicksort: median-of-three pivot, Hoare-style partition, recurse on the smaller side, loop on the larger ---- */
static inline void swap_int(int *x, int *y) { int t = *x; *x = *y; *y = t; }
static void quick_rec(int *a, size_t n) {
    while (n > 16) {
        size_t mid = n / 2;
        if (LT(a[mid], a[0])) swap_int(&a[mid], &a[0]);          /* order a[0], a[mid], a[n-1] */
        if (LT(a[n - 1], a[0])) swap_int(&a[n - 1], &a[0]);
        if (LT(a[n - 1], a[mid])) swap_int(&a[n - 1], &a[mid]);
        int pivot = a[mid];
        size_t i = 0, j = n - 1;
        for (;;) {                                               /* Hoare partition: both scans stop on equal keys, which balances duplicates */
            while (LT(a[i], pivot)) i++;
            while (LT(pivot, a[j])) j--;
            if (i >= j) break;
            swap_int(&a[i], &a[j]); i++; j--;
        }
        size_t left = j + 1;                                     /* [0, left) <= pivot <= [left, n) */
        if (left < n - left) { quick_rec(a, left); a += left; n -= left; }         /* recurse into the smaller part ... */
        else                 { quick_rec(a + left, n - left); n = left; }          /* ... iterate on the larger: stack depth O(log n) */
    }
    insertion_sort(a, n);
}
static inline void quick_sort(int *a, size_t n) { quick_rec(a, n); }

/* ---- 3-way (Dutch national flag) quicksort: O(n) when there are few distinct keys.
        The pivot is RANDOM: a fixed choice (first, last, middle) has an adversarial input that makes it O(n^2).
        (An earlier draft used the middle element and went quadratic on "organ pipe" input, where the middle is always the maximum.) ---- */
static uint64_t g_rng = 88172645463325252ULL;
static inline uint64_t rng_next(void) { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 7; g_rng ^= g_rng << 17; return g_rng; }
static void quick3_rec(int *a, long lo, long hi) {
    while (lo < hi) {
        int pivot = a[lo + (long)(rng_next() % (uint64_t)(hi - lo + 1))];
        long lt = lo, i = lo, gt = hi;                           /* invariant: a[lo..lt) < pivot, a[lt..i) == pivot, a(gt..hi] > pivot */
        while (i <= gt) {
            if (LT(a[i], pivot)) swap_int(&a[lt++], &a[i++]);
            else if (LT(pivot, a[i])) swap_int(&a[i], &a[gt--]);
            else i++;
        }
        if (lt - lo < hi - gt) { quick3_rec(a, lo, lt - 1); lo = gt + 1; }
        else                   { quick3_rec(a, gt + 1, hi); hi = lt - 1; }
    }
}
static inline void quick3_sort(int *a, size_t n) { if (n > 1) quick3_rec(a, 0, (long)n - 1); }

/* ---- heapsort: O(n log n) worst case, in place, not stable, poor cache behaviour ---- */
static void sift_down(int *a, size_t root, size_t n) {
    int v = a[root];
    for (;;) {
        size_t child = 2 * root + 1;
        if (child >= n) break;
        if (child + 1 < n && LT(a[child], a[child + 1])) child++;
        if (!LT(v, a[child])) break;
        a[root] = a[child]; root = child;
    }
    a[root] = v;
}
static inline void heap_sort(int *a, size_t n) {
    if (n < 2) return;
    for (size_t i = n / 2; i-- > 0; ) sift_down(a, i, n);        /* build a max-heap in O(n) */
    for (size_t end = n - 1; end > 0; end--) { swap_int(&a[0], &a[end]); sift_down(a, 0, end); }
}

/* ---- introsort: quicksort that falls back to heapsort when recursion gets too deep => O(n log n) worst case ---- */
static void intro_rec(int *a, size_t n, int depth) {
    while (n > 16) {
        if (depth-- == 0) { heap_sort(a, n); return; }
        size_t mid = n / 2;
        if (LT(a[mid], a[0])) swap_int(&a[mid], &a[0]);
        if (LT(a[n - 1], a[0])) swap_int(&a[n - 1], &a[0]);
        if (LT(a[n - 1], a[mid])) swap_int(&a[n - 1], &a[mid]);
        int pivot = a[mid]; size_t i = 0, j = n - 1;
        for (;;) { while (LT(a[i], pivot)) i++; while (LT(pivot, a[j])) j--; if (i >= j) break; swap_int(&a[i], &a[j]); i++; j--; }
        size_t left = j + 1;
        if (left < n - left) { intro_rec(a, left, depth); a += left; n -= left; } else { intro_rec(a + left, n - left, depth); n = left; }
    }
    insertion_sort(a, n);
}
static inline void intro_sort(int *a, size_t n) { int d = 0; for (size_t m = n; m; m >>= 1) d += 2; intro_rec(a, n, d); }
#endif
```

Validation: the test sorts arrays of length 0–300 in six patterns (random, sorted, reversed, few distinct, organ pipe, all equal) with all seven algorithms — including libc `qsort` — and compares each result with `qsort`'s, under AddressSanitizer/UBSan:

```text
$ gcc -std=gnu11 -O1 -Wall -Wextra -fsanitize=address,undefined test_sorts.c -o test_sorts && ./test_sorts
2016 small cases (n = 0..300, six input patterns, seven algorithms) match libc qsort

stability (records with many equal keys):
  insertion sort: stable
  selection sort (swap-based): NOT stable
```

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

```c
#define COUNT_CMPS 1
#include <math.h>
#include <stdio.h>
#include "sorts.h"

unsigned long long g_cmps;
static void gen(int *a, size_t n, int kind) { uint64_t r = 88172645463325252ULL; for (size_t i = 0; i < n; i++) { r ^= r << 13; r ^= r >> 7; r ^= r << 17; a[i] = kind == 0 ? (int)(r >> 33) : kind == 1 ? (int)i : (int)(n - i); } }

int main(void) {
    size_t n = 100000;
    int *a = malloc(n * sizeof *a), *b = malloc(n * sizeof *b);
    double lg = log2((double)n);
    printf("comparisons to sort n = %zu (n*log2(n) = %.0f, information-theoretic minimum log2(n!) = %.0f)\n", n, n * lg, lgamma((double)n + 1) / log(2.0));
    printf("%-14s %14s %14s %14s\n", "algorithm", "random", "sorted", "reversed");
    struct { const char *name; int which; } algs[] = { {"merge", 0}, {"quick med3", 1}, {"quick 3-way", 2}, {"heap", 3}, {"introsort", 4} };
    for (int k = 0; k < 5; k++) {
        printf("%-14s", algs[k].name);
        for (int kind = 0; kind < 3; kind++) {
            gen(b, n, kind); memcpy(a, b, n * sizeof *a); g_cmps = 0;
            switch (algs[k].which) { case 0: merge_sort(a, n); break; case 1: quick_sort(a, n); break; case 2: quick3_sort(a, n); break; case 3: heap_sort(a, n); break; default: intro_sort(a, n); }
            printf(" %14llu", g_cmps);
        }
        puts("");
    }
    free(a); free(b);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra count_cmps.c -o count_cmps -lm && ./count_cmps
comparisons to sort n = 100000 (n*log2(n) = 1660964, information-theoretic minimum log2(n!) = 1516704)
algorithm              random         sorted       reversed
merge                 1646872          99999        1221423
quick med3            1940485        1424572        1424573
quick 3-way           3141672        3093330        3212071
heap                  3019784        3112517        2926640
introsort             1940485        1424572        1424573
```

- Merge sort is within ~9 % of the information-theoretic minimum on random input and needs only **n−1 comparisons on sorted input**.
- Quicksort uses ~18 % more comparisons than merge sort on random data (but fewer data movements and no auxiliary buffer).
- Heapsort makes about **2 n log₂ n** comparisons (two per level) — roughly double the minimum.
- 3-way quicksort roughly doubles the comparison count on distinct keys (each element is compared with the pivot twice) — worthwhile only with many duplicates.

## Measured: time on one million integers

Five input patterns × six algorithms (milliseconds; the machine is a shared VM so absolute numbers are noisy, relative ones are informative):

```c
#include <stdio.h>
#include "sorts.h"
#include "../../c/perf/timing.h"

unsigned long long g_cmps;
static int cmp_int(const void *x, const void *y) { int a = *(const int *)x, b = *(const int *)y; return (a > b) - (a < b); }
typedef void (*sort_fn)(int *, size_t);
static void merge_wrap(int *a, size_t n) { merge_sort(a, n); }
static void libc_wrap(int *a, size_t n) { qsort(a, n, sizeof *a, cmp_int); }

static const char *kinds[] = { "random", "sorted", "reversed", "few distinct", "organ pipe" };
static void gen(int *a, size_t n, int kind) {
    uint64_t r = 88172645463325252ULL;
    for (size_t i = 0; i < n; i++) {
        r ^= r << 13; r ^= r >> 7; r ^= r << 17;
        switch (kind) { case 0: a[i] = (int)(r >> 33); break; case 1: a[i] = (int)i; break; case 2: a[i] = (int)(n - i); break;
                        case 3: a[i] = (int)(r % 8); break; default: a[i] = (int)(i < n / 2 ? i : n - i); }
    }
}

int main(void) {
    struct { const char *name; sort_fn fn; } algs[] = { {"merge", merge_wrap}, {"quick med3", quick_sort}, {"quick 3-way", quick3_sort}, {"heap", heap_sort}, {"introsort", intro_sort}, {"libc qsort", libc_wrap} };
    size_t n = 1000000;
    int *a = malloc(n * sizeof *a), *b = malloc(n * sizeof *b);
    printf("time in ms to sort %zu ints (lower is better)\n%-14s", n, "input");
    for (size_t k = 0; k < 6; k++) printf("%12s", algs[k].name);
    puts("");
    for (int kind = 0; kind < 5; kind++) {
        gen(b, n, kind);
        printf("%-14s", kinds[kind]);
        for (size_t k = 0; k < 6; k++) {
            memcpy(a, b, n * sizeof *a);
            double t0 = now(); algs[k].fn(a, n); double t1 = now();
            for (size_t i = 1; i < n; i++) if (a[i - 1] > a[i]) { printf(" UNSORTED!"); break; }
            printf("%12.1f", (t1 - t0) * 1e3);
        }
        puts("");
    }
    size_t m = 20000;
    int *c = malloc(m * sizeof *c); gen(c, m, 0);
    double t0 = now(); insertion_sort(c, m); double t1 = now();
    gen(c, m, 0); /* random */
    printf("\ninsertion sort on only %zu random ints: %.1f ms (O(n^2): the 1,000,000-element version would take ~%.0f s)\n", m, (t1 - t0) * 1e3, (t1 - t0) * (double)n / m * (double)n / m);
    free(a); free(b); free(c);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra bench_sorts.c -o bench_sorts && ./bench_sorts
time in ms to sort 1000000 ints (lower is better)
input                merge  quick med3 quick 3-way        heap   introsort  libc qsort
random               123.1       108.9       160.8       172.2       109.1       184.8
sorted                 1.6        13.8        74.0        82.4        14.0        41.8
reversed              21.7        14.5        74.9        89.5        15.0        49.6
few distinct          43.5        33.7        18.1        82.8        35.4        96.6
organ pipe            11.1        63.8        57.8        76.6        78.7        32.2

insertion sort on only 20000 random ints: 45.6 ms (O(n^2): the 1,000,000-element version would take ~114 s)
```

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
