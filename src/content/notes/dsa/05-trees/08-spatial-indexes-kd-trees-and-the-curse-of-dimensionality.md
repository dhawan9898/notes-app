---
title: "Spatial Indexes: k-d Trees, Grids, Quadtrees, R-trees and the Curse of Dimensionality"
track: dsa
module: Trees
moduleOrder: 5
order: 8
level: advanced
summary: "Nearest-neighbour and range queries without scanning everything: a k-d tree built with quickselect and verified against brute force in 2, 3 and 8 dimensions, a measurement of how its advantage collapses by 16 dimensions, and when to use grids, quadtrees, R-trees, geohashes and approximate methods instead."
prerequisites:
  - Binary Trees and Binary Search Trees
  - Non-Comparison Sorts and Selection
sources:
  - label: "Bentley — Multidimensional binary search trees used for associative searching (CACM 18(9), 1975)"
    url: https://dl.acm.org/doi/10.1145/361002.361007
  - label: "Friedman, Bentley, Finkel — An Algorithm for Finding Best Matches in Logarithmic Expected Time (ACM TOMS 1977)"
    url: https://dl.acm.org/doi/10.1145/355744.355745
  - label: "Guttman — R-trees: a dynamic index structure for spatial searching (SIGMOD 1984)"
    url: https://dl.acm.org/doi/10.1145/602259.602266
  - label: "Finkel & Bentley — Quad trees: a data structure for retrieval on composite keys (Acta Informatica 4, 1974)"
  - label: "Beyer, Goldstein, Ramakrishnan, Shaft — When Is 'Nearest Neighbor' Meaningful? (ICDT 1999)"
    url: https://link.springer.com/chapter/10.1007/3-540-49257-7_15
  - label: "Malkov & Yashunin — Efficient and robust approximate nearest neighbor search using Hierarchical Navigable Small World graphs (IEEE TPAMI 2020)"
    url: https://arxiv.org/abs/1603.09320
  - label: "Beckmann, Kriegel, Schneider, Seeger — The R*-tree: an efficient and robust access method for points and rectangles (SIGMOD 1990)"
    url: https://dl.acm.org/doi/10.1145/93597.98741
---

A **spatial query** asks about geometry: which stored point is nearest to this location? which points lie in this rectangle? which of these polygons overlap the viewport? A linear scan answers each in O(n). If the data is fixed (or changes slowly) and queries are many, an **index** reduces a query to roughly the answer's size plus O(log n). The classic one is the **k-d tree**.

## k-d tree

A **k-d tree** is a binary search tree over points in *k* dimensions. Each node stores one point and splits the space by a hyperplane perpendicular to one coordinate axis: points with a smaller value on that axis go left, larger go right. The axis cycles with depth (x, y, x, y, ... in 2-D). Choosing the **median** point as the node keeps the tree balanced with depth ⌈log₂ n⌉.

The tree here is **implicit**: the points are stored in one array, rearranged so that the median of each subarray is the node, with its left subtree in the part before it and the right subtree after. There are no pointers, so the structure is compact and cache-friendly. Construction uses **quickselect** (see the selection note) to put the median in place in expected linear time per level, so the build takes expected **O(n log n)**.

### Queries

- **Nearest neighbour**: descend to the leaf region containing the query, recording the best point seen; on the way back, the other side of a split can contain something closer *only if* the distance from the query to the splitting plane is smaller than the best distance found so far, so visit it only in that case. For low dimensions this prunes almost the entire tree: expected O(log n) per query for uniform data.
- **Range query** (orthogonal box): visit a node's left subtree if the box extends to the left of the split value, the right subtree if it extends right; report points inside. Cost O(n^(1−1/k) + r) worst case for *r* reported points (in 2-D about √n + r), O(log n + r) for typical small boxes.
- **k nearest neighbours**: keep a bounded max-heap of the best *k* and use its worst distance as the pruning bound. **Radius queries** are the same with a fixed bound.

### Code

