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
