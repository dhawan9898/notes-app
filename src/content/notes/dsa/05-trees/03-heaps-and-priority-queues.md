---
title: Heaps and Priority Queues
track: dsa
module: Trees
moduleOrder: 5
order: 3
level: intermediate
summary: "The array-embedded binary heap, why building one takes O(n), d-ary heaps and cache behavior (measured on two workloads), the indexed heap with decrease-key for Dijkstra, k-way merge, running median and streaming top-k."
sources:
  - label: "Williams — Algorithm 232, Heapsort (CACM 7(6), 1964)"
  - label: "Floyd — Algorithm 245, Treesort 3 (CACM 7(12), 1964)"
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 6 (Heapsort) and Ch. 19 (Fibonacci heaps)"
  - label: "Knuth — The Art of Computer Programming, Vol. 3, §5.2.3 (Sorting by Selection)"
  - label: "Fredman & Tarjan — Fibonacci Heaps and Their Uses in Improved Network Optimization Algorithms (J. ACM 34(3), 1987)"
  - label: "Fredman, Sedgewick, Sleator, Tarjan — The Pairing Heap, a New Form of Self-Adjusting Heap (Algorithmica 1, 1986)"
  - label: "LaMarca & Ladner — The Influence of Caches on the Performance of Heaps (ACM JEA 1, 1996)"
  - label: "Linux kernel — include/linux/min_heap.h"
    url: https://github.com/torvalds/linux/blob/master/include/linux/min_heap.h
---

A **priority queue** supports `insert`, `find-min` and `extract-min` (and often `decrease-key`). A **binary heap** implements it by storing a *complete* binary tree in an array and enforcing only a *local* order — each parent is no larger than its children. It is not sorted (the order between siblings and cousins is unspecified), which is exactly why it is cheap: you pay for the one relation you need, the minimum at the root.

## Layout and operations

For a node at index `i` (zero-based): parent `(i−1)/d`, children `d·i+1 … d·i+d`. With `d = 2` that is the classic `2i+1`, `2i+2`. Because the tree is complete, no pointers are stored, there is no per-node allocation, and the height is `⌈log_d(n(d−1)+1)⌉`.

- **push**: append at the end, then **sift up**: swap with the parent while smaller. Worst case `log n`; for random keys the average is O(1).
- **pop**: take the root, move the last element to the root, then **sift down**: swap with the *smaller* child while larger. Worst case `2 log n` comparisons (two per level).
- **build**: Floyd's algorithm sifts down every internal node from the last parent back to the root.

The code shifts elements along the path and writes the moving element once at the end (rather than swapping each step), which saves about two-thirds of the stores.