```c
// A k-d tree over D-dimensional double points (D fixed at compile time), plus a uniform grid for 2-D points.
// The tree is implicit: points are reordered so that the median of each subarray is the node, left half before it, right half after.
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef D
#define D 2
#endif
typedef struct { double c[D]; int id; } kpt;

static int g_axis;
static int cmp_axis(const void *a, const void *b) { double x = ((const kpt *)a)->c[g_axis], y = ((const kpt *)b)->c[g_axis]; return (x > y) - (x < y); }

// Quickselect: put the element of rank k (within [lo,hi)) in place, smaller before, larger after: O(n) expected per level.
static void select_k(kpt *p, int lo, int hi, int k, int axis) {
    while (hi - lo > 1) {
        double pivot = p[lo + (hi - lo) / 2].c[axis];
        int i = lo, j = hi - 1;
        while (i <= j) {
            while (p[i].c[axis] < pivot) i++;
            while (p[j].c[axis] > pivot) j--;
            if (i <= j) { kpt t = p[i]; p[i] = p[j]; p[j] = t; i++; j--; }
        }
        if (k <= j) hi = j + 1; else if (k >= i) lo = i; else return;
    }
}
// Build over p[lo,hi); the axis cycles with depth. Iterative explicit stack avoids deep recursion on adversarial data.
static void kd_build(kpt *p, int n) {
    struct { int lo, hi, axis; } st[128]; int sp = 0;
    st[sp++] = (typeof(st[0])){0, n, 0};
    while (sp) {
        int lo = st[sp - 1].lo, hi = st[sp - 1].hi, axis = st[sp - 1].axis; sp--;
        if (hi - lo <= 1) continue;
        int mid = lo + (hi - lo) / 2;
        select_k(p, lo, hi, mid, axis);
        st[sp++] = (typeof(st[0])){lo, mid, (axis + 1) % D};
        st[sp++] = (typeof(st[0])){mid + 1, hi, (axis + 1) % D};
    }
}
static double dist2k(const double *a, const double *b) { double s = 0; for (int i = 0; i < D; i++) { double d = a[i] - b[i]; s += d * d; } return s; }

// Nearest neighbour: descend towards the query, then on the way back visit the far side only if the splitting plane is closer than the best so far.
static void nn_rec(const kpt *p, int lo, int hi, int axis, const double *q, int *best, double *bd, long *visited) {
    if (lo >= hi) return;
    int mid = lo + (hi - lo) / 2; (*visited)++;
    double d = dist2k(p[mid].c, q);
    if (d < *bd) { *bd = d; *best = mid; }
    double diff = q[axis] - p[mid].c[axis];
    int na = (axis + 1) % D;
    if (diff < 0) { nn_rec(p, lo, mid, na, q, best, bd, visited); if (diff * diff < *bd) nn_rec(p, mid + 1, hi, na, q, best, bd, visited); }
    else          { nn_rec(p, mid + 1, hi, na, q, best, bd, visited); if (diff * diff < *bd) nn_rec(p, lo, mid, na, q, best, bd, visited); }
}
static int kd_nearest(const kpt *p, int n, const double *q, double *dist2_out, long *visited) {
    int best = -1; double bd = INFINITY; long v = 0;
    nn_rec(p, 0, n, 0, q, &best, &bd, &v);
    if (dist2_out) *dist2_out = bd;
    if (visited) *visited = v;
    return best;
}
// Orthogonal range query: count points in the box [lo, hi] per axis.
static long range_rec(const kpt *p, int l, int h, int axis, const double *lo, const double *hi, long *visited) {
    if (l >= h) return 0;
    int mid = l + (h - l) / 2; (*visited)++;
    long c = 1;
    for (int i = 0; i < D; i++) if (p[mid].c[i] < lo[i] || p[mid].c[i] > hi[i]) { c = 0; break; }
    int na = (axis + 1) % D;
    if (lo[axis] <= p[mid].c[axis]) c += range_rec(p, l, mid, na, lo, hi, visited);
    if (hi[axis] >= p[mid].c[axis]) c += range_rec(p, mid + 1, h, na, lo, hi, visited);
    return c;
}
static long kd_range(const kpt *p, int n, const double *lo, const double *hi, long *visited) { long v = 0; long c = range_rec(p, 0, n, 0, lo, hi, &v); if (visited) *visited = v; return c; }
```

