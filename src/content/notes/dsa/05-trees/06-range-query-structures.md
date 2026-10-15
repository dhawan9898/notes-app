---
title: "Range-Query Structures: Prefix Sums, Fenwick and Segment Trees"
track: dsa
module: Trees
moduleOrder: 5
order: 6
level: advanced
summary: "Answering 'what is the sum or minimum over positions l..r?' fast — prefix sums, difference arrays, Fenwick trees, lazy segment trees, sparse tables and sqrt decomposition, all fuzz-tested against a naive scan and measured."
prerequisites:
  - Binary Trees and Binary Search Trees
  - Recursion, Divide and Conquer, and Loop Invariants
sources:
  - label: "Fenwick — A New Data Structure for Cumulative Frequency Tables (Software, Practice and Experience 24(3), 1994)"
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 14 (Augmenting Data Structures, interval trees and order statistics)"
  - label: "de Berg, Cheong, van Kreveld, Overmars — Computational Geometry, Algorithms and Applications, Ch. 10 (Segment trees)"
  - label: "Bender & Farach-Colton — The LCA Problem Revisited (LATIN 2000)"
  - label: "Bentley — Programming Pearls, 2nd ed., Column 8 (Algorithm Design Techniques; cumulative sums)"
  - label: "cp-algorithms — Fenwick Tree and Segment Tree"
    url: https://cp-algorithms.com/data_structures/fenwick.html
---

Given an array `a[0..n)`, a **range query** asks for an aggregate of `a[l..r)` — a sum, a minimum, a count of values in a range — and, in the dynamic setting, the array also changes between queries. The naive scan costs `O(r−l)` per query. The structures below exploit the aggregate's algebra (sums can be subtracted, minima are idempotent, both are associative) to answer in `O(1)` or `O(log n)`.

The first question is always *what changes*:

| Array | Query | Structure | Update | Query |
|---|---|---|---|---|
| static | sum | prefix sums | – | O(1) |
| static | min/max/gcd (idempotent) | sparse table | – | O(1), O(n log n) space |
| point updates | sum / any invertible op | Fenwick tree | O(log n) | O(log n) |
| point **and range** updates | sum / min / ... | segment tree (lazy) | O(log n) | O(log n) |
| many range adds, one read at the end | final values | difference array | O(1) | O(n) once |

## Prefix sums and difference arrays

Define `P[i] = a[0] + … + a[i−1]`. Then `sum(l, r) = P[r] − P[l]`: one subtraction after an `O(n)` build. The same idea in two dimensions — a *summed-area table*, the basis of integral images in computer vision — answers any rectangle sum in four lookups by inclusion–exclusion. The **difference array** is the inverse: to add `v` to every element of `[l, r)` do `d[l] += v; d[r] -= v`, and materialize the final array once with a running sum. It turns `m` range updates into `O(m + n)` instead of `O(m·n)`, but only when you do not need to read in between.