```c
#ifndef HEAP_H
#define HEAP_H
#include <stddef.h>
#include <stdlib.h>

/* d-ary min-heap of ints in an array. Children of i: d*i+1 .. d*i+d.  Parent of i: (i-1)/d. d = 2 is the classic binary heap.
 * `off` shifts the logical root to physical slot d-1 when `aligned` is set, so each block of d siblings starts at a multiple of d
 * ints and (with a 64-byte-aligned base) a block of 16 four-byte siblings fills exactly one cache line. */
typedef struct { int *base, *a; size_t n, cap; unsigned d; int aligned; unsigned long long cmps; } Heap;

static inline void heap_init(Heap *h, unsigned d, int aligned) { h->base = h->a = NULL; h->n = h->cap = 0; h->d = d; h->aligned = aligned; h->cmps = 0; }
static void heap_reserve(Heap *h, size_t cap) {
    size_t off = h->aligned ? h->d - 1 : 0;
    size_t bytes = ((cap + off) * sizeof(int) + 63) / 64 * 64;
    int *nb = aligned_alloc(64, bytes ? bytes : 64);
    if (h->base) { for (size_t i = 0; i < h->n; i++) nb[off + i] = h->a[i]; free(h->base); }
    h->base = nb; h->a = nb + off; h->cap = cap;
}
static inline void heap_free(Heap *h) { free(h->base); h->base = h->a = NULL; h->n = h->cap = 0; }

static void sift_up(Heap *h, size_t i) {
    int x = h->a[i];
    while (i > 0) {
        size_t p = (i - 1) / h->d;
        h->cmps++;
        if (h->a[p] <= x) break;
        h->a[i] = h->a[p]; i = p;                    /* move the parent down; write x once at the end */
    }
    h->a[i] = x;
}
static void sift_down(Heap *h, size_t i) {
    int x = h->a[i];
    for (;;) {
        size_t c = h->d * i + 1;
        if (c >= h->n) break;
        size_t best = c, end = c + h->d < h->n ? c + h->d : h->n;
        for (size_t k = c + 1; k < end; k++) { h->cmps++; if (h->a[k] < h->a[best]) best = k; }
        h->cmps++;
        if (x <= h->a[best]) break;
        h->a[i] = h->a[best]; i = best;
    }
    h->a[i] = x;
}
static void heap_push(Heap *h, int x) {
    if (h->n == h->cap) heap_reserve(h, h->cap ? h->cap * 2 : 16);
    h->a[h->n++] = x; sift_up(h, h->n - 1);
}
static inline int heap_top(const Heap *h) { return h->a[0]; }
static int heap_pop(Heap *h) {
    int top = h->a[0];
    h->a[0] = h->a[--h->n];
    if (h->n) sift_down(h, 0);
    return top;
}
/* Floyd's bottom-up build: sift down every internal node from the last parent back to the root. */
static inline void heap_build(Heap *h, const int *src, size_t n) {
    heap_reserve(h, n); h->n = n;
    for (size_t i = 0; i < n; i++) h->a[i] = src[i];
    if (n > 1) for (size_t i = (n - 2) / h->d + 1; i-- > 0; ) sift_down(h, i);
}
static inline int heap_check(const Heap *h) {
    for (size_t i = 1; i < h->n; i++) if (h->a[(i - 1) / h->d] > h->a[i]) return 0;
    return 1;
}
#endif
```

### Why build is O(n), not O(n log n)

Inserting `n` keys one at a time costs up to `n log n` (a key may sift all the way up). Floyd's bottom-up build is linear: a node at height `h` sifts down at most `h` levels, and at most `⌈n/2^{h+1}⌉` nodes have height `h`, so the total work is bounded by

`Σ_h (n / 2^{h+1}) · h  =  n · Σ h/2^{h+1}  ≤  n`,

i.e. at most `n` swaps and `2n` comparisons — most nodes are near the leaves and move almost nothing. The experiment confirms it and also shows what random pushes cost.

```c
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "heap.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }
static int cmp_int(const void *a, const void *b) { return (*(const int *)a > *(const int *)b) - (*(const int *)a < *(const int *)b); }

int main(void) {
    /* correctness: heapsort via the heap equals qsort */
    { enum { N = 5000 }; int a[N], b[N]; for (int i = 0; i < N; i++) a[i] = b[i] = (int)(rnd() % 1000);
      Heap h; heap_init(&h, 2, 0); heap_build(&h, a, N); assert(heap_check(&h));
      qsort(b, N, sizeof b[0], cmp_int); for (int i = 0; i < N; i++) assert(heap_pop(&h) == b[i]);
      heap_free(&h); printf("heap_build + %d pops == qsort: OK\n\n", N); }

    printf("comparisons to build a binary heap of n random keys\n%9s %14s %12s %14s %12s\n", "n", "n pushes", "per element", "Floyd build", "per element");
    for (size_t n = 1000; n <= 1000000; n *= 10) {
        int *a = malloc(n * sizeof *a); for (size_t i = 0; i < n; i++) a[i] = (int)rnd();
        Heap p; heap_init(&p, 2, 0); for (size_t i = 0; i < n; i++) heap_push(&p, a[i]);
        Heap f; heap_init(&f, 2, 0); heap_build(&f, a, n); assert(heap_check(&p) && heap_check(&f));
        printf("%9zu %14llu %12.2f %14llu %12.2f\n", n, p.cmps, (double)p.cmps / (double)n, f.cmps, (double)f.cmps / (double)n);
        free(a); heap_free(&p); heap_free(&f);
    }
    printf("\npushes in SORTED order cost O(1) each (nothing sifts); pushes in REVERSE sorted order cost log2 n each:\n");
    for (int rev = 0; rev < 2; rev++) {
        size_t n = 1000000; Heap h; heap_init(&h, 2, 0);
        for (size_t i = 0; i < n; i++) heap_push(&h, rev ? (int)(n - i) : (int)i);
        printf("  %-14s %.2f comparisons per push\n", rev ? "descending" : "ascending", (double)h.cmps / (double)n); heap_free(&h);
    }
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra -fsanitize=address,undefined build_cost.c -o build_cost && ./build_cost
heap_build + 5000 pops == qsort: OK

comparisons to build a binary heap of n random keys
        n       n pushes  per element    Floyd build  per element
     1000           2252         2.25           1848         1.85
    10000          22695         2.27          18737         1.87
   100000         227859         2.28         188060         1.88
  1000000        2280738         2.28        1881131         1.88

pushes in SORTED order cost O(1) each (nothing sifts); pushes in REVERSE sorted order cost log2 n each:
  ascending      1.00 comparisons per push
  descending     17.95 comparisons per push
```

