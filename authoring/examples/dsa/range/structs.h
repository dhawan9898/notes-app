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