```c
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* 2D prefix sums (summed-area table) and the difference array: the two offline tricks that need no tree. */
enum { R = 300, C = 400 };
static int grid[R][C];
static long long P[R + 1][C + 1];                          /* P[i][j] = sum of grid[0..i)[0..j) */

static long long rect(int r1, int c1, int r2, int c2) {    /* sum over rows [r1, r2), columns [c1, c2) in four lookups */
    return P[r2][c2] - P[r1][c2] - P[r2][c1] + P[r1][c1];
}

int main(void) {
    srand(7);
    for (int i = 0; i < R; i++) for (int j = 0; j < C; j++) grid[i][j] = rand() % 100 - 20;
    for (int i = 0; i < R; i++) for (int j = 0; j < C; j++) P[i + 1][j + 1] = grid[i][j] + P[i][j + 1] + P[i + 1][j] - P[i][j];   /* inclusion-exclusion */
    for (int t = 0; t < 2000; t++) {
        int r1 = rand() % R, r2 = r1 + 1 + rand() % (R - r1), c1 = rand() % C, c2 = c1 + 1 + rand() % (C - c1); long long want = 0;
        for (int i = r1; i < r2; i++) for (int j = c1; j < c2; j++) want += grid[i][j];
        assert(rect(r1, c1, r2, c2) == want);
    }
    printf("2D prefix sums: 2000 random rectangle sums match brute force (%d x %d grid; each query is 4 lookups)\n", R, C);

    /* difference array: apply many range additions in O(1) each, materialize once with a prefix sum */
    enum { N = 100000, UPD = 50000 };
    static int diff[N + 1], direct[N], via[N];
    for (int u = 0; u < UPD; u++) {
        int l = rand() % N, r = l + 1 + rand() % (N - l), v = rand() % 11 - 5;
        for (int i = l; i < r; i++) direct[i] += v;       /* O(length) */
        diff[l] += v; diff[r] -= v;                       /* O(1) */
    }
    int run = 0; for (int i = 0; i < N; i++) { run += diff[i]; via[i] = run; }
    for (int i = 0; i < N; i++) assert(via[i] == direct[i]);
    printf("difference array: %d range additions applied in O(1) each, final array identical to the O(length) version\n", UPD);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra -fsanitize=address,undefined prefix2d.c -o prefix2d && ./prefix2d
2D prefix sums: 2000 random rectangle sums match brute force (300 x 400 grid; each query is 4 lookups)
difference array: 50000 range additions applied in O(1) each, final array identical to the O(length) version
```

Both are the right answer far more often than any tree: static data, offline queries, histogram counting (cumulative distribution), cumulative packet/byte counters. Reach for something heavier only when the array mutates between queries.

## Fenwick tree (binary indexed tree)

Peter Fenwick's 1994 structure supports point update and prefix sum in `O(log n)` using one array of `n` words and about ten lines of code. The trick is the binary representation of the index. Using 1-based indices, `t[i]` stores the sum of the `lowbit(i)` elements ending at `i`, where `lowbit(i) = i & −i` is the lowest set bit:

```text
index   binary   lowbit   t[i] covers
  1     0001       1      a[1]
  2     0010       2      a[1..2]
  3     0011       1      a[3]
  4     0100       4      a[1..4]
  5     0101       1      a[5]
  6     0110       2      a[5..6]
  7     0111       1      a[7]
  8     1000       8      a[1..8]
```

- **Prefix sum(r)**: start at `r`, add `t[r]`, then strip the lowest set bit (`r −= r & −r`) and repeat — at most `log2 n` terms, because each step removes one bit. For `r = 7` the blocks are `t[7] + t[6] + t[4]` = `a[7] + a[5..6] + a[1..4]`.
- **Update(i, d)**: add `d` to `t[i]`, then move to the next block that covers `i` (`i += i & −i`) — at most `log2 n` blocks.
- **Build in O(n)**: instead of `n` updates (`O(n log n)`), add each `a[i]` to `t[i]` and then push `t[i]` to its parent `i + lowbit(i)`.

Fenwick trees work for any **invertible** aggregate (sum, xor, count). Range minimum is not invertible, which is why it needs a segment tree. Extensions: range-add/range-sum with two trees, a 2D version, and `O(log n)` "find the smallest index whose prefix sum reaches `k`" by binary lifting — which turns a Fenwick tree over counts into an **order-statistic** structure (k-th smallest element, rank of a value).

## Segment tree with lazy propagation

A segment tree is a balanced binary tree where each node stores the aggregate of a contiguous range and the root covers the whole array. A range query decomposes `[l, r)` into at most `2 log n` maximal nodes (at most two per level) and combines them. It works for **any associative** operation — sum, min, max, gcd, matrix product, "number of inversions in a range" — and, with **lazy propagation**, supports updating a whole range in `O(log n)`: when an update fully covers a node, apply it to that node's aggregate and park a pending "tag" there; push the tag down to the children only when a later operation must descend through it.

The implementation below supports `range add` and `range sum`; the lazy tag is the pending addition, and applying it to a node of length `len` adds `tag · len` to the sum.