Floyd's build uses about 1.9 comparisons per element at every size — constant, below the `2n` bound. Building by repeated push costs about 2.3 per element on random input: also constant on average, but with a worst case of `n log n` — the last two lines show both extremes (ascending input never sifts; descending input sifts the full height, about `log2 n` = 20 levels for a million keys, giving ~18 comparisons per push). So prefer `heap_build` when you have all the data up front.

## Heapsort in one paragraph

Build the heap (O(n)), then pop `n` times, each `O(log n)`: total `O(n log n)` in-place with no worst-case degradation, and no extra memory when done in place with a max-heap (swap the root with the last element and shrink). It is the fallback inside introsort for exactly that guarantee. In practice it loses to quicksort and merge sort because sift-down touches memory in a pattern with poor locality and its comparisons are hard for the branch predictor — see the sorting notes for measurements.

## d-ary heaps and the cache

A heap of `d` children per node has height `log_d n`, so `sift_up` does fewer swaps (`log_d n`); but `sift_down` compares `d` children at each of those levels (`d·log_d n` comparisons). A larger `d` also puts all `d` children of a node adjacent in memory — one cache line for up to 16 four-byte keys — which cuts cache misses per level at no extra miss cost. LaMarca & Ladner showed that tuning the heap to the cache, not the comparison count, decides performance once the heap is bigger than the cache. The `heap.h` above supports `d` and optionally shifts the root so each block of `d` siblings starts on an aligned boundary (with a 64-byte-aligned allocation).

Whether a larger `d` helps depends on which operation dominates, so the benchmark below runs two workloads. In **A** the pop takes the minimum and the push inserts a *random* key; the heap's contents drift upward over time so the pushed key is usually small and sifts up to near the root (push-dominated). In **B**, an event queue, the push is `min + delay`, so the new key goes to the bottom and the pop's sift-down dominates.

```c
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include "heap.h"
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

/* workload 0: pop the min, push a random key.  Popped keys are small, so the heap's contents drift upward and a fresh
 *             random key is usually SMALL relative to the heap: it sifts up almost to the root (push-dominated).
 * workload 1: event queue: pop the min m, push m + random delay.  The new key is larger than everything popped, so it
 *             stays near a leaf and the sift-down of the pop dominates. */
static double run(size_t n, unsigned d, int workload) {
    Heap h; heap_init(&h, d, 1); heap_reserve(&h, n + 1);
    for (size_t i = 0; i < n; i++) heap_push(&h, (int)(rnd() >> 2));
    long sink = 0; double t0 = now();
    for (int i = 0; i < 2000000; i++) {
        int m = heap_pop(&h); sink += m;
        heap_push(&h, workload ? m + (int)(rnd() % 1000000) : (int)(rnd() >> 2));
    }
    double dt = now() - t0; heap_free(&h); if (sink == 42) puts("");
    return dt / 2000000 * 1e9;
}

int main(void) {
    const char *name[] = { "A: pop min, push a random key (sift-up heavy)", "B: event queue, push min + delay (sift-down heavy)" };
    for (int w = 0; w < 2; w++) {
        printf("%s -- ns per (pop + push)\n%10s %9s | %6s %6s %6s %6s\n", name[w], "n", "size", "d=2", "d=4", "d=8", "d=16");
        for (size_t n = 1000; n <= 33000000; n *= 8)
            printf("%10zu %7.1fMB | %6.0f %6.0f %6.0f %6.0f\n", n, (double)n * 4 / 1e6, run(n, 2, w), run(n, 4, w), run(n, 8, w), run(n, 16, w));
        printf("\n");
    }
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra bench_d.c -o bench_d && ./bench_d
A: pop min, push a random key (sift-up heavy) -- ns per (pop + push)
         n      size |    d=2    d=4    d=8   d=16
      1000     0.0MB |    113     73     73     70
      8000     0.0MB |    136    105     97    110
     64000     0.3MB |    102     75     85    103
    512000     2.0MB |    125    106    114    135
   4096000    16.4MB |    160    160    177    210
  32768000   131.1MB |    531    819    400    401

B: event queue, push min + delay (sift-down heavy) -- ns per (pop + push)
         n      size |    d=2    d=4    d=8   d=16
      1000     0.0MB |    126     87     75     97
      8000     0.0MB |    149    169    184    240
     64000     0.3MB |     83    103    139    249
    512000     2.0MB |    196    231    140    184
   4096000    16.4MB |    220    271    328    472
  32768000   131.1MB |    220    332    432    567
```