### Verification

The tree is compared with brute force on random queries in 2, 3 and 8 dimensions. A third of the cases place points on a coarse integer grid, which produces many equal coordinates, duplicate points and ties, the situations where median partitioning and "go left or right on equality" mistakes hide. It runs under AddressSanitizer and UBSan.

```c
// k-d tree nearest neighbour and range counting against brute force, in 2, 3 and 8 dimensions, including duplicate points.
#include <stdio.h>
#include <stdint.h>
#define D DIM
#include "kd.h"

static uint64_t rng = 0x9E3779B97F4A7C15ULL;
static double urand(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (rng >> 11) * (1.0 / 9007199254740992.0); }

int main(void) {
    long checks = 0;
    for (int trial = 0; trial < 300; trial++) {
        int n = 1 + (int)(urand() * 400), grid = (trial % 3 == 0) ? 4 : 0;           // coarse grid => many duplicates and equal coordinates
        kpt *p = malloc(n * sizeof *p), *orig = malloc(n * sizeof *orig);
        for (int i = 0; i < n; i++) { p[i].id = i; for (int a = 0; a < D; a++) p[i].c[a] = grid ? floor(urand() * grid) : urand(); }
        memcpy(orig, p, n * sizeof *p);
        kd_build(p, n);
        for (int q = 0; q < 40; q++) {
            double qq[D]; for (int a = 0; a < D; a++) qq[a] = grid ? urand() * grid : urand();
            double want = INFINITY; for (int i = 0; i < n; i++) { double d = dist2k(orig[i].c, qq); if (d < want) want = d; }
            double got; kd_nearest(p, n, qq, &got, NULL);
            if (got != want) { printf("nearest mismatch D=%d\n", D); return 1; }
            double lo[D], hi[D]; for (int a = 0; a < D; a++) { double x = urand() * (grid ? grid : 1), y = urand() * (grid ? grid : 1); lo[a] = x < y ? x : y; hi[a] = x < y ? y : x; }
            long cnt = 0; for (int i = 0; i < n; i++) { int in = 1; for (int a = 0; a < D; a++) if (orig[i].c[a] < lo[a] || orig[i].c[a] > hi[a]) in = 0; cnt += in; }
            if (kd_range(p, n, lo, hi, NULL) != cnt) { printf("range mismatch D=%d\n", D); return 1; }
            checks += 2;
        }
        free(p); free(orig);
    }
    printf("D=%d: %ld nearest-neighbour and range queries match brute force\n", D, checks);
    return 0;
}
```

```text
$ for d in 2 3 8; do gcc -O1 -g -Wall -Wextra -Wno-unused-function -DDIM=$d -fsanitize=address,undefined test.c -o t$d -lm && ASAN_OPTIONS=detect_leaks=0 ./t$d; done
D=2: 24000 nearest-neighbour and range queries match brute force
D=3: 24000 nearest-neighbour and range queries match brute force
D=8: 24000 nearest-neighbour and range queries match brute force
```

## The curse of dimensionality

The pruning argument works because in 2-D or 3-D a splitting plane quickly separates "near" from "far". As the number of dimensions grows, nearly all points become about equally far from any query (Beyer et al.), and a hypersphere around the best candidate crosses almost every splitting plane, so nothing can be pruned. The next program builds a tree of 200,000 uniformly random points for several dimensions and reports how many nodes a nearest-neighbour query visits and how long it takes compared with brute force:

