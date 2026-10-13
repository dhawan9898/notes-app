---
title: "Computational Geometry: Orientation, Hulls, Segments, Polygons and Closest Pair"
track: dsa
module: Algorithm Design
moduleOrder: 8
order: 6
level: advanced
summary: "The geometric primitives that everything else is built on (the cross product and the orientation test, exact in integers and not in doubles), plus segment intersection, Andrew's convex hull, polygon area and point-in-polygon, and divide-and-conquer closest pair, all verified against independent implementations."
prerequisites:
  - Comparison Sorts — Insertion, Merge, Quick, Heap and Introsort
  - Number Theory Essentials
sources:
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, Ch. 33 Computational Geometry"
  - label: "de Berg, Cheong, van Kreveld, Overmars — Computational Geometry: Algorithms and Applications (3rd ed., Springer 2008)"
    url: https://link.springer.com/book/10.1007/978-3-540-77974-2
  - label: "Andrew — Another efficient algorithm for convex hulls in two dimensions (Information Processing Letters 9, 1979)"
  - label: "Shamos & Hoey — Closest-point problems (FOCS 1975)"
    url: https://ieeexplore.ieee.org/document/4567872
  - label: "Shewchuk — Adaptive Precision Floating-Point Arithmetic and Fast Robust Geometric Predicates (Discrete & Computational Geometry 18, 1997)"
    url: https://people.eecs.berkeley.edu/~jrs/papers/robustr.pdf
  - label: "Kahan — How Futile are Mindless Assessments of Roundoff in Floating-Point Computation? (the robustness of geometric tests)"
    url: https://people.eecs.berkeley.edu/~wkahan/Mindless.pdf
---

Computational geometry turns questions about shapes (is this point inside that polygon? which points form the outline of a set? do these two roads cross?) into algorithms on coordinates. Its main difficulty is not the asymptotic analysis. It is that the *predicates* (left or right of a line? crossing or not?) must be answered correctly even for degenerate input: collinear points, duplicates, touching segments. A single wrong sign in a near-degenerate case can make a convex hull non-convex or a polygon routine loop forever.

## The one primitive: the cross product

For vectors `u = (ux, uy)` and `v = (vx, vy)`, the 2-D cross product `u × v = ux·vy − uy·vx` is the signed area of the parallelogram they span. Its **sign** says which way you turn going from `u` to `v`: positive for a counter-clockwise (left) turn, negative for clockwise (right), zero if the vectors are collinear. The **orientation test** of three points applies it to `b − a` and `c − a`:

`orient(a, b, c) = sign((b − a) × (c − a))`

Almost every algorithm below consists of orientation tests, comparisons of coordinates, and sorting. Nothing needs angles, `atan2`, slopes or divisions, all of which add rounding error and special cases (vertical lines).

### Exactness: why integers and 128 bits

With integer coordinates up to ±10⁹, differences fit in 32 bits, each product in 63 bits, and the difference of products in 64 bits: just past what a `long long` holds safely. The code below uses `__int128` for the cross product so that **every predicate is exact**.

Doubles are not safe. A `double` has 53 bits of precision, but each product in the cross product can be about 10ⁱ⁸ (60 bits), and subtracting two nearly equal large numbers loses everything: the result is dominated by rounding error. The following program builds triples of points that are as close to collinear as integer points can be, using Cassini's identity for Fibonacci numbers (`F(k+1)·F(k−1) − F(k)² = ±1`, so the cross product is exactly ±1 while each product is around 10¹⁷), and compares the exact and double-precision answers:

```c
// Why exact predicates matter. Consecutive Fibonacci numbers satisfy Cassini's identity F(k+1)*F(k-1) - F(k)^2 = +-1, so the vectors
// b = (F(k+1), F(k)) and u = (F(k), F(k-1)) have a cross product of exactly +-1, while each product in it is about 10^17.
// Adding multiples of b to u keeps the cross product at +-1. These triples are as close to collinear as integer points can be.
#include "geom.h"
#include <stdio.h>

int main(void) {
    long long F[60]; F[0] = 0; F[1] = 1; for (int i = 2; i < 60; i++) F[i] = F[i - 1] + F[i - 2];
    long total = 0, collinear_by_double = 0, wrong_sign = 0, shown = 0;
    for (int k = 20; k < 44; k++)
        for (int m = 0; m < 400; m++) {
            pt a = {0, 0}, b = {F[k + 1], F[k]};
            pt c = {F[k] + m * b.x, F[k - 1] + m * b.y};
            if (c.x > 2000000000LL) break;
            int e = orient(a, b, c), d = orient_double(a, b, c);
            total++;
            if (d == 0) collinear_by_double++; else if (d != e) wrong_sign++;
            if (e != d && shown < 3) { shown++; printf("  example: b = (%lld, %lld), c = (%lld, %lld): exact orientation %+d, double says %+d\n", b.x, b.y, c.x, c.y, e, d); }
        }
    printf("%ld triples whose true orientation is never 0 (the cross product is +-1):\n", total);
    printf("  double precision reports 'collinear' for %ld of them and the opposite turn direction for %ld\n", collinear_by_double, wrong_sign);
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra -Wno-unused-function robust.c -o robust -lm && ./robust
  example: b = (9227465, 5702887), c = (1602054332, 990124029): exact orientation +1, double says +0
  example: b = (9227465, 5702887), c = (1611281797, 995826916): exact orientation +1, double says +0
  example: b = (9227465, 5702887), c = (1638964192, 1012935577): exact orientation +1, double says +0
6113 triples whose true orientation is never 0 (the cross product is +-1):
  double precision reports 'collinear' for 243 of them and the opposite turn direction for 0
```

