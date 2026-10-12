#ifndef LOCKFREE_H
#define LOCKFREE_H
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

/* Lock-free structures over a fixed pool of nodes addressed by 32-bit INDEX.  Every reference is a 64-bit word  (tag << 32) | index,
 * where the tag is incremented on each successful CAS.  The tag defeats ABA: a node that left and came back has a different tag, so a stale CAS fails.
 * The pool is TYPE-STABLE (nodes are recycled, never returned to the OS), so a thread that reads a node another thread just popped touches valid memory.
 * That is a deliberately simple answer to the memory-reclamation problem; general solutions are hazard pointers, epochs/RCU, or reference counts. */
#define NIL 0xFFFFFFFFu
typedef uint64_t ref_t;
static inline ref_t mk(uint32_t tag, uint32_t idx) { return ((ref_t)tag << 32) | idx; }
static inline uint32_t idx_of(ref_t r) { return (uint32_t)r; }
static inline uint32_t tag_of(ref_t r) { return (uint32_t)(r >> 32); }

/* `next` is the QUEUE link (tagged; its tag must survive recycling or ABA reappears on that field); `fl` is the free-list / stack link. */
typedef struct { _Atomic ref_t next; _Atomic uint32_t fl; _Atomic uint64_t value; } LNode;

/* ---------- Treiber stack ---------- */
typedef struct { LNode *pool; _Atomic ref_t head; } TStack;
static inline void tstack_push_idx(TStack *s, uint32_t i) {
    ref_t old = atomic_load(&s->head);
    do { atomic_store_explicit(&s->pool[i].fl, idx_of(old), memory_order_relaxed); } while (!atomic_compare_exchange_weak(&s->head, &old, mk(tag_of(old) + 1, i)));
}
static inline uint32_t tstack_pop_idx(TStack *s) {
    ref_t old = atomic_load(&s->head);
    for (;;) {
        if (idx_of(old) == NIL) return NIL;
        uint32_t nxt = atomic_load_explicit(&s->pool[idx_of(old)].fl, memory_order_relaxed);
        if (atomic_compare_exchange_weak(&s->head, &old, mk(tag_of(old) + 1, nxt))) return idx_of(old);   /* on failure `old` is reloaded */
    }
}

/* ---------- Michael-Scott queue (PODC 1996): dummy node, separate head and tail, tail may lag by one and any thread helps it forward ---------- */
typedef struct { LNode *pool; TStack freelist; _Atomic ref_t head, tail; uint32_t cap; } MSQueue;
static void msq_init(MSQueue *q, uint32_t cap) {
    q->cap = cap; q->pool = calloc(cap, sizeof(LNode)); q->freelist.pool = q->pool; atomic_init(&q->freelist.head, mk(0, NIL));
    for (uint32_t i = cap; i-- > 1; ) tstack_push_idx(&q->freelist, i);                 /* node 0 becomes the initial dummy */
    atomic_init(&q->pool[0].next, mk(0, NIL)); atomic_init(&q->head, mk(0, 0)); atomic_init(&q->tail, mk(0, 0));
}
static void msq_destroy(MSQueue *q) { free(q->pool); }
static int msq_enqueue(MSQueue *q, uint64_t v) {
    uint32_t n = tstack_pop_idx(&q->freelist); if (n == NIL) return 0;                  /* pool exhausted */
    atomic_store(&q->pool[n].value, v);
    atomic_store(&q->pool[n].next, mk(tag_of(atomic_load(&q->pool[n].next)) + 1, NIL));       /* bump, never reset, the tag of a recycled node */
    for (;;) {
        ref_t t = atomic_load(&q->tail), nx = atomic_load(&q->pool[idx_of(t)].next);
        if (t != atomic_load(&q->tail)) continue;                                      /* tail moved: retry with a consistent snapshot */
        if (idx_of(nx) == NIL) {                                                       /* tail really is last: try to link the new node after it */
            if (atomic_compare_exchange_weak(&q->pool[idx_of(t)].next, &nx, mk(tag_of(nx) + 1, n))) { atomic_compare_exchange_strong(&q->tail, &t, mk(tag_of(t) + 1, n)); return 1; }
        } else atomic_compare_exchange_strong(&q->tail, &t, mk(tag_of(t) + 1, idx_of(nx)));   /* tail is lagging: help advance it */
    }
}
static int msq_dequeue(MSQueue *q, uint64_t *v) {
    for (;;) {
        ref_t h = atomic_load(&q->head), t = atomic_load(&q->tail), nx = atomic_load(&q->pool[idx_of(h)].next);
        if (h != atomic_load(&q->head)) continue;
        if (idx_of(h) == idx_of(t)) {
            if (idx_of(nx) == NIL) return 0;                                           /* empty */
            atomic_compare_exchange_strong(&q->tail, &t, mk(tag_of(t) + 1, idx_of(nx)));   /* tail lagging behind a linked node: help */
        } else {
            uint64_t val = atomic_load(&q->pool[idx_of(nx)].value);                    /* read BEFORE the CAS: afterwards nx may be recycled */
            if (atomic_compare_exchange_weak(&q->head, &h, mk(tag_of(h) + 1, idx_of(nx)))) { *v = val; tstack_push_idx(&q->freelist, idx_of(h)); return 1; }   /* old dummy goes back to the pool */
        }
    }
}

/* ---------- baseline: unbounded linked queue guarded by one mutex ---------- */
typedef struct MNode { struct MNode *next; uint64_t v; } MNode;
typedef struct { MNode *head, *tail; pthread_mutex_t m; } LockQueue;
static void lq_init(LockQueue *q) { q->head = q->tail = NULL; pthread_mutex_init(&q->m, NULL); }
static void lq_enqueue(LockQueue *q, uint64_t v) { MNode *n = malloc(sizeof *n); n->v = v; n->next = NULL; pthread_mutex_lock(&q->m); if (q->tail) q->tail->next = n; else q->head = n; q->tail = n; pthread_mutex_unlock(&q->m); }
static int lq_dequeue(LockQueue *q, uint64_t *v) { pthread_mutex_lock(&q->m); MNode *n = q->head; if (!n) { pthread_mutex_unlock(&q->m); return 0; } q->head = n->next; if (!q->head) q->tail = NULL; pthread_mutex_unlock(&q->m); *v = n->v; free(n); return 1; }
#endif