```c
// Nearest-neighbour search cost versus dimension: how many of the n = 200,000 tree nodes does a query visit? (Compile once per dimension.)
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#define D DIM
#include "kd.h"

static uint64_t rng = 0x2545F4914F6CDD1DULL;
static double urand(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (rng >> 11) * (1.0 / 9007199254740992.0); }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

int main(void) {
    int n = 200000, Q = 2000;
    kpt *p = malloc(n * sizeof *p), *raw = malloc(n * sizeof *raw);
    for (int i = 0; i < n; i++) for (int a = 0; a < D; a++) raw[i].c[a] = urand();
    memcpy(p, raw, n * sizeof *p);
    double t0 = now(); kd_build(p, n); double tb = now() - t0;
    long visited = 0; double tq = 0, tbf = 0; double sink = 0;
    for (int q = 0; q < Q; q++) {
        double qq[D]; for (int a = 0; a < D; a++) qq[a] = urand();
        long v; double d; t0 = now(); kd_nearest(p, n, qq, &d, &v); tq += now() - t0; visited += v; sink += d;
        if (q < 100) { t0 = now(); double best = INFINITY; for (int i = 0; i < n; i++) { double e = dist2k(raw[i].c, qq); if (e < best) best = e; } tbf += now() - t0; sink += best; }
    }
    printf("D=%2d  build %.3f s  nodes visited per query: %8.0f of %d (%5.1f%%)   kd-tree %7.1f us/query   brute force %7.1f us/query\n",
           D, tb, (double)visited / Q, n, 100.0 * visited / Q / n, tq / Q * 1e6, tbf / 100 * 1e6);
    if (sink < 0) puts("");
    return 0;
}
```

```text
$ for d in 2 3 8 16; do gcc -O2 -Wall -Wextra -Wno-unused-function -DDIM=$d bench.c -o b$d -lm && ./b$d; done
D= 2  build 0.083 s  nodes visited per query:       24 of 200000 (  0.0%)   kd-tree     0.9 us/query   brute force   343.9 us/query
D= 3  build 0.087 s  nodes visited per query:       40 of 200000 (  0.0%)   kd-tree     2.9 us/query   brute force   889.3 us/query
D= 8  build 0.103 s  nodes visited per query:     1707 of 200000 (  0.9%)   kd-tree    73.6 us/query   brute force  1287.1 us/query
D=16  build 0.107 s  nodes visited per query:   177498 of 200000 ( 88.7%)   kd-tree 14043.7 us/query   brute force  3643.4 us/query
```

The pattern, which is robust across machines even though the microsecond values are not:

- In **2-D and 3-D** a query touches a few dozen of the 200,000 nodes and is two to three orders of magnitude faster than the scan.
- At **8 dimensions** about one percent of nodes are visited; the tree still wins, by an order of magnitude.
- At **16 dimensions** a query visits almost 90 percent of the nodes and the k-d tree is *slower* than brute force: it does the same work as the scan plus recursion overhead and cache misses. The usual rule of thumb is that k-d trees pay off when the number of points is much larger than 2^k.

For high-dimensional data (embeddings of text, images, audio: hundreds of dimensions) exact indexes do not help, and practical systems use **approximate nearest neighbour** methods: graph-based indexes such as **HNSW**, inverted-file and product-quantization indexes (as in FAISS), locality-sensitive hashing, and random-projection forests, trading a small loss of recall for a large speed-up. Dimensionality reduction (PCA) before indexing is common, and so is exploiting the fact that real data often has low *intrinsic* dimension.

## Other spatial structures