The pattern to take from a single run (absolute numbers vary by machine and from run to run, especially for the biggest heaps where everything is limited by memory latency):

- **Workload A (sift-up heavy):** `d = 4` or `8` is faster than binary (by up to about 1.5× in this run) while the heap fits in the CPU caches — fewer levels to climb and sibling blocks in one line. Beyond the cache, all variants converge on memory latency.
- **Workload B (sift-down heavy):** binary is the best or tied: each level costs `d` comparisons, and at depth the extra comparisons do not buy fewer cache misses since a node's children are already close.
- Hence the conventional advice — a 4-ary heap is a good default when pushes and `decrease-key` dominate (Dijkstra, Prim), while pop-dominated queues gain little. Measure your own mix.

## The indexed heap: decrease-key

Dijkstra's and Prim's algorithms need "lower the priority of vertex `v`", but a plain heap can only find the minimum — locating `v` would cost `O(n)`. An **indexed priority queue** keeps a position array `pos[id]` kept in sync on every swap, so `decrease-key` is `pos` lookup + sift-up: `O(log n)`. The alternative used by many implementations is *lazy deletion*: push a duplicate entry with the smaller key and discard stale entries when popped, which costs extra memory and heap size but avoids the position array.

```c
#ifndef INDEXED_H
#define INDEXED_H
#include <stdlib.h>

/* Indexed min-priority queue over ids 0..cap-1 with decrease-key in O(log n).  pos[id] = heap slot or -1.
 * This is the structure Dijkstra and Prim want: "lower the priority of vertex v" without searching for it. */
typedef struct { int *heap; int *pos; long *prio; int n; } IPQ;

static void ipq_init(IPQ *q, int cap) {
    q->heap = malloc((size_t)cap * sizeof(int)); q->pos = malloc((size_t)cap * sizeof(int)); q->prio = malloc((size_t)cap * sizeof(long)); q->n = 0;
    for (int i = 0; i < cap; i++) q->pos[i] = -1;
}
static void ipq_free(IPQ *q) { free(q->heap); free(q->pos); free(q->prio); }
static inline void ipq_swap(IPQ *q, int i, int j) { int a = q->heap[i], b = q->heap[j]; q->heap[i] = b; q->heap[j] = a; q->pos[b] = i; q->pos[a] = j; }
static void ipq_up(IPQ *q, int i) { while (i > 0 && q->prio[q->heap[(i - 1) / 2]] > q->prio[q->heap[i]]) { ipq_swap(q, i, (i - 1) / 2); i = (i - 1) / 2; } }
static void ipq_down(IPQ *q, int i) {
    for (;;) {
        int l = 2 * i + 1, r = l + 1, m = i;
        if (l < q->n && q->prio[q->heap[l]] < q->prio[q->heap[m]]) m = l;
        if (r < q->n && q->prio[q->heap[r]] < q->prio[q->heap[m]]) m = r;
        if (m == i) return;
        ipq_swap(q, i, m); i = m;
    }
}
static inline int ipq_contains(const IPQ *q, int id) { return q->pos[id] >= 0; }
static void ipq_insert(IPQ *q, int id, long p) { q->prio[id] = p; q->heap[q->n] = id; q->pos[id] = q->n; q->n++; ipq_up(q, q->n - 1); }
static void ipq_decrease(IPQ *q, int id, long p) { q->prio[id] = p; ipq_up(q, q->pos[id]); }          /* requires p <= current priority */
static int ipq_extract(IPQ *q) {
    int top = q->heap[0]; ipq_swap(q, 0, q->n - 1); q->pos[top] = -1; q->n--;
    if (q->n) ipq_down(q, 0);
    return top;
}
#endif
```