```c
#ifndef STRUCTS_H
#define STRUCTS_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef long long ll;

/* ============ Prefix sums: static array, O(n) build, O(1) range sum ============ */
static inline ll *prefix_build(const int *a, size_t n) { ll *p = malloc((n + 1) * sizeof *p); p[0] = 0; for (size_t i = 0; i < n; i++) p[i + 1] = p[i] + a[i]; return p; }
static inline ll prefix_sum(const ll *p, size_t l, size_t r) { return p[r] - p[l]; }          /* sum of a[l..r) */

/* ============ Fenwick tree (binary indexed tree): point update + prefix sum, both O(log n), n words ============ */
typedef struct { ll *t; size_t n; } Fenwick;                  /* t[1..n]; t[i] covers the last lowbit(i) elements ending at i */
static inline Fenwick fw_new(size_t n) { Fenwick f = { calloc(n + 1, sizeof(ll)), n }; return f; }
static inline void fw_add(Fenwick *f, size_t i, ll d) { for (i++; i <= f->n; i += i & -i) f->t[i] += d; }    /* a[i] += d */
static inline ll fw_prefix(const Fenwick *f, size_t r) { ll s = 0; for (; r > 0; r -= r & -r) s += f->t[r]; return s; }   /* sum a[0..r) */
static inline ll fw_range(const Fenwick *f, size_t l, size_t r) { return fw_prefix(f, r) - fw_prefix(f, l); }
static inline Fenwick fw_build(const int *a, size_t n) {              /* O(n): push each partial sum to its parent */
    Fenwick f = fw_new(n);
    for (size_t i = 1; i <= n; i++) { f.t[i] += a[i - 1]; size_t j = i + (i & -i); if (j <= n) f.t[j] += f.t[i]; }
    return f;
}

/* ============ Segment tree: range sum AND range add with lazy propagation, bottom-up size 4n recursive ============ */
typedef struct { ll *sum, *lazy; size_t n; } Seg;
static inline void seg_build_rec(Seg *s, size_t x, size_t l, size_t r, const int *a) {
    s->lazy[x] = 0;
    if (r - l == 1) { s->sum[x] = a[l]; return; }
    size_t m = (l + r) / 2; seg_build_rec(s, 2 * x, l, m, a); seg_build_rec(s, 2 * x + 1, m, r, a); s->sum[x] = s->sum[2 * x] + s->sum[2 * x + 1];
}
static inline Seg seg_new(const int *a, size_t n) { Seg s = { malloc(4 * n * sizeof(ll)), malloc(4 * n * sizeof(ll)), n }; seg_build_rec(&s, 1, 0, n, a); return s; }
static inline void seg_apply(Seg *s, size_t x, size_t len, ll v) { s->sum[x] += v * (ll)len; s->lazy[x] += v; }
static inline void seg_push(Seg *s, size_t x, size_t l, size_t r) {               /* hand the pending add down to the two children */
    if (s->lazy[x]) { size_t m = (l + r) / 2; seg_apply(s, 2 * x, m - l, s->lazy[x]); seg_apply(s, 2 * x + 1, r - m, s->lazy[x]); s->lazy[x] = 0; }
}
static inline void seg_add_rec(Seg *s, size_t x, size_t l, size_t r, size_t ql, size_t qr, ll v) {
    if (qr <= l || r <= ql) return;
    if (ql <= l && r <= qr) { seg_apply(s, x, r - l, v); return; }                /* fully covered: stop, remember lazily */
    seg_push(s, x, l, r); size_t m = (l + r) / 2;
    seg_add_rec(s, 2 * x, l, m, ql, qr, v); seg_add_rec(s, 2 * x + 1, m, r, ql, qr, v);
    s->sum[x] = s->sum[2 * x] + s->sum[2 * x + 1];
}
static inline ll seg_sum_rec(Seg *s, size_t x, size_t l, size_t r, size_t ql, size_t qr) {
    if (qr <= l || r <= ql) return 0;
    if (ql <= l && r <= qr) return s->sum[x];
    seg_push(s, x, l, r); size_t m = (l + r) / 2;
    return seg_sum_rec(s, 2 * x, l, m, ql, qr) + seg_sum_rec(s, 2 * x + 1, m, r, ql, qr);
}
static inline void seg_add(Seg *s, size_t l, size_t r, ll v) { seg_add_rec(s, 1, 0, s->n, l, r, v); }     /* a[l..r) += v */
static inline ll seg_sum(Seg *s, size_t l, size_t r) { return seg_sum_rec(s, 1, 0, s->n, l, r); }         /* sum a[l..r) */

/* ============ Sparse table: static range MINIMUM in O(1) after O(n log n) preprocessing (idempotent operation) ============ */
typedef struct { int **t; int *lg; size_t n; int levels; } Sparse;
static inline Sparse sp_build(const int *a, size_t n) {
    Sparse s; s.n = n; s.lg = malloc((n + 1) * sizeof(int)); s.lg[1] = 0; for (size_t i = 2; i <= n; i++) s.lg[i] = s.lg[i / 2] + 1;
    s.levels = s.lg[n] + 1; s.t = malloc((size_t)s.levels * sizeof(int *));
    s.t[0] = malloc(n * sizeof(int)); memcpy(s.t[0], a, n * sizeof(int));
    for (int k = 1; k < s.levels; k++) {
        size_t len = n - ((size_t)1 << k) + 1; s.t[k] = malloc(len * sizeof(int));
        for (size_t i = 0; i < len; i++) { int x = s.t[k - 1][i], y = s.t[k - 1][i + ((size_t)1 << (k - 1))]; s.t[k][i] = x < y ? x : y; }
    }
    return s;
}
static inline int sp_min(const Sparse *s, size_t l, size_t r) {                   /* min a[l..r), two overlapping power-of-two blocks */
    int k = s->lg[r - l]; int x = s->t[k][l], y = s->t[k][r - ((size_t)1 << k)]; return x < y ? x : y;
}
static inline void sp_free(Sparse *s) { for (int k = 0; k < s->levels; k++) free(s->t[k]); free(s->t); free(s->lg); }

/* ============ sqrt decomposition: blocks of size B; point update O(1), range sum O(sqrt n) ============ */
typedef struct { int *a; ll *blk; size_t n, B; } Sqrt;
static inline Sqrt sq_new(const int *a, size_t n) {
    Sqrt q; q.n = n; q.B = 1; while (q.B * q.B < n) q.B++;
    q.a = malloc(n * sizeof(int)); memcpy(q.a, a, n * sizeof(int)); q.blk = calloc(n / q.B + 1, sizeof(ll));
    for (size_t i = 0; i < n; i++) q.blk[i / q.B] += a[i];
    return q;
}
static inline void sq_add(Sqrt *q, size_t i, int d) { q->a[i] += d; q->blk[i / q->B] += d; }
static inline ll sq_sum(const Sqrt *q, size_t l, size_t r) {
    ll s = 0; size_t i = l;
    while (i < r && i % q->B) s += q->a[i++];                                    /* ragged left edge */
    while (i + q->B <= r) { s += q->blk[i / q->B]; i += q->B; }                  /* whole blocks */
    while (i < r) s += q->a[i++];                                               /* ragged right edge */
    return s;
}
#endif
```

