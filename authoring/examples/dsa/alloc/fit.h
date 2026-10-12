#ifndef FIT_H
#define FIT_H
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* A boundary-tag allocator over a fixed heap, with an explicit doubly linked free list and three placement policies.
 *
 *   block:  [ header 8 B | payload ........................ | footer 8 B ]      header = footer = size | allocated-bit
 *   a FREE block keeps its list links in the first 16 bytes of the payload (offsets from the heap base)
 *   heap:   [ prologue (16 B, allocated) | blocks ... | epilogue header (size 0, allocated) ]
 *
 * Because every block carries a footer, free() can find the PREVIOUS block in O(1) and merge ("coalesce") with free neighbours. */
enum { FIRST_FIT, NEXT_FIT, BEST_FIT };
typedef uint64_t w_t;
typedef struct { uint8_t *base; size_t size; w_t free_head; w_t rover; int policy; unsigned long long steps, allocs, fails; size_t live_bytes, high_water; } Heap;

#define ALLOC 1u
#define MINBLK 32
static inline w_t pack(size_t sz, int a) { return (w_t)sz | (w_t)a; }
static inline size_t bsize(const uint8_t *hp) { return (size_t)(*(const w_t *)hp & ~(w_t)15); }
static inline int balloc(const uint8_t *hp) { return (int)(*(const w_t *)hp & 1); }
static inline void set_block(uint8_t *hp, size_t sz, int a) { *(w_t *)hp = pack(sz, a); *(w_t *)(hp + sz - 8) = pack(sz, a); }
#define NEXT_OF(h, off) (*(w_t *)((h)->base + (off) + 8))
#define PREV_OF(h, off) (*(w_t *)((h)->base + (off) + 16))

static void fl_insert(Heap *h, w_t off) { NEXT_OF(h, off) = h->free_head; PREV_OF(h, off) = 0; if (h->free_head) PREV_OF(h, h->free_head) = off; h->free_head = off; }
static void fl_remove(Heap *h, w_t off) {
    w_t n = NEXT_OF(h, off), p = PREV_OF(h, off);
    if (p) NEXT_OF(h, p) = n; else h->free_head = n;
    if (n) PREV_OF(h, n) = p;
    if (h->rover == off) h->rover = n ? n : h->free_head;
}
static void heap_init(Heap *h, size_t size, int policy) {
    memset(h, 0, sizeof *h); h->size = size; h->policy = policy; h->base = aligned_alloc(16, size);
    set_block(h->base, 16, 1);                                                      /* prologue */
    size_t first = size - 16 - 8; first &= ~(size_t)15;                             /* one big free block, leaving room for the epilogue */
    set_block(h->base + 16, first, 0); *(w_t *)(h->base + 16 + first) = pack(0, 1); /* epilogue header */
    fl_insert(h, 16); h->rover = 16;
}
static void heap_destroy(Heap *h) { free(h->base); }

static void *h_malloc(Heap *h, size_t n) {
    size_t need = (n + 16 + 15) & ~(size_t)15; if (need < MINBLK) need = MINBLK;
    w_t best = 0, found = 0; h->allocs++;
    if (h->policy == FIRST_FIT) { for (w_t o = h->free_head; o; o = NEXT_OF(h, o)) { h->steps++; if (bsize(h->base + o) >= need) { found = o; break; } } }
    else if (h->policy == NEXT_FIT) {
        w_t start = h->rover ? h->rover : h->free_head, o = start;
        if (o) do { h->steps++; if (bsize(h->base + o) >= need) { found = o; break; } o = NEXT_OF(h, o) ? NEXT_OF(h, o) : h->free_head; } while (o != start);
    } else { for (w_t o = h->free_head; o; o = NEXT_OF(h, o)) { h->steps++; size_t s = bsize(h->base + o); if (s >= need && (!best || s < bsize(h->base + best))) { best = o; if (s == need) break; } } found = best; }
    if (!found) { h->fails++; return NULL; }
    uint8_t *hp = h->base + found; size_t sz = bsize(hp); h->rover = NEXT_OF(h, found) ? NEXT_OF(h, found) : h->free_head; fl_remove(h, found);
    if (sz - need >= MINBLK) { set_block(hp, need, 1); set_block(hp + need, sz - need, 0); fl_insert(h, found + need); } else set_block(hp, sz, 1);
    h->live_bytes += bsize(hp); if (h->live_bytes > h->high_water) h->high_water = h->live_bytes;
    return hp + 8;
}
static void h_free(Heap *h, void *p) {
    if (!p) return;
    uint8_t *hp = (uint8_t *)p - 8; size_t sz = bsize(hp); h->live_bytes -= sz;
    uint8_t *next = hp + sz; int prev_alloc = balloc(hp - 8);                      /* previous block's footer sits just below our header */
    if (!balloc(next)) { size_t ns = bsize(next); fl_remove(h, (w_t)(next - h->base)); sz += ns; }
    if (!prev_alloc) { size_t ps = bsize(hp - 8); hp -= ps; fl_remove(h, (w_t)(hp - h->base)); sz += ps; }
    set_block(hp, sz, 0); fl_insert(h, (w_t)(hp - h->base));
}
/* Heap consistency checker: walk every block by size and cross-check against the free list. Returns total free bytes; asserts on corruption. */
static size_t heap_check(const Heap *h, size_t *largest_free, size_t *nfree_blocks) {
    const uint8_t *p = h->base + 16; size_t free_bytes = 0, nfree = 0, largest = 0; int prev_free = 0;
    while (bsize(p) != 0) {
        size_t s = bsize(p); assert(s >= MINBLK && s % 16 == 0 && p + s <= h->base + h->size);
        assert(*(const w_t *)p == *(const w_t *)(p + s - 8));                       /* header == footer */
        if (!balloc(p)) { assert(!prev_free); free_bytes += s; nfree++; if (s > largest) largest = s; prev_free = 1; } else prev_free = 0;   /* no two adjacent free blocks */
        p += s;
    }
    size_t listed = 0, lbytes = 0; w_t prev = 0;
    for (w_t o = h->free_head; o; o = NEXT_OF(h, o)) { assert(!balloc(h->base + o)); assert(PREV_OF(h, o) == prev); prev = o; listed++; lbytes += bsize(h->base + o); assert(listed <= nfree); }
    assert(listed == nfree && lbytes == free_bytes);
    if (largest_free) *largest_free = largest;
    if (nfree_blocks) *nfree_blocks = nfree;
    return free_bytes;
}
#endif