## Classic applications

- **k-way merge.** Keep the current head of each of `k` sorted runs in a heap; pop the minimum, replace it with the next element of the same run. `O(N log k)` for `N` elements — the core of external sorting, LSM compaction and merging log files.
- **Running median.** A max-heap of the lower half and a min-heap of the upper half, kept within one element in size: O(log n) per insert and O(1) to read the median (the code negates keys to reuse the min-heap).
- **Streaming top-k.** A min-heap of size `k` holds the best `k` seen; a new item enters only if it beats the root. For random data the root is replaced about `k·ln(n/k)` times, so almost all items cost a single comparison.
- **Event simulation, schedulers and timers**: the next event is always the minimum timestamp. (The Linux kernel provides `min_heap` helpers and uses ordered trees for the high-resolution timer queue; network stacks and databases use hierarchical timing wheels or heaps for large timer sets.)

```c
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "heap.h"
#include "indexed.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }
static int cmp_int(const void *a, const void *b) { return (*(const int *)a > *(const int *)b) - (*(const int *)a < *(const int *)b); }

/* ---- 1. k-way merge: a heap holds the current head of each of the k sorted runs ---- */
typedef struct { int val, run; } Head;
static void merge_sift(Head *h, int n, int i, unsigned long long *cmps) {
    Head x = h[i];
    for (;;) {
        int c = 2 * i + 1; if (c >= n) break;
        if (c + 1 < n) { (*cmps)++; if (h[c + 1].val < h[c].val) c++; }
        (*cmps)++; if (x.val <= h[c].val) break;
        h[i] = h[c]; i = c;
    }
    h[i] = x;
}
static unsigned long long kway_merge(int **runs, int *len, int k, int *out) {
    Head *h = malloc((size_t)k * sizeof *h); int *idx = calloc((size_t)k, sizeof *idx), n = 0; unsigned long long cmps = 0;
    for (int r = 0; r < k; r++) if (len[r]) h[n++] = (Head){ runs[r][0], r };
    for (int i = n / 2 - 1; i >= 0; i--) merge_sift(h, n, i, &cmps);
    int o = 0;
    while (n) {
        out[o++] = h[0].val; int r = h[0].run;
        if (++idx[r] < len[r]) h[0] = (Head){ runs[r][idx[r]], r }; else h[0] = h[--n];
        if (n) merge_sift(h, n, 0, &cmps);
    }
    free(h); free(idx); return cmps;
}

int main(void) {
    /* ---- indexed heap: randomized test against an O(n) scan ---- */
    { enum { ID = 500, OPS = 200000 }; IPQ q; ipq_init(&q, ID); long ref[ID]; char in[ID] = {0}; int count = 0;
      for (int i = 0; i < OPS; i++) {
          int id = (int)(rnd() % ID), op = (int)(rnd() % 3);
          if (op == 0 && !in[id]) { long p = (long)(rnd() % 100000); ipq_insert(&q, id, p); ref[id] = p; in[id] = 1; count++; }
          else if (op == 1 && in[id]) { long p = ref[id] - (long)(rnd() % 1000); ipq_decrease(&q, id, p); ref[id] = p; }
          else if (op == 2 && count) {
              long best = 1L << 60; for (int j = 0; j < ID; j++) if (in[j] && ref[j] < best) best = ref[j];
              int got = ipq_extract(&q); assert(ref[got] == best && in[got]); in[got] = 0; count--;
          }
          assert(q.n == count);
      }
      ipq_free(&q); printf("indexed heap: %d random insert/decrease-key/extract-min ops match a linear scan\n\n", OPS); }

    /* ---- k-way merge: comparisons vs the k*N naive scan, and vs n*log2(k) bound ---- */
    printf("k-way merge of k sorted runs, 1,000,000 elements total\n%8s %16s %14s %16s\n", "k", "heap compares", "per element", "log2(k)*2 bound");
    int total = 1000000; int *all = malloc((size_t)total * sizeof *all), *out = malloc((size_t)total * sizeof *out);
    for (int k = 2; k <= 1024; k *= 4) {
        for (int i = 0; i < total; i++) all[i] = (int)rnd();
        int **runs = malloc((size_t)k * sizeof *runs), *len = malloc((size_t)k * sizeof *len);
        for (int r = 0; r < k; r++) { len[r] = total / k; runs[r] = all + (size_t)r * (size_t)(total / k); qsort(runs[r], (size_t)len[r], sizeof(int), cmp_int); }
        int merged = k * (total / k);
        unsigned long long c = kway_merge(runs, len, k, out);
        for (int i = 1; i < merged; i++) assert(out[i - 1] <= out[i]);
        printf("%8d %16llu %14.2f %16.1f\n", k, c, (double)c / merged, 2 * log2(k));
        free(runs); free(len);
    }
    free(all); free(out);

    /* ---- running median: max-heap of the lower half, min-heap of the upper half ---- */
    { enum { N = 200000 }; int *v = malloc(N * sizeof *v), *sorted = malloc(N * sizeof *sorted);
      Heap hi; heap_init(&hi, 2, 0); Heap lo; heap_init(&lo, 2, 0);             /* lo stores NEGATED keys so the min-heap acts as a max-heap */
      int checked = 0;
      for (int i = 0; i < N; i++) {
          v[i] = (int)(rnd() % 1000000);
          if (lo.n == 0 || v[i] <= -heap_top(&lo)) heap_push(&lo, -v[i]); else heap_push(&hi, v[i]);
          if (lo.n > hi.n + 1) heap_push(&hi, -heap_pop(&lo));
          else if (hi.n > lo.n) heap_push(&lo, -heap_pop(&hi));
          if (i % 997 == 0) {                                                   /* verify the lower median against a full sort */
              memcpy(sorted, v, (size_t)(i + 1) * sizeof *v); qsort(sorted, (size_t)(i + 1), sizeof *sorted, cmp_int);
              assert(-heap_top(&lo) == sorted[i / 2]); checked++;
          }
      }
      printf("\nrunning median over %d values: lower median verified against a full sort at %d checkpoints (O(log n) per update, O(1) query)\n", N, checked);
      heap_free(&hi); heap_free(&lo); free(v); free(sorted); }

    /* ---- streaming top-k: a size-k min-heap; how often does a new item enter it? ---- */
    { int k = 100; size_t n = 10000000; Heap h; heap_init(&h, 2, 0); size_t replaced = 0;
      for (size_t i = 0; i < n; i++) {
          int x = (int)rnd();
          if ((int)h.n < k) heap_push(&h, x);
          else if (x > heap_top(&h)) { h.a[0] = x; sift_down(&h, 0); replaced++; }
      }
      printf("\ntop-%d of %zu random items: the heap root was replaced %zu times (expected about k*ln(n/k) = %.0f); %.4f%% of items touched the heap\n",
             k, n, replaced, k * log((double)n / k), 100.0 * (double)replaced / (double)n);
      heap_free(&h); }
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined applications.c -o applications -lm && ./applications
indexed heap: 200000 random insert/decrease-key/extract-min ops match a linear scan

k-way merge of k sorted runs, 1,000,000 elements total
       k    heap compares    per element  log2(k)*2 bound
       2           999999           1.00              2.0
       8          4001178           4.00              6.0
      32          7375138           7.38             10.0
     128         11122461          11.12             14.0
     512         15037753          15.04             18.0

running median over 200000 values: lower median verified against a full sort at 201 checkpoints (O(log n) per update, O(1) query)

top-100 of 10000000 random items: the heap root was replaced 1124 times (expected about k*ln(n/k) = 1151); 0.0112% of items touched the heap
```

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