Notes on the code: the tree is stored in an array with children at `2x` and `2x+1` (the heap layout from the previous note) and sized `4n` to be safe for arbitrary `n`; `seg_push` is the only place the lazy invariant is enforced — *a node's `sum` is always correct for its own range, its pending tag applies to its children*. Forgetting to push before descending is the classic bug, and the differential test below catches it.

The **sparse table** answers range minimum in `O(1)` for a static array: store `min` of every block of length `2^k` starting at each index, and answer a query with two possibly-overlapping blocks that together cover the range (overlap is harmless because `min` is idempotent). That is why it works for min, max, gcd and bitwise and/or but not for sum. The cost is `O(n log n)` memory. The classic use is **lowest common ancestor**: an Euler tour of the tree turns LCA into a range-minimum over depths (Bender & Farach-Colton reduce the space to `O(n)` with the same `O(1)` query).

**Square-root decomposition** splits the array into blocks of `√n`: a point update touches one element and one block total; a range query sums at most two ragged edges and `√n` whole blocks. It is slower asymptotically but trivially simple, and the same idea underlies **Mo's algorithm** for offline queries that are hard to maintain in a tree.

## Verification

Every structure is checked against a naive array through randomized sequences of point updates, range updates and range queries, on sizes that are not powers of two (the usual place the index arithmetic breaks), including the `O(n)` Fenwick build against `n` individual updates.