| Structure | Idea | Best for |
|---|---|---|
| **Uniform grid / spatial hash** | divide space into equal cells; hash points by cell; query the cell and its neighbours | evenly distributed points, collision detection in games, fixed query radius; O(1) updates |
| **Quadtree** (octree in 3-D) | recursively split a square into four quadrants until each holds few points | clustered data in 2-D/3-D, level-of-detail rendering, image compression; adapts to density |
| **k-d tree** | alternate axis splits at the median | static point sets, nearest neighbour in low dimension |
| **R-tree / R\*-tree** | tree of nested **bounding rectangles**; leaves hold objects; a node's children's rectangles may overlap; balanced like a B-tree | rectangles, polygons and points on **disk**; used by PostGIS (GiST), SQLite R*Tree, MySQL spatial indexes |
| **BVH** (bounding volume hierarchy) | tree of bounding boxes over objects | ray tracing, collision detection |
| **Space-filling curve + ordinary index** | map 2-D to 1-D with a Z-order (Morton) or Hilbert curve, store in a B-tree | databases without spatial support; geohash, S2, H3 use this idea on the sphere |
| **Geohash / S2 / H3** | hierarchical cell identifiers of the Earth's surface; nearby points share a prefix | geographic "points near here" queries, sharding by location |

### Space-filling curves in brief

A **Morton (Z-order) code** interleaves the bits of x and y, so that points close in the plane are usually close in the code, which can then be stored in any ordered structure (a B-tree, a sorted file, a key-value store). It is cheap to compute (bit interleaving) but has jumps: two adjacent cells can have very different codes at the boundary of a large quadrant, so a range query becomes several code ranges. The **Hilbert curve** has better locality (no jumps) at higher computation cost. **Geohash** is a base-32 encoding of a Z-order code of latitude and longitude, where longer prefixes mean smaller cells; remember to query the neighbouring cells because a point near a cell edge can have a closer neighbour in the adjacent cell.

### R-trees

R-trees are the standard for spatial data in databases because, like B-trees, they are balanced, page-oriented and support inserts and deletes. Each node holds up to *M* entries of (bounding rectangle, child pointer). A search descends into every child whose rectangle intersects the query; because rectangles can overlap, more than one path may be followed, so quality depends on keeping overlap and dead space small. Insertion chooses the subtree needing the least enlargement; splits minimize area or overlap (the R\*-tree adds forced reinsertion). For static data, **bulk loading** (Sort-Tile-Recursive, Hilbert-sorted packing) builds a nearly optimal tree much faster than repeated insertion.

## Choosing

1. A few thousand points: scan. Simpler and often faster than any index (cache-friendly, no build).
2. Points on a plane with a roughly constant query radius and frequent updates: **grid / spatial hash**.
3. Static or slowly changing low-dimensional point set, nearest neighbours: **k-d tree**.
4. Rectangles/polygons or data on disk: **R-tree** (or an existing database extension).
5. Geographic points in a key-value store: **geohash/S2/H3** cells as keys.
6. Hundreds of dimensions: **approximate** methods and evaluate recall against brute force on a sample.

## Pitfalls

- **Dynamic data**: a k-d tree built by median splitting does not stay balanced under inserts. Rebuild periodically, use the logarithmic method (several trees of sizes 2^i, merged as needed), or choose a quadtree/R-tree/grid.
- **Equal coordinates and duplicates** require a consistent rule for which side they go to; the test above uses data that exercises it.
- **Distance metrics**: the pruning rule above is for Euclidean distance. Other metrics (cosine, geographic great-circle distance) need adaptation: on the sphere, index 3-D unit vectors and use chord length, which is monotonic with the great-circle distance.
- **Scaling the axes**: if the dimensions have different units or ranges, distance is dominated by the largest; normalize before indexing.
- **Memory layout**: the implicit array layout is fast because of locality; a pointer-based tree with one allocation per node can be several times slower.

## Key takeaways

- A k-d tree splits space by alternating axes at the median; nearest-neighbour search prunes the far side of a split whenever the splitting plane is farther than the best distance so far.
- It is excellent in 2-D to ~10-D and loses its advantage as dimensions grow: measured above, a query visits a few dozen nodes in 2-D but almost 90 percent of them in 16-D, where brute force wins.
- Use grids for uniform data and moving points, quadtrees for clustered 2-D/3-D data, R-trees for rectangles and disk-based spatial indexes, space-filling-curve keys (geohash, S2, H3) to reuse ordinary ordered stores, and approximate methods for high-dimensional vectors.
- Always verify an index against brute force on random data and on degenerate data with ties and duplicates.
