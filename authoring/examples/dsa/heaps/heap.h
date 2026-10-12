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