```c
#include <assert.h>
#include <stdio.h>
#include "structs.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    /* Randomized differential tests of every structure against a naive array, on sizes that are not powers of two */
    for (size_t n = 1; n <= 300; n += (n < 40 ? 1 : 37)) {
        int *a = malloc(n * sizeof *a), *ref = malloc(n * sizeof *ref);
        for (size_t i = 0; i < n; i++) ref[i] = a[i] = (int)(rnd() % 2001) - 1000;
        ll *p = prefix_build(a, n); Fenwick f = fw_build(a, n); Seg g = seg_new(a, n); Sparse sp = sp_build(a, n); Sqrt q = sq_new(a, n);
        Fenwick f2 = fw_new(n); for (size_t i = 0; i < n; i++) fw_add(&f2, i, a[i]);        /* built by n updates must equal the O(n) build */
        for (size_t i = 0; i < n; i++) assert(f.t[i + 1] == f2.t[i + 1]);
        for (int op = 0; op < 3000; op++) {
            size_t l = rnd() % n, r = l + 1 + rnd() % (n - l); ll want = 0; int mn = ref[l];
            for (size_t i = l; i < r; i++) { want += ref[i]; if (ref[i] < mn) mn = ref[i]; }
            int kind = (int)(rnd() % 4);
            if (kind == 0) {                                                      /* point update on the dynamic structures */
                size_t i = rnd() % n; int d = (int)(rnd() % 201) - 100; ref[i] += d; fw_add(&f, i, d); sq_add(&q, i, d); seg_add(&g, i, i + 1, d);
                free(p); p = prefix_build(ref, n); sp_free(&sp); sp = sp_build(ref, n);
            } else if (kind == 1) {                                               /* range add (lazy segment tree only) */
                int d = (int)(rnd() % 21) - 10; for (size_t i = l; i < r; i++) ref[i] += d;
                seg_add(&g, l, r, d); for (size_t i = l; i < r; i++) { fw_add(&f, i, d); sq_add(&q, i, d); }
                free(p); p = prefix_build(ref, n); sp_free(&sp); sp = sp_build(ref, n);
            } else {
                assert(prefix_sum(p, l, r) == want); assert(fw_range(&f, l, r) == want); assert(seg_sum(&g, l, r) == want);
                assert(sq_sum(&q, l, r) == want); assert(sp_min(&sp, l, r) == mn);
            }
        }
        free(a); free(ref); free(p); free(f.t); free(f2.t); free(g.sum); free(g.lazy); sp_free(&sp); free(q.a); free(q.blk);
    }
    printf("prefix sums, Fenwick, lazy segment tree, sparse table and sqrt decomposition agree with a naive scan on random sizes, updates and queries\n");
    return 0;
}
```

```text
$ gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined test_range.c -o test_range && ./test_range
prefix sums, Fenwick, lazy segment tree, sparse table and sqrt decomposition agree with a naive scan on random sizes, updates and queries
```

## Measured cost

The dynamic test mixes 50 % point updates and 50 % random range-sum queries. The naive version scans; the other three are the Fenwick tree, the lazy segment tree (using its general range-add machinery for point updates), and square-root decomposition. The second experiment is a static range-minimum workload on two million elements.

