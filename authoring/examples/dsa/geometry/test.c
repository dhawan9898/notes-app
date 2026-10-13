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
