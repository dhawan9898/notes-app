---
title: "Range-Query Structures: Prefix Sums, Fenwick and Segment Trees"
track: dsa
module: Trees
moduleOrder: 5
order: 6
level: advanced
summary: "Answering 'what is the sum or minimum over positions l..r?' fast — prefix sums, difference arrays, Fenwick trees, lazy segment trees, sparse tables and sqrt decomposition, all fuzz-tested against a naive scan and measured."
sources:
  - label: Fenwick — A New Data Structure for Cumulative Frequency Tables (Software, Practice and Experience 24(3), 1994)
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 14 (Augmenting Data Structures, interval trees and order statistics)
  - label: de Berg, Cheong, van Kreveld, Overmars — Computational Geometry, Algorithms and Applications, Ch. 10 (Segment trees)
  - label: Bender & Farach-Colton — The LCA Problem Revisited (LATIN 2000)
  - label: Bentley — Programming Pearls, 2nd ed., Column 8 (Algorithm Design Techniques; cumulative sums)
  - label: cp-algorithms — Fenwick Tree and Segment Tree
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

{{include dsa/range/prefix2d.c}}

{{run dsa/range :: gcc -std=gnu11 -O2 -Wall -Wextra -fsanitize=address,undefined prefix2d.c -o prefix2d && ./prefix2d}}

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

{{include dsa/range/structs.h}}

Notes on the code: the tree is stored in an array with children at `2x` and `2x+1` (the heap layout from the previous note) and sized `4n` to be safe for arbitrary `n`; `seg_push` is the only place the lazy invariant is enforced — *a node's `sum` is always correct for its own range, its pending tag applies to its children*. Forgetting to push before descending is the classic bug, and the differential test below catches it.

The **sparse table** answers range minimum in `O(1)` for a static array: store `min` of every block of length `2^k` starting at each index, and answer a query with two possibly-overlapping blocks that together cover the range (overlap is harmless because `min` is idempotent). That is why it works for min, max, gcd and bitwise and/or but not for sum. The cost is `O(n log n)` memory. The classic use is **lowest common ancestor**: an Euler tour of the tree turns LCA into a range-minimum over depths (Bender & Farach-Colton reduce the space to `O(n)` with the same `O(1)` query).

**Square-root decomposition** splits the array into blocks of `√n`: a point update touches one element and one block total; a range query sums at most two ragged edges and `√n` whole blocks. It is slower asymptotically but trivially simple, and the same idea underlies **Mo's algorithm** for offline queries that are hard to maintain in a tree.

## Verification

Every structure is checked against a naive array through randomized sequences of point updates, range updates and range queries, on sizes that are not powers of two (the usual place the index arithmetic breaks), including the `O(n)` Fenwick build against `n` individual updates.

{{include dsa/range/test_range.c}}

{{run dsa/range :: gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined test_range.c -o test_range && ./test_range}}

## Measured cost

The dynamic test mixes 50 % point updates and 50 % random range-sum queries. The naive version scans; the other three are the Fenwick tree, the lazy segment tree (using its general range-add machinery for point updates), and square-root decomposition. The second experiment is a static range-minimum workload on two million elements.

{{include dsa/range/bench_range.c}}

{{run dsa/range :: gcc -std=gnu11 -O2 -Wall -Wextra bench_range.c -o bench_range && ./bench_range}}

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