```c
#define _GNU_SOURCE
#include <stdio.h>
#include "structs.h"
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    printf("dynamic range sum: 50%% point updates, 50%% random range sums; ns per operation\n");
    printf("%9s | %10s %10s %10s %10s | %12s\n", "n", "naive", "Fenwick", "segtree", "sqrt dec.", "memory (F/S)");
    for (size_t n = 10000; n <= 4000000; n *= 20) {
        int *a = malloc(n * sizeof *a); for (size_t i = 0; i < n; i++) a[i] = (int)(rnd() % 1000);
        enum { Q = 400000 }; size_t *L = malloc(Q * sizeof *L), *R = malloc(Q * sizeof *R); for (int i = 0; i < Q; i++) { L[i] = rnd() % n; R[i] = L[i] + 1 + rnd() % (n - L[i]); }
        long sink = 0; double t0;
        int naive_q = (int)(2000000000ull / n / 4); if (naive_q > Q) naive_q = Q; if (naive_q < 200) naive_q = 200;
        int *c = malloc(n * sizeof *c); memcpy(c, a, n * sizeof *c);
        t0 = now(); for (int i = 0; i < naive_q; i++) { if (i & 1) c[L[i]] += 1; else { ll t = 0; for (size_t j = L[i]; j < R[i]; j++) t += c[j]; sink += t; } } double tn = (now() - t0) / naive_q * 1e9;
        Fenwick f = fw_build(a, n); t0 = now(); for (int i = 0; i < Q; i++) { if (i & 1) fw_add(&f, L[i], 1); else sink += fw_range(&f, L[i], R[i]); } double tf = (now() - t0) / Q * 1e9;
        Seg g = seg_new(a, n); t0 = now(); for (int i = 0; i < Q; i++) { if (i & 1) seg_add(&g, L[i], L[i] + 1, 1); else sink += seg_sum(&g, L[i], R[i]); } double ts = (now() - t0) / Q * 1e9;
        Sqrt q = sq_new(a, n); t0 = now(); for (int i = 0; i < Q; i++) { if (i & 1) sq_add(&q, L[i], 1); else sink += sq_sum(&q, L[i], R[i]); } double tq = (now() - t0) / Q * 1e9;
        printf("%9zu | %10.0f %10.0f %10.0f %10.0f | %5.1f/%4.1f MB\n", n, tn, tf, ts, tq, (double)(n + 1) * 8 / 1e6, (double)n * 64 / 1e6);
        if (sink == 42) puts("");
        free(a); free(c); free(L); free(R); free(f.t); free(g.sum); free(g.lazy); free(q.a); free(q.blk);
    }

    printf("\nstatic range minimum: n = 2,000,000, 4,000,000 random queries\n");
    size_t n = 2000000; int *a = malloc(n * sizeof *a); for (size_t i = 0; i < n; i++) a[i] = (int)rnd();
    enum { Q = 4000000 }; long sink = 0; double t0;
    t0 = now(); Sparse sp = sp_build(a, n); double build = now() - t0;
    size_t *L = malloc(Q * sizeof *L), *R = malloc(Q * sizeof *R); for (int i = 0; i < Q; i++) { L[i] = rnd() % n; R[i] = L[i] + 1 + rnd() % (n - L[i]); }
    t0 = now(); for (int i = 0; i < Q; i++) sink += sp_min(&sp, L[i], R[i]); double tq = (now() - t0) / Q * 1e9;
    int nq = 2000; t0 = now(); for (int i = 0; i < nq; i++) { int m = a[L[i]]; for (size_t j = L[i]; j < R[i]; j++) if (a[j] < m) m = a[j]; sink += m; } double tn = (now() - t0) / nq * 1e9;
    printf("sparse table: build %.0f ms, %d levels, %.0f MB; query %.1f ns.   naive scan: %.0f ns per query (%.0fx slower)\n",
           build * 1e3, sp.levels, (double)n * (double)sp.levels * 4 / 1e6, tq, tn, tn / tq);
    if (sink == 42) puts("");
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra bench_range.c -o bench_range && ./bench_range
dynamic range sum: 50% point updates, 50% random range sums; ns per operation
        n |      naive    Fenwick    segtree  sqrt dec. | memory (F/S)
    10000 |       1162         34        317        374 |   0.1/ 0.6 MB
   200000 |      22450         54        440       1789 |   1.6/12.8 MB
  4000000 |     443735        398        720       6845 |  32.0/256.0 MB

static range minimum: n = 2,000,000, 4,000,000 random queries
sparse table: build 64 ms, 21 levels, 168 MB; query 37.1 ns.   naive scan: 423871 ns per query (11429x slower)
```

