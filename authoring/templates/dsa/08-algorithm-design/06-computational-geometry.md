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

{{include dsa/geometry/robust.c}}

{{run dsa/geometry :: gcc -O2 -Wall -Wextra -Wno-unused-function robust.c -o robust -lm && ./robust}}

For some of these triples the true orientation is a definite left turn, but the double computation reports "collinear". A convex hull or segment-intersection routine built on that predicate would treat a real corner as a straight line, and the failure is not a crash but a wrong answer. If the input is floating point, either convert to exact arithmetic first (scale to integers, or exact rationals), use an **adaptive-precision predicate** (Shewchuk's `orient2d`, which evaluates in double and only escalates when the error bound cannot decide the sign), or add an explicit tolerance and accept the consequences consciously.

## The code

{{include dsa/geometry/geom.h}}

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

{{include dsa/geometry/test.c}}

{{run dsa/geometry :: gcc -O1 -g -Wall -Wextra -Wno-unused-function -fsanitize=address,undefined test.c -o t -lm && ASAN_OPTIONS=detect_leaks=0 ./t}}

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