For some of these triples the true orientation is a definite left turn, but the double computation reports "collinear". A convex hull or segment-intersection routine built on that predicate would treat a real corner as a straight line, and the failure is not a crash but a wrong answer. If the input is floating point, either convert to exact arithmetic first (scale to integers, or exact rationals), use an **adaptive-precision predicate** (Shewchuk's `orient2d`, which evaluates in double and only escalates when the error bound cannot decide the sign), or add an explicit tolerance and accept the consequences consciously.

## The code

```c
// 2-D computational geometry on integer coordinates (|x|,|y| <= 1e9): every predicate is exact because the arithmetic is.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct { long long x, y; } pt;

// Orientation of c relative to the directed line a->b: +1 left turn (counter-clockwise), -1 right turn, 0 collinear.
// The cross product of two vectors with 31-bit coordinates needs up to ~63 bits per product, so we use 128-bit arithmetic.
static int orient(pt a, pt b, pt c) {
    __int128 v = (__int128)(b.x - a.x) * (c.y - a.y) - (__int128)(b.y - a.y) * (c.x - a.x);
    return (v > 0) - (v < 0);
}
// the same predicate in double precision, to show why it is not safe
static int orient_double(pt a, pt b, pt c) {
    double v = (double)(b.x - a.x) * (double)(c.y - a.y) - (double)(b.y - a.y) * (double)(c.x - a.x);
    return (v > 0) - (v < 0);
}

static int on_segment(pt a, pt b, pt p) {            // p collinear with ab: is it between a and b (inclusive)?
    return (p.x >= (a.x < b.x ? a.x : b.x)) && (p.x <= (a.x > b.x ? a.x : b.x)) && (p.y >= (a.y < b.y ? a.y : b.y)) && (p.y <= (a.y > b.y ? a.y : b.y));
}
// Do closed segments ab and cd share at least one point? Handles all the collinear and touching cases.
static int segments_intersect(pt a, pt b, pt c, pt d) {
    int o1 = orient(a, b, c), o2 = orient(a, b, d), o3 = orient(c, d, a), o4 = orient(c, d, b);
    if (o1 != o2 && o3 != o4) return 1;                                      // proper crossing (or endpoint on the other line, with different sides)
    if (o1 == 0 && on_segment(a, b, c)) return 1;
    if (o2 == 0 && on_segment(a, b, d)) return 1;
    if (o3 == 0 && on_segment(c, d, a)) return 1;
    if (o4 == 0 && on_segment(c, d, b)) return 1;
    return 0;
}

static int cmp_xy(const void *p, const void *q) {
    const pt *a = p, *b = q; return a->x != b->x ? (a->x > b->x) - (a->x < b->x) : (a->y > b->y) - (a->y < b->y);
}
// Andrew's monotone chain. Sorts pts in place, writes the hull in counter-clockwise order WITHOUT collinear points, returns its size.
// out must have room for 2*n points.
static int convex_hull(pt *pts, int n, pt *out) {
    qsort(pts, n, sizeof *pts, cmp_xy);
    int m = 0, u = 0;
    for (int i = 0; i < n; i++) if (i == 0 || cmp_xy(&pts[i], &pts[i - 1])) pts[u++] = pts[i];     // drop duplicates
    n = u;
    if (n < 3) { for (int i = 0; i < n; i++) out[m++] = pts[i]; return m; }
    for (int i = 0; i < n; i++) {                                            // lower hull
        while (m >= 2 && orient(out[m - 2], out[m - 1], pts[i]) <= 0) m--;   // <= 0 pops collinear points too
        out[m++] = pts[i];
    }
    for (int i = n - 2, lo = m + 1; i >= 0; i--) {                           // upper hull
        while (m >= lo && orient(out[m - 2], out[m - 1], pts[i]) <= 0) m--;
        out[m++] = pts[i];
    }
    return m - 1;                                                            // the last point repeats the first
}

// Twice the signed area of a simple polygon (positive if counter-clockwise): the shoelace formula, exact in 128 bits.
static __int128 area2(const pt *p, int n) {
    __int128 s = 0; for (int i = 0; i < n; i++) { pt a = p[i], b = p[(i + 1) % n]; s += (__int128)a.x * b.y - (__int128)b.x * a.y; } return s;
}

// Point in a simple polygon: -1 outside, 0 on the boundary, +1 inside (crossing-number test with exact arithmetic).
static int point_in_polygon(const pt *poly, int n, pt q) {
    int inside = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        pt a = poly[j], b = poly[i];
        if (orient(a, b, q) == 0 && on_segment(a, b, q)) return 0;
        if ((a.y > q.y) != (b.y > q.y)) {                                    // edge straddles the horizontal line through q
            // is q to the left of the crossing point? sign of orient, flipped when the edge goes downward
            int o = orient(a, b, q);
            if ((b.y > a.y) ? o > 0 : o < 0) inside = !inside;
        }
    }
    return inside ? 1 : -1;
}

// Closest pair of points by divide and conquer, O(n log n). pts is sorted by x first; returns the squared distance.
static long double dist2(pt a, pt b) { long double dx = a.x - b.x, dy = a.y - b.y; return dx * dx + dy * dy; }
static long double closest_rec(pt *p, pt *buf, int l, int r) {                 // [l, r)
    if (r - l <= 3) {
        long double best = INFINITY;
        for (int i = l; i < r; i++) for (int j = i + 1; j < r; j++) { long double d = dist2(p[i], p[j]); if (d < best) best = d; }
        // keep the range sorted by y for the merge step of the caller
        for (int i = l + 1; i < r; i++) { pt k = p[i]; int j = i - 1; while (j >= l && p[j].y > k.y) { p[j + 1] = p[j]; j--; } p[j + 1] = k; }
        return best;
    }
    int mid = (l + r) / 2; long long midx = p[mid].x;
    long double d = closest_rec(p, buf, l, mid), d2 = closest_rec(p, buf, mid, r);
    if (d2 < d) d = d2;
    // merge the two y-sorted halves
    int i = l, j = mid, k = l;
    while (i < mid && j < r) buf[k++] = p[i].y <= p[j].y ? p[i++] : p[j++];
    while (i < mid) buf[k++] = p[i++];
    while (j < r) buf[k++] = p[j++];
    memcpy(p + l, buf + l, (r - l) * sizeof(pt));
    // points within sqrt(d) of the dividing line, in y order; each needs comparing with at most ~7 successors
    int m = 0;
    for (int t = l; t < r; t++) { long double dx = p[t].x - midx; if (dx * dx < d) buf[m++] = p[t]; }
    for (int a = 0; a < m; a++) for (int b = a + 1; b < m; b++) {
        long double dy = buf[b].y - buf[a].y; if (dy * dy >= d) break;
        long double dd = dist2(buf[a], buf[b]); if (dd < d) d = dd;
    }
    return d;
}
static long double closest_pair2(pt *pts, int n) {
    qsort(pts, n, sizeof *pts, cmp_xy);
    pt *buf = malloc(n * sizeof *buf); long double d = closest_rec(pts, buf, 0, n); free(buf); return d;
}
```

### Segment intersection

Two closed segments `ab` and `cd` intersect when `c` and `d` lie on different sides of line `ab` *and* `a` and `b` lie on different sides of line `cd` (four orientation tests with different signs). That covers proper crossings. The remaining cases are **degenerate**: an endpoint of one segment lies exactly on the other (orientation 0 and the point is within the other's bounding box), or the segments are collinear and overlap. Writing the four `on_segment` checks explicitly is what makes the function handle touching and collinear segments.

### Convex hull: Andrew's monotone chain

The **convex hull** of a point set is the smallest convex polygon containing it (picture a rubber band around nails). Andrew's algorithm sorts the points by `(x, y)`, then builds the **lower hull** by scanning left to right while keeping a stack of points that always turn left: before pushing a point, pop the top while the last two stack points and the new point do not make a strict left turn (`orient <= 0`). A second scan right to left builds the upper hull. The popping condition `<= 0` removes collinear points; using `< 0` would keep them. Cost: `O(n log n)` for sorting, `O(n)` for the two scans, and the output size is at most `n`. Duplicates must be removed first, and fewer than three distinct points, or all points collinear, are special cases (the hull degenerates to a point or a segment).

The lower bound for the hull is `Ω(n log n)` in the comparison model (sorting reduces to it), and **output-sensitive** algorithms such as Chan's run in `O(n log h)` for `h` hull points. In 3-D the hull is also `O(n log n)` but considerably more intricate; libraries such as CGAL and Qhull handle it.

### Area and point in polygon

For a simple polygon with vertices `p₀ … p_{n−1}`, the **shoelace formula** `2A = Σ (xᵢ·yᵢ₊₁ − xᵢ₊₁·yᵢ)` gives twice the signed area, positive for counter-clockwise order. It is exact in integers, needs no division (so the area is a multiple of ½), and also tells you the vertex order: negative means clockwise. The same sum decomposes the polygon into triangles from the origin; areas of the signed pieces cancel outside the polygon.

**Point in polygon** casts a ray from the query point to the right and counts how many edges it crosses: odd means inside. Only edges that straddle the point's horizontal line count (with the half-open rule `(a.y > q.y) != (b.y > q.y)` that treats a vertex on the ray consistently), and whether the crossing is to the right of `q` is decided with the same orientation test, flipped for downward edges, so no division is needed. Points exactly on an edge are detected first and reported separately. Cost `O(n)` per query; for many queries on a fixed polygon use a trapezoid map or a grid, and for convex polygons binary search on the angle from one vertex gives `O(log n)`.

### Closest pair of points

Naively compare all pairs in `O(n²)`. The **divide-and-conquer** algorithm (Shamos–Hoey) sorts by x, splits in half, finds the minimum distance `d` within each half recursively, and then handles pairs that straddle the dividing line. Only points within distance `d` of the line matter, and if those are examined in order of y, each needs to be compared with at most a handful (≤ 7) of its successors, because a `d × 2d` rectangle can hold only a bounded number of points that are pairwise at least `d` apart. Merging the halves by y (as in merge sort) keeps the whole thing at `T(n) = 2T(n/2) + O(n) = O(n log n)`. The implementation squares distances (no square root) and uses `long double` for them since squared distances of 10⁹-range coordinates reach 10¹⁹.

## Verification

Each routine is compared with an independent method: the monotone-chain hull with gift wrapping (Jarvis march, `O(nh)`), on small coordinate ranges where duplicates and collinear points are everywhere and on large ranges; segment intersection with a parametric solution of the two lines in 128-bit integers on small ranges (all touching and overlapping configurations occur) and large ones; point-in-polygon with the half-plane definition for convex polygons including boundary points; and closest pair with the quadratic scan.

```c
// Cross-check the geometry routines against independent slower methods, on small coordinate ranges (many degeneracies) and large ones.
#include "geom.h"
#include <stdio.h>

static uint64_t rng = 0x2545F4914F6CDD1DULL;
static long long rnd(long long range) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (long long)(rng % (2 * range + 1)) - range; }

static int hull_gift_wrap(pt *p, int n, pt *out) {           // Jarvis march O(n h), a different algorithm; collinear boundary points excluded
    int start = 0; for (int i = 1; i < n; i++) if (cmp_xy(&p[i], &p[start]) < 0) start = i;
    int m = 0, cur = start;
    do {
        out[m++] = p[cur]; int nxt = (cur + 1) % n;
        for (int i = 0; i < n; i++) {
            int o = orient(p[cur], p[nxt], p[i]);
            if (o < 0 || (o == 0 && dist2(p[cur], p[i]) > dist2(p[cur], p[nxt]))) nxt = i;     // farthest among collinear
        }
        cur = nxt;
    } while (cur != start && m <= n);
    // remove collinear points on the way (keep only strict corners)
    int k = 0; pt *tmp = malloc(m * sizeof *tmp);
    for (int i = 0; i < m; i++) { pt a = out[(i + m - 1) % m], b = out[i], c = out[(i + 1) % m]; if (m < 3 || orient(a, b, c) != 0) tmp[k++] = b; }
    memcpy(out, tmp, k * sizeof *tmp); free(tmp); return k;
}

int main(void) {
    long hullc = 0, segc = 0, pipc = 0, cpc = 0;
    // 1. convex hull vs gift wrapping
    for (int t = 0; t < 3000; t++) {
        int n = 1 + rnd(30) + 30, range = (t % 3 == 0) ? 3 : (t % 3 == 1) ? 20 : 1000000000;   // tiny range: lots of duplicates and collinear points
        pt *p = malloc(n * sizeof *p), *q = malloc(n * sizeof *q), *h1 = malloc(2 * n * sizeof *h1), *h2 = malloc(2 * n * sizeof *h2);
        for (int i = 0; i < n; i++) p[i] = (pt){rnd(range), rnd(range)};
        memcpy(q, p, n * sizeof *p);
        int a = convex_hull(p, n, h1);
        // gift wrapping needs distinct points
        qsort(q, n, sizeof *q, cmp_xy); int u = 0; for (int i = 0; i < n; i++) if (!i || cmp_xy(&q[i], &q[i - 1])) q[u++] = q[i];
        int b = u >= 3 ? hull_gift_wrap(q, u, h2) : u;
        if (u >= 3) {
            if (a != b) { printf("hull size mismatch %d vs %d\n", a, b); return 1; }
            // compare as sets: every corner of h1 appears in h2
            for (int i = 0; i < a; i++) { int f = 0; for (int j = 0; j < b; j++) if (!cmp_xy(&h1[i], &h2[j])) f = 1; if (!f) { printf("hull corner mismatch\n"); return 1; } }
            if (a >= 3 && area2(h1, a) <= 0) { printf("hull not counter-clockwise\n"); return 1; }
        }
        hullc++; free(p); free(q); free(h1); free(h2);
    }
    // 2. segment intersection: compare with a method based on solving for the intersection of the lines in exact integer arithmetic
    for (int t = 0; t < 200000; t++) {
        int range = (t % 2) ? 4 : 1000;
        pt a = {rnd(range), rnd(range)}, b = {rnd(range), rnd(range)}, c = {rnd(range), rnd(range)}, d = {rnd(range), rnd(range)};
        int got = segments_intersect(a, b, c, d), want = 0;
        // brute force: enumerate lattice points is possible only for tiny ranges; otherwise use the interval test via parametric t,u with __int128
        __int128 rx = b.x - a.x, ry = b.y - a.y, sx = d.x - c.x, sy = d.y - c.y, den = rx * sy - ry * sx;
        __int128 qx = c.x - a.x, qy = c.y - a.y;
        if ((rx == 0 && ry == 0) || (sx == 0 && sy == 0)) continue;       // degenerate (point) segments are outside this comparison
        if (den != 0) {                                       // lines cross at a=a+t*r=c+u*s with t = (q x s)/den, u = (q x r)/den; need both in [0, 1]
            __int128 tn = qx * sy - qy * sx, un = qx * ry - qy * rx;
            if (den < 0) { den = -den; tn = -tn; un = -un; }
            want = tn >= 0 && tn <= den && un >= 0 && un <= den;
        } else if (qx * ry - qy * rx == 0) {                  // parallel and collinear: the intervals along r must overlap
            __int128 r2 = rx * rx + ry * ry, t0 = qx * rx + qy * ry, t1 = t0 + sx * rx + sy * ry;
            if (t0 > t1) { __int128 x = t0; t0 = t1; t1 = x; }
            want = !(t1 < 0 || t0 > r2);
        }
        if (got != want) { printf("segment mismatch: (%lld,%lld)-(%lld,%lld) vs (%lld,%lld)-(%lld,%lld): got %d want %d\n", a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y, got, want); return 1; }
        segc++;
    }
    // 3. point in polygon: a convex polygon (the hull of random points) against the half-plane definition
    for (int t = 0; t < 3000; t++) {
        int n = 5 + t % 20; pt *p = malloc(n * sizeof *p), *h = malloc(2 * n * sizeof *h);
        for (int i = 0; i < n; i++) p[i] = (pt){rnd(10), rnd(10)};
        int hn = convex_hull(p, n, h);
        if (hn >= 3) for (int q = 0; q < 50; q++) {
            pt s = {rnd(12), rnd(12)}; int outside = 0, on_boundary = 0;
            for (int i = 0; i < hn; i++) {                    // for a convex counter-clockwise polygon: outside if strictly right of any edge
                pt a = h[i], b = h[(i + 1) % hn]; int o = orient(a, b, s);
                if (o < 0) outside = 1;
                if (o == 0 && on_segment(a, b, s)) on_boundary = 1;
            }
            int want = outside ? -1 : on_boundary ? 0 : 1;
            if (point_in_polygon(h, hn, s) != want) { printf("point-in-polygon mismatch\n"); return 1; }
            pipc++;
        }
        free(p); free(h);
    }
    // 4. closest pair vs O(n^2)
    for (int t = 0; t < 2000; t++) {
        int n = 2 + t % 60, range = (t % 2) ? 10 : 100000; pt *p = malloc(n * sizeof *p), *q = malloc(n * sizeof *q);
        for (int i = 0; i < n; i++) p[i] = (pt){rnd(range), rnd(range)};
        memcpy(q, p, n * sizeof *p);
        long double best = INFINITY; for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) { long double d = dist2(q[i], q[j]); if (d < best) best = d; }
        if (closest_pair2(p, n) != best) { printf("closest pair mismatch\n"); return 1; }
        cpc++; free(p); free(q);
    }
    printf("convex hull: %ld cases match gift wrapping; segment intersection: %ld cases match the parametric method;\n", hullc, segc);
    printf("point in polygon: %ld queries match the half-plane test; closest pair: %ld cases match the O(n^2) scan\n", pipc, cpc);
    return 0;
}
```

```text
$ gcc -O1 -g -Wall -Wextra -Wno-unused-function -fsanitize=address,undefined test.c -o t -lm && ASAN_OPTIONS=detect_leaks=0 ./t
convex hull: 3000 cases match gift wrapping; segment intersection: 197573 cases match the parametric method;
point in polygon: 150000 queries match the half-plane test; closest pair: 2000 cases match the O(n^2) scan
```

(Leak reporting is off only because the test returns early on a mismatch.)

## Beyond these primitives

| Problem | Standard approach |
|---|---|
| Detect whether any two of *n* segments intersect | sweep line with a balanced tree ordered by y at the sweep position (Shamos–Hoey), `O(n log n)` |
| Report all `k` intersections | Bentley–Ottmann sweep, `O((n + k) log n)` |
| Diameter of a point set, farthest pair | rotating calipers on the convex hull, `O(n)` after the hull |
| Triangulate a polygon | ear clipping `O(n²)`, monotone decomposition `O(n log n)`, Chazelle `O(n)` (theoretical) |
| Voronoi diagram / Delaunay triangulation | Fortune's sweep, incremental or divide and conquer, `O(n log n)`; Delaunay maximizes the minimum angle, used for meshes |
| Half-plane intersection, linear programming in 2-D | sort by angle and sweep with a deque, `O(n log n)`; Seidel's randomized algorithm `O(n)` expected |
| Area of union of rectangles, circles | sweep line with segment tree or integration |
| Range search (points in a rectangle) | k-d tree, range tree, grid (see the next note on spatial indexes) |
| Minimum enclosing circle | Welzl's randomized algorithm, expected `O(n)` |

## Pitfalls

- **Floating-point predicates** (shown above). Decide on exact coordinates or robust predicates at the start of a project; retrofitting is painful.
- **Degeneracies**: duplicate points, collinear triples, vertical segments, zero-length segments, coincident endpoints. List them in tests; random data hides almost all of them, small coordinate ranges expose them quickly (as in the test above).
- **Polygon orientation**: many algorithms assume counter-clockwise order; compute the signed area and reverse if negative.
- **Overflow**: cross products of 32-bit coordinates overflow 64-bit signed integers in the extreme case; use 128-bit arithmetic or bound coordinates.
- **Sorting comparators** that use subtraction have the overflow problem from the standard library note; compare with `<` and `>`.
- **Non-simple polygons** (self-intersecting): the shoelace formula and ray casting then give "signed area" and "even-odd" semantics, which may not be what the caller means; state which fill rule is used (even-odd or non-zero winding).

## Key takeaways

- Nearly all 2-D geometry reduces to the orientation test, the sign of a cross product; compute it exactly (integers in 128 bits) because double precision gives wrong signs for nearly collinear points.
- Segment intersection needs four orientation tests plus explicit collinear/touching cases; Andrew's monotone chain gives the convex hull in `O(n log n)`; the shoelace formula gives polygon area and orientation; a ray-casting count gives point in polygon.
- Closest pair by divide and conquer is `O(n log n)` because only a bounded number of points near the dividing line need checking.
- Test geometry code against slower independent methods on tiny coordinate ranges where degeneracies are frequent.