What the numbers show:

- The **naive scan grows linearly**: from about a microsecond at 10,000 elements to hundreds of microseconds at four million — about a thousand times slower than the Fenwick tree there.
- **Fenwick is the fastest dynamic structure by a wide margin** — a handful of memory reads on one dense array of `n` words — and the segment tree costs roughly 2–9× more per operation (the gap is widest while everything fits in cache) and 8× more memory (`4n` nodes × 2 words) for the extra power. Use the Fenwick tree whenever the operation is invertible and updates are points.
- **Square-root decomposition degrades as `√n`** — competitive for small arrays, 10× behind at four million elements.
- The **sparse table** answers a range minimum in tens of nanoseconds — about ten thousand times faster than scanning — but uses `n log n` words (about 170 MB for two million 32-bit values here) and takes tens of milliseconds to build. If the array changes, it must be rebuilt.

## Other range-structure ideas

- **Interval trees** (augmented balanced BST, CLRS Ch. 14): each node stores the maximum endpoint in its subtree; "find an interval overlapping a query" is `O(log n)`. The Linux kernel uses an augmented red-black tree as `interval_tree` for memory-mapping and driver bookkeeping.
- **Merge-sort tree / wavelet tree**: count elements in `a[l..r)` that lie in `[x, y]`, in `O(log² n)` or `O(log n)`.
- **Persistent segment trees**: path copying gives every past version at `O(log n)` extra space per update, enabling "k-th smallest in a range" queries.
- **Order-statistic tree**: a balanced BST with subtree sizes supports `rank` and `select` in `O(log n)`.
- **Network and systems uses**: Fenwick trees for per-flow cumulative counters and weighted random sampling by priority; segment trees for time-window aggregation; difference arrays for batch-applying ACL range rules; 2D prefix sums for image processing.

## Pitfalls

- **0-based vs 1-based indices.** A Fenwick tree must be 1-based internally (index 0 has `lowbit` 0 and loops forever); expose a 0-based API and convert once.
- **Half-open vs closed ranges.** Mixing `[l, r)` and `[l, r]` produces off-by-one answers that look right on small tests. Use half-open throughout, as the code does.
- **Overflow.** Sums of `n` 32-bit values overflow 32 bits for `n` above about 2 000; accumulate in 64-bit (`long long` here).
- **Lazy-propagation bugs**: forgetting to push before recursing, using the wrong multiplier (`tag × length`), or composing non-commutative tags in the wrong order (assignment after add).
- **Memory for sparse tables** at large `n`: `n log n` words adds up; consider the block-decomposition (`O(n)` space) variants.
- **Assuming a tree is needed.** If queries come after all updates, prefix sums or sorting offline solves it with less code and better constants.

## Key takeaways

1. First classify the problem: static or dynamic, point or range updates, invertible or idempotent aggregate. That picks the structure.
2. Prefix sums (O(1) query) and difference arrays (O(1) range update) handle all the offline cases; 2D prefix sums answer rectangle sums in four lookups.
3. A Fenwick tree needs `n` words and a dozen lines for point-update/prefix-query in `O(log n)`, and was 2–9× faster than a segment tree in these measurements.
4. A segment tree handles any associative operation and range updates through lazy propagation; the lazy invariant is the thing to get right.
5. A sparse table gives O(1) range-minimum on static data for `O(n log n)` memory, and is the building block for LCA.
6. Always fuzz against a naive scan, using odd sizes: off-by-one and propagation bugs only show up there.
