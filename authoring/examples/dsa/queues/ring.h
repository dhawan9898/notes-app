#ifndef RING_H
#define RING_H
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Fixed-capacity ring buffer (circular queue). Capacity is a power of two so wrap-around is a single AND.
   head and tail are free-running counters: the empty/full tests need no wasted slot. */
typedef struct {
    int    *buf;
    size_t  mask;            /* capacity - 1 */
    size_t  head, tail;      /* pop at head, push at tail; count = tail - head (unsigned wrap is well-defined) */
} Ring;

static inline int ring_init(Ring *r, size_t capacity_pow2) {
    if (capacity_pow2 == 0 || (capacity_pow2 & (capacity_pow2 - 1))) return -1;
    r->buf = malloc(capacity_pow2 * sizeof *r->buf);
    if (!r->buf) return -1;
    r->mask = capacity_pow2 - 1; r->head = r->tail = 0;
    return 0;
}
static inline void   ring_free(Ring *r) { free(r->buf); r->buf = NULL; }
static inline size_t ring_len(const Ring *r)  { return r->tail - r->head; }
static inline bool   ring_empty(const Ring *r) { return r->head == r->tail; }
static inline bool   ring_full(const Ring *r)  { return ring_len(r) == r->mask + 1; }
static inline bool   ring_push(Ring *r, int v) { if (ring_full(r)) return false; r->buf[r->tail++ & r->mask] = v; return true; }
static inline bool   ring_pop(Ring *r, int *out) { if (ring_empty(r)) return false; *out = r->buf[r->head++ & r->mask]; return true; }
/* Deque operations come for free: push at the head by decrementing it. */
static inline bool   ring_push_front(Ring *r, int v) { if (ring_full(r)) return false; r->buf[--r->head & r->mask] = v; return true; }
static inline bool   ring_pop_back(Ring *r, int *out) { if (ring_empty(r)) return false; *out = r->buf[--r->tail & r->mask]; return true; }
#endif
