---
title: Non-Comparison Sorts and Selection
track: dsa
module: Searching and Sorting
moduleOrder: 3
order: 3
level: advanced
summary: Beating the n log n bound by exploiting key structure (counting, radix, bucket sort — 7–10× faster than qsort on 32-bit keys), and not sorting at all when you only need the k-th element or the top k (quickselect, median of medians, bounded heaps), with external sorting for data larger than memory.
sources:
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 8 (Sorting in Linear Time) and Ch. 9 (Medians and Order Statistics)"
  - label: "Blum, Floyd, Pratt, Rivest, Tarjan — Time Bounds for Selection (JCSS, 1973)"
  - label: "Hoare — Algorithm 65: Find (CACM, 1961)"
  - label: "Knuth — The Art of Computer Programming, Vol. 3, §5.2.5 (Sorting by distribution) and §5.4 (External sorting)"
  - label: "Terdiman — Radix Sort Revisited (2000); Satish et al. — Designing Efficient Sorting Algorithms for Manycore GPUs (IPDPS 2009)"
  - label: "Vitter — External Memory Algorithms and Data Structures (ACM Computing Surveys, 2001)"
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

```c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

/* ---- counting sort: keys in [0, K).  Stable, O(n + K). ---- */
static void counting_sort(const uint16_t *in, uint16_t *out, size_t n, size_t K) {
    size_t *count = calloc(K + 1, sizeof *count);
    for (size_t i = 0; i < n; i++) count[in[i] + 1]++;          /* histogram, shifted by one */
    for (size_t k = 0; k < K; k++) count[k + 1] += count[k];    /* prefix sums: count[k] = number of keys < k = first output slot of key k */
    for (size_t i = 0; i < n; i++) out[count[in[i]]++] = in[i]; /* scan left to right => stable */
    free(count);
}

/* ---- LSD radix sort on 32-bit unsigned keys with 8-bit digits: 4 stable counting-sort passes. ---- */
static void radix_sort_u32(uint32_t *a, uint32_t *tmp, size_t n) {
    for (int shift = 0; shift < 32; shift += 8) {
        size_t count[257] = {0};
        for (size_t i = 0; i < n; i++) count[((a[i] >> shift) & 0xFF) + 1]++;
        for (int d = 0; d < 256; d++) count[d + 1] += count[d];
        for (size_t i = 0; i < n; i++) tmp[count[(a[i] >> shift) & 0xFF]++] = a[i];
        uint32_t *t = a; a = tmp; tmp = t;                      /* ping-pong buffers; after 4 passes the result is back in the original array */
    }
}

/* signed ints: flip the sign bit so that negative numbers sort before positive ones in unsigned order */
static void radix_sort_i32(int32_t *a, uint32_t *tmp, size_t n) {
    uint32_t *u = (uint32_t *)a;
    for (size_t i = 0; i < n; i++) u[i] ^= 0x80000000u;
    radix_sort_u32(u, tmp, n);
    for (size_t i = 0; i < n; i++) u[i] ^= 0x80000000u;
}

/* ---- bucket sort for floating-point keys uniformly distributed in [0, 1): expected O(n) ---- */
typedef struct { double v; int next; } BNode;
static void bucket_sort(double *a, size_t n) {
    int *head = malloc(n * sizeof *head); BNode *nodes = malloc(n * sizeof *nodes);
    for (size_t i = 0; i < n; i++) head[i] = -1;
    for (size_t i = 0; i < n; i++) {                            /* n buckets; element goes to bucket floor(v * n) */
        size_t b = (size_t)(a[i] * (double)n);
        nodes[i].v = a[i];
        /* insert into the bucket's list keeping it sorted (buckets are tiny on average) */
        int *p = &head[b];
        while (*p != -1 && nodes[*p].v < a[i]) p = &nodes[*p].next;
        nodes[i].next = *p; *p = (int)i;
    }
    size_t k = 0;
    for (size_t b = 0; b < n; b++) for (int p = head[b]; p != -1; p = nodes[p].next) a[k++] = nodes[p].v;
    free(head); free(nodes);
}

static int cmp_u32(const void *x, const void *y) { uint32_t a = *(const uint32_t *)x, b = *(const uint32_t *)y; return (a > b) - (a < b); }
static int cmp_dbl(const void *x, const void *y) { double a = *(const double *)x, b = *(const double *)y; return (a > b) - (a < b); }

int main(void) {
    srand(5);
    /* correctness against qsort */
    for (int t = 0; t < 300; t++) {
        size_t n = (size_t)rand() % 500;
        uint32_t a[500], b[500], tmp[500];
        for (size_t i = 0; i < n; i++) a[i] = ((uint32_t)rand() << 16) ^ (uint32_t)rand();
        memcpy(b, a, n * sizeof *a);
        radix_sort_u32(a, tmp, n); qsort(b, n, sizeof *b, cmp_u32);
        assert(!memcmp(a, b, n * sizeof *a));
        int32_t s[500], sref[500];
        for (size_t i = 0; i < n; i++) s[i] = (int32_t)(((uint32_t)rand() << 16) ^ (uint32_t)rand());
        memcpy(sref, s, n * sizeof *s);
        radix_sort_i32(s, tmp, n);
        for (size_t i = 1; i < n; i++) assert(s[i - 1] <= s[i]);
        double d[500], dref[500];
        for (size_t i = 0; i < n; i++) d[i] = rand() / (RAND_MAX + 1.0);
        memcpy(dref, d, n * sizeof *d);
        bucket_sort(d, n); qsort(dref, n, sizeof *dref, cmp_dbl);
        assert(!memcmp(d, dref, n * sizeof *d));
    }
    printf("counting/radix/bucket sorts match qsort on 300 random arrays (signed and unsigned, n up to 500)\n");

    uint16_t ca[20] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3, 2, 3, 8, 4}, cb[20];
    counting_sort(ca, cb, 20, 10);
    printf("counting sort of digits: "); for (int i = 0; i < 20; i++) printf("%d", cb[i]); puts("");

    /* benchmark: 32-bit keys */
    printf("\n%12s %12s %12s %12s\n", "n", "radix (ms)", "qsort (ms)", "speedup");
    for (size_t n = 100000; n <= 10000000; n *= 10) {
        uint32_t *a = malloc(n * 4), *b = malloc(n * 4), *tmp = malloc(n * 4);
        uint64_t r = 88172645463325252ULL;
        for (size_t i = 0; i < n; i++) { r ^= r << 13; r ^= r >> 7; r ^= r << 17; a[i] = (uint32_t)r; }
        memcpy(b, a, n * 4);
        double t0 = now(); radix_sort_u32(a, tmp, n); double t1 = now();
        qsort(b, n, 4, cmp_u32); double t2 = now();
        assert(!memcmp(a, b, n * 4));
        printf("%12zu %12.1f %12.1f %11.1fx\n", n, (t1 - t0) * 1e3, (t2 - t1) * 1e3, (t2 - t1) / (t1 - t0));
        free(a); free(b); free(tmp);
    }
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra radix.c -o radix && ./radix
counting/radix/bucket sorts match qsort on 300 random arrays (signed and unsigned, n up to 500)
counting sort of digits: 11223333445556788999

           n   radix (ms)   qsort (ms)      speedup
      100000          2.1         19.4         9.1x
     1000000         23.1        236.4        10.2x
    10000000        285.8       1636.3         5.7x
```

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

