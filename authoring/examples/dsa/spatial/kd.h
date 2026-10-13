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