```c
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

static uint64_t rng = 88172645463325252ULL;
static uint64_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }
static void swap_i(int *a, int *b) { int t = *a; *a = *b; *b = t; }
static int cmp_int(const void *x, const void *y) { int a = *(const int *)x, b = *(const int *)y; return (a > b) - (a < b); }

/* 3-way partition around a pivot value; returns the [lt, gt] range equal to the pivot. */
static void partition3(int *a, int lo, int hi, int pivot, int *plt, int *pgt) {
    int lt = lo, i = lo, gt = hi;
    while (i <= gt) { if (a[i] < pivot) swap_i(&a[lt++], &a[i++]); else if (a[i] > pivot) swap_i(&a[i], &a[gt--]); else i++; }
    *plt = lt; *pgt = gt;
}

/* Quickselect with a RANDOM pivot: expected O(n), worst case O(n^2) (astronomically unlikely). Returns the k-th smallest (0-based). */
static int quickselect(int *a, int n, int k) {
    int lo = 0, hi = n - 1;
    while (lo < hi) {
        int pivot = a[lo + (int)(rnd() % (uint64_t)(hi - lo + 1))], lt, gt;
        partition3(a, lo, hi, pivot, &lt, &gt);
        if (k < lt) hi = lt - 1; else if (k > gt) lo = gt + 1; else return pivot;
    }
    return a[lo];
}

/* Median of medians (BFPRT): deterministic worst-case O(n).  Pivot = median of the medians of groups of five. */
static int select_mom(int *a, int lo, int hi, int k);
static int median_of_five(int *a, int lo, int hi) {                 /* sort <= 5 elements by insertion; return the index of the median */
    for (int i = lo + 1; i <= hi; i++) { int key = a[i], j = i - 1; while (j >= lo && a[j] > key) { a[j + 1] = a[j]; j--; } a[j + 1] = key; }
    return lo + (hi - lo) / 2;
}
static int select_mom(int *a, int lo, int hi, int k) {              /* k is an absolute index into a[] */
    while (lo < hi) {
        int n = hi - lo + 1, m = 0;
        for (int i = lo; i <= hi; i += 5) {                          /* medians of groups of five, gathered at the front */
            int end = i + 4 < hi ? i + 4 : hi;
            swap_i(&a[lo + m++], &a[median_of_five(a, i, end)]);
        }
        int pivot = n <= 5 ? a[lo + (m - 1) / 2] : select_mom(a, lo, lo + m - 1, lo + (m - 1) / 2);   /* median of the medians (recursive) */
        int lt, gt;
        partition3(a, lo, hi, pivot, &lt, &gt);
        if (k < lt) hi = lt - 1; else if (k > gt) lo = gt + 1; else return pivot;
    }
    return a[lo];
}

/* Top-k smallest with a bounded max-heap: O(n log k) time, O(k) space, works on a stream. */
static void heap_sift_down(int *h, int n, int i) { for (;;) { int c = 2 * i + 1; if (c >= n) break; if (c + 1 < n && h[c + 1] > h[c]) c++; if (h[i] >= h[c]) break; swap_i(&h[i], &h[c]); i = c; } }
static void topk_smallest(const int *a, int n, int k, int *out) {
    int size = 0;
    for (int i = 0; i < n; i++) {
        if (size < k) { out[size++] = a[i]; if (size == k) for (int j = k / 2 - 1; j >= 0; j--) heap_sift_down(out, k, j); }
        else if (a[i] < out[0]) { out[0] = a[i]; heap_sift_down(out, k, 0); }          /* replace the current k-th smallest */
    }
}

int main(void) {
    /* correctness */
    for (int t = 0; t < 500; t++) {
        int n = 1 + (int)(rnd() % 300), a[300], b[300], c[300];
        for (int i = 0; i < n; i++) a[i] = (int)(rnd() % 50);
        memcpy(b, a, n * sizeof *a); memcpy(c, a, n * sizeof *a);
        int k = (int)(rnd() % (uint64_t)n);
        int ref[300]; memcpy(ref, a, n * sizeof *a); qsort(ref, n, sizeof *ref, cmp_int);
        assert(quickselect(b, n, k) == ref[k]);
        assert(select_mom(c, 0, n - 1, k) == ref[k]);
        int kk = 1 + (int)(rnd() % (uint64_t)n), out[300];
        topk_smallest(a, n, kk, out); qsort(out, kk, sizeof *out, cmp_int);
        assert(!memcmp(out, ref, kk * sizeof *out));
    }
    printf("quickselect, median-of-medians and heap top-k agree with sorting on 500 random arrays\n\n");

    size_t n = 5000000; int *base = malloc(n * sizeof *base), *a = malloc(n * sizeof *a);
    for (size_t i = 0; i < n; i++) base[i] = (int)(rnd() >> 33);
    int k = (int)(n / 2);
    memcpy(a, base, n * sizeof *a); double t0 = now(); qsort(a, n, sizeof *a, cmp_int); int m1 = a[k]; double t1 = now();
    memcpy(a, base, n * sizeof *a); double t2 = now(); int m2 = quickselect(a, (int)n, k); double t3 = now();
    memcpy(a, base, n * sizeof *a); double t4 = now(); int m3 = select_mom(a, 0, (int)n - 1, k); double t5 = now();
    printf("median of %zu ints:  sort+index %7.1f ms | quickselect %6.1f ms | median-of-medians %6.1f ms   (all equal: %s)\n", n, (t1 - t0) * 1e3, (t3 - t2) * 1e3, (t5 - t4) * 1e3, m1 == m2 && m2 == m3 ? "yes" : "NO");
    int top[100];
    double t6 = now(); topk_smallest(base, (int)n, 100, top); double t7 = now();
    printf("100 smallest of %zu: heap top-k %6.1f ms (one pass, O(k) memory) vs full sort %7.1f ms\n", n, (t7 - t6) * 1e3, (t1 - t0) * 1e3);
    free(base); free(a);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra select.c -o select && ./select | tail -n 2
median of 5000000 ints:  sort+index  1325.7 ms | quickselect  103.8 ms | median-of-medians  242.7 ms   (all equal: yes)
100 smallest of 5000000: heap top-k    5.5 ms (one pass, O(k) memory) vs full sort  1325.7 ms
```

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
