---
title: Concurrent and Lock-Free Structures
track: dsa
module: Systems-Flavored Structures
moduleOrder: 9
order: 3
level: expert
summary: "The ABA problem replayed deterministically, a tagged Treiber stack and a Michael-Scott queue stress-tested under ThreadSanitizer (including the recycling bug the FIFO check caught), and an honest comparison with a mutex queue: lock-free buys progress guarantees, not necessarily speed."
prerequisites:
  - Stacks, Queues, Deques and Ring Buffers
  - Threads and Synchronization
  - Threads, Atomics and the C11 Memory Model
sources:
  - label: "Treiber — Systems Programming, Coping with Parallelism (IBM Research Report RJ 5118, 1986)"
  - label: "Michael & Scott — Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms (PODC 1996)"
  - label: "Herlihy & Wing — Linearizability, A Correctness Condition for Concurrent Objects (ACM TOPLAS 12(3), 1990)"
  - label: "Herlihy — Wait-Free Synchronization (ACM TOPLAS 13(1), 1991)"
  - label: "Michael — Hazard Pointers, Safe Memory Reclamation for Lock-Free Objects (IEEE TPDS 15(6), 2004)"
  - label: "McKenney — Is Parallel Programming Hard, And, If So, What Can You Do About It? (perfbook), Ch. on RCU and memory ordering"
    url: https://mirrors.edge.kernel.org/pub/linux/kernel/people/paulmck/perfbook/perfbook.html
  - label: "ISO/IEC 9899:2011 §7.17 (Atomics) and §5.1.2.4 (Multi-threaded executions and data races)"
  - label: "Herlihy & Shavit — The Art of Multiprocessor Programming, 2nd ed."
---

Shared data structures in a multithreaded program need a rule for who may touch what, and when. There are three families: **lock-based** (a mutex protects the structure), **lock-free** (some thread always makes progress even if others are stalled, using atomic read-modify-write instructions such as compare-and-swap) and **wait-free** (every thread finishes in a bounded number of steps). This note builds the classic lock-free stack and queue, shows how they break, and measures whether they were worth it. Prerequisites: the *Threads and Synchronization* note (mutexes, condition variables, memory model basics) and the SPSC ring buffer in *Stacks, Queues, Deques and Ring Buffers*.

## Correctness first: linearizability and memory ordering

A concurrent object is **linearizable** (Herlihy & Wing) if every operation appears to take effect atomically at a single instant between its invocation and its response, consistent with a legal sequential history. A queue test therefore checks sequential properties on a concurrent run: every item arrives exactly once, and items from one producer keep their order in the queue.

C11 atomics give the tools. `atomic_compare_exchange_*(p, &expected, desired)` stores `desired` only if `*p == expected`, otherwise it loads the current value into `expected`; the `_weak` form may fail spuriously and is meant for retry loops. Memory ordering (`memory_order_relaxed`, `acquire`/`release`, `seq_cst`) controls what other threads may see in which order; the structures below use the default sequentially consistent ordering for the shared pointers for clarity, with relaxed accesses only where a field is protected by a subsequent CAS. Weaker orders can be faster but are far harder to verify — the SPSC queue note shows an acquire/release design.

## The ABA problem

A CAS compares *values*, not history. In a lock-free stack the pop operation reads the head `A` and its successor `B`, then does `CAS(head, A, B)`. If between the read and the CAS other threads pop `A` and `B`, free `B`, and push `A` back, `head` is again `A` and the CAS succeeds — installing `B`, a node that is no longer part of the stack. The value matched; the world did not.

The program replays that interleaving deterministically (no race conditions, no luck needed), so the corruption is reproducible:

```c
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

/* Deterministic replay of the ABA problem on a Treiber stack (lock-free LIFO, pointer version).  We run the "threads" by hand,
 * in the exact interleaving that breaks it, so the result is reproducible rather than a once-a-week crash. */
typedef struct Node { const char *name; struct Node *next; } Node;
static _Atomic(Node *) head;

static void show(const char *label) { printf("%-44s stack:", label); for (Node *n = atomic_load(&head); n; n = n->next) printf(" %s", n->name); printf("\n"); }

int main(void) {
    Node C = { "C", NULL }, B = { "B", &C }, A = { "A", &B };
    atomic_store(&head, &A);
    show("initial");

    /* Thread 1 starts pop(): reads head and head->next, then is descheduled before its compare-and-swap */
    Node *old = atomic_load(&head), *next = old->next;                 /* old = A, next = B */
    printf("thread 1: read head = %s, next = %s, then it is preempted...\n", old->name, next->name);

    /* Thread 2 runs: pop A, pop B (B is now owned by someone else and may be freed or reused), push A back */
    Node *a = atomic_load(&head); atomic_store(&head, a->next);        /* pop A */
    Node *b = atomic_load(&head); atomic_store(&head, b->next);        /* pop B -> B is "freed" */
    a->next = atomic_load(&head); atomic_store(&head, a);              /* push A again; A->next = C */
    show("thread 2: popped A, popped B, pushed A");
    B.next = (Node *)0xDEAD;                                           /* B was recycled: its next now holds garbage */
    printf("thread 2: B has been freed and reused (B.next now garbage)\n");

    /* Thread 1 resumes: CAS(head, A, B) SUCCEEDS because head is again A -- the value A is back, but the world has changed */
    Node *expected = old;
    if (atomic_compare_exchange_strong(&head, &expected, next)) printf("thread 1: CAS(head, A -> B) succeeded!  head is now B, a node that is no longer in the stack, whose next is garbage\n");
    printf("head = %s, head->next = %p  (C was lost; the stack is corrupt)\n\n", atomic_load(&head)->name, (void *)atomic_load(&head)->next);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O1 -g -Wall -Wextra aba.c -o aba && ./aba
initial                                      stack: A B C
thread 1: read head = A, next = B, then it is preempted...
thread 2: popped A, popped B, pushed A       stack: A C
thread 2: B has been freed and reused (B.next now garbage)
thread 1: CAS(head, A -> B) succeeded!  head is now B, a node that is no longer in the stack, whose next is garbage
head = B, head->next = 0xdead  (C was lost; the stack is corrupt)
```

The final stack contains a freed node whose `next` is garbage, and `C` is lost. In real code the failure is intermittent: it needs a precise preemption, usually under load, and appears as a crash or lost data far from the cause.

**Cures** for ABA:

1. **Tagged pointers / version counters**: pair the pointer with a counter incremented on every successful CAS (double-width CAS, or pack an index and a tag into 64 bits as below). The recycled `A` has a different tag, so the stale CAS fails.
2. **Safe memory reclamation**: do not recycle a node while any thread might still hold a reference — *hazard pointers* (threads publish what they are about to dereference), *epoch-based reclamation* and *RCU* (defer frees until all readers have passed a quiescent point; the Linux kernel's workhorse), or reference counting.
3. **Avoid the pattern**: single-producer/single-consumer queues and per-CPU data need no CAS loops at all.

## A tagged Treiber stack and a Michael–Scott queue

The implementation addresses nodes by 32-bit **index** into a fixed pool, packing `(tag << 32) | index` in one 64-bit atomic word. The pool is *type-stable* — nodes are recycled, never returned to the operating system — so a thread that reads a node another thread just removed still reads valid memory. That is a simple, practical answer to the reclamation problem for pool-based designs; general-purpose containers need hazard pointers or epochs.

The **Treiber stack** (1986) is the lock-free LIFO: push links the new node under the current head and CASes the head; pop reads head and its next, CASes the head to next. The **Michael–Scott queue** (1996) is the classic lock-free FIFO and the basis of `java.util.concurrent.ConcurrentLinkedQueue`: a linked list with a permanent dummy node, separate `head` and `tail` pointers, and a **helping** rule — if a thread finds the tail lagging one node behind the true last node, it advances the tail itself before continuing — so that a thread preempted mid-operation cannot block the others. That helping is what makes it lock-free rather than merely "optimistic".

```c
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
```

### A real bug the tests caught

The first version of this code reused one `next` field for both the queue link *and* the free-list link, and on recycling reset the tag to zero (`mk(0, NIL)`). That reintroduced ABA on the node's own `next` field: a stalled enqueuer holding `(tag 0, NIL)` for a tail node could later succeed in its CAS after the node had been dequeued, recycled and reused with the *same* `(tag 0, NIL)` — linking a new item behind a node no longer at the end of the queue. The symptom appeared in the **FIFO check** as per-producer sequence violations, with ThreadSanitizer completely silent, because every access involved was an atomic operation (TSan finds data races, not logic races). The fix is the pair of comments in the header: a separate `fl` field for the free list, and a recycled node's queue tag is **incremented, never reset**.

This is the general lesson of lock-free programming: a design that survives a million-operation stress test can still be wrong, and tools that find data races do not find protocol errors. Test with the properties of the abstract data type (exactly-once delivery, FIFO order), at high thread counts, and then prove the algorithm or use a model checker.

## Stress test and measurement

The test first hammers the Treiber stack with eight threads (each repeatedly popping and pushing nodes of the shared pool) and checks afterwards that every pool node is on the stack exactly once. Then, for the Michael–Scott queue and for a mutex-protected linked queue, producers enqueue numbered items and consumers dequeue them, checking that **every item is delivered exactly once** and that **each consumer sees each producer's items in increasing order** (a necessary consequence of FIFO). It was run under ThreadSanitizer for correctness and in an optimized build for timing.

```c
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lockfree.h"
#include "../../c/perf/timing.h"

enum { MAXT = 16 };
static MSQueue msq; static LockQueue lq; static TStack stack; static LNode *spool;
static int PER, NP, NC; static _Atomic long consumed_total;
static uint8_t *seen[MAXT]; static int seq_violations[MAXT];
#define MAKE(p, s) (((uint64_t)(p) << 32) | (uint32_t)(s))

typedef struct { int id, kind; } Arg;                  /* kind 0 = MS queue, 1 = mutex queue */
static void *producer(void *a) {
    Arg *g = a;
    for (int i = 0; i < PER; i++) {
        if (g->kind == 0) { while (!msq_enqueue(&msq, MAKE(g->id, i))) sched_yield(); } else lq_enqueue(&lq, MAKE(g->id, i));
    }
    return NULL;
}
static void *consumer(void *a) {
    Arg *g = a; int last[MAXT]; for (int p = 0; p < MAXT; p++) last[p] = -1; long bad = 0;
    while (atomic_load(&consumed_total) < (long)NP * PER) {
        uint64_t v; int ok = g->kind == 0 ? msq_dequeue(&msq, &v) : lq_dequeue(&lq, &v);
        if (!ok) continue;
        int p = (int)(v >> 32), s = (int)(uint32_t)v; if (s <= last[p]) bad++;          /* FIFO: per-producer sequence must be increasing for each consumer */
        last[p] = s; seen[p][s]++; atomic_fetch_add(&consumed_total, 1);
    }
    seq_violations[g->id] = (int)bad; return NULL;
}
static double run_queue(int kind, int np, int nc, int per) {
    NP = np; NC = nc; PER = per; atomic_store(&consumed_total, 0);
    for (int p = 0; p < np; p++) { seen[p] = calloc((size_t)per, 1); }
    if (kind == 0) msq_init(&msq, 4096); else lq_init(&lq);
    pthread_t th[2 * MAXT]; Arg args[2 * MAXT]; double t0 = now();
    for (int c = 0; c < nc; c++) { args[c] = (Arg){ c, kind }; pthread_create(&th[c], NULL, consumer, &args[c]); }
    for (int p = 0; p < np; p++) { args[MAXT + p] = (Arg){ p, kind }; pthread_create(&th[MAXT + p], NULL, producer, &args[MAXT + p]); }
    for (int p = 0; p < np; p++) pthread_join(th[MAXT + p], NULL);
    for (int c = 0; c < nc; c++) pthread_join(th[c], NULL);
    double dt = now() - t0;
    for (int p = 0; p < np; p++) { for (int i = 0; i < per; i++) assert(seen[p][i] == 1); free(seen[p]); }          /* every item delivered exactly once */
    for (int c = 0; c < nc; c++) assert(seq_violations[c] == 0);
    if (kind == 0) msq_destroy(&msq);
    return dt;
}

/* Treiber stack conservation test: threads push their own ids and pop concurrently; nothing may be lost or duplicated */
static void *stack_worker(void *a) {
    long id = (long)a; int mine[64]; int n = 0;
    for (int i = 0; i < 200000; i++) {
        if (n < 64 && (i & 1) == 0) { uint32_t x = tstack_pop_idx(&stack); if (x != NIL) mine[n++] = (int)x; }
        else if (n > 0) { tstack_push_idx(&stack, (uint32_t)mine[--n]); }
    }
    while (n > 0) tstack_push_idx(&stack, (uint32_t)mine[--n]);
    (void)id; return NULL;
}

int main(void) {
    int caps = 1024; spool = calloc((size_t)caps, sizeof(LNode)); stack.pool = spool; atomic_init(&stack.head, mk(0, NIL));
    for (uint32_t i = 0; i < (uint32_t)caps; i++) tstack_push_idx(&stack, i);
    pthread_t th[8]; for (long t = 0; t < 8; t++) pthread_create(&th[t], NULL, stack_worker, (void *)t); for (int t = 0; t < 8; t++) pthread_join(th[t], NULL);
    char *present = calloc((size_t)caps, 1); int count = 0; for (uint32_t x; (x = tstack_pop_idx(&stack)) != NIL; ) { assert(!present[x]); present[x] = 1; count++; }
    assert(count == caps);
    printf("Treiber stack (tagged indices): 8 threads x 200,000 push/pop operations over %d nodes; afterwards every node was present exactly once\n\n", caps);
    free(present); free(spool);

    printf("queue stress: each producer enqueues sequence numbers; every item must be dequeued exactly once and, per producer, in FIFO order\n");
    printf("%-26s %12s %12s\n", "queue (producers, consumers)", "ms", "M items/s");
    int cfg[][2] = { { 1, 1 }, { 2, 2 }, { 4, 4 } }; int per = 250000;
    for (int k = 0; k < 2; k++) for (int c = 0; c < 3; c++) {
        double dt = run_queue(k, cfg[c][0], cfg[c][1], per);
        printf("%-18s (%d, %d) %12.0f %12.2f\n", k ? "mutex linked queue" : "Michael-Scott lock-free", cfg[c][0], cfg[c][1], dt * 1e3, (double)per * cfg[c][0] / dt / 1e6);
    }
    return 0;
}
```

```text
$ gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=thread stress.c -o stress_tsan -lpthread && ./stress_tsan | head -3 && gcc -std=gnu11 -O2 -Wall -Wextra stress.c -o stress -lpthread && ./stress
Treiber stack (tagged indices): 8 threads x 200,000 push/pop operations over 1024 nodes; afterwards every node was present exactly once

queue stress: each producer enqueues sequence numbers; every item must be dequeued exactly once and, per producer, in FIFO order
Treiber stack (tagged indices): 8 threads x 200,000 push/pop operations over 1024 nodes; afterwards every node was present exactly once

queue stress: each producer enqueues sequence numbers; every item must be dequeued exactly once and, per producer, in FIFO order
queue (producers, consumers)           ms    M items/s
Michael-Scott lock-free (1, 1)          358         0.70
Michael-Scott lock-free (2, 2)         1462         0.34
Michael-Scott lock-free (4, 4)         4037         0.25
mutex linked queue (1, 1)          179         1.39
mutex linked queue (2, 2)          361         1.38
mutex linked queue (4, 4)         1009         0.99
```

The first lines come from the ThreadSanitizer build, which reported no data race and passed every correctness assertion. The table is the optimized build:

- **The lock-free queue was not faster under contention — it was slower** than a plain mutex-protected queue with 2 and 4 producer/consumer pairs (roughly 2.5–4× slower at 4 pairs across repeated runs), while with a single pair the two were comparable and the ranking changed from run to run. This surprises people and is the most important result here. With a handful of threads a good mutex (a futex in Linux) is cheap, and it lets one thread do a whole operation without retrying; the CAS loops of the lock-free queue contend on the same `head`, `tail` and free-list cache lines, each failed CAS is wasted work, and every operation also touches the shared free list. Consumers here spin when the queue is empty, which makes contention worse for both designs.
- **What lock-freedom buys is a guarantee, not throughput**: no thread is ever blocked by another thread being preempted, descheduled or killed while holding the structure — essential in real-time code, signal handlers, kernels (where sleeping is forbidden), and when a stalled thread must not stall the system. A mutex-based queue can suffer **priority inversion** and **lock convoys** under load.
- Per-operation cost matters more than the label: cache-line contention is the real scalability limit for any single shared head or tail, lock-free or not.

## What performs well in practice

| Situation | Practical choice |
|---|---|
| Low contention, modest rates | A mutex (it is simple, and fast when uncontended) |
| High contention on one structure | **Shard** it: N independent queues/maps hashed by key; per-thread or per-CPU data merged later |
| One producer, one consumer | SPSC ring buffer with acquire/release and cached indices (no CAS) |
| Read-mostly shared data | **RCU** or reader-writer locks / seqlocks; readers take no locks |
| Counters and statistics | per-CPU / per-thread counters summed on read, or atomic add (`fetch_add`) |
| Must not block (interrupt/signal context, real time) | lock-free or wait-free structures |
| Memory reclamation required | epochs / hazard pointers (libraries: crossbeam-epoch, Folly, libcds) or RCU in the kernel |

Inside the Linux kernel, the pervasive tools are per-CPU variables, spinlocks, RCU for read-mostly structures (routing tables, dentry cache — the lock-free LC-trie FIB in the previous tries note is read under RCU), seqlocks for small data read frequently, and lock-free `llist` for producer/consumer handoff. Userspace libraries add concurrent hash maps (sharded or lock-free), work-stealing deques (Chase–Lev) for schedulers, and MPMC bounded queues built from per-slot sequence numbers (Vyukov) which avoid the free list altogether.

## Pitfalls

- **Assuming lock-free means faster.** Measure under your contention level (the data above) and prefer sharding to cleverness.
- **ABA and reclamation**: every pointer-based lock-free structure needs a story for when a node may be reused or freed. "It never crashed in testing" is not one.
- **Weak memory orders without a model**: `relaxed` is only safe where nothing depends on the ordering; an acquire/release pair must connect the publisher and the reader of every non-atomic field.
- **Data races on the payload**: the queue's `value` field is atomic here; a plain field read concurrently with a recycle would be undefined behavior. Run ThreadSanitizer, but remember it does not catch protocol errors.
- **False sharing**: unrelated hot variables on one 64-byte cache line (queue `head` and `tail`, per-thread counters) make cores fight; pad and align them (`alignas(64)`).
- **Busy-waiting**: spinning consumers burn CPU and slow producers; back off, use `sched_yield`, futexes, or a blocking wrapper.
- **Progress terminology**: obstruction-free < lock-free < wait-free; most "lock-free" libraries are lock-free for the structure but may block in the allocator.
- **128-bit CAS availability**: double-width CAS is not available everywhere (it needs `cmpxchg16b` on x86-64, and aarch64 uses LL/SC pairs); packing index and tag into 64 bits, as here, limits the structure to a 2³²-node pool.

## Key takeaways

1. A CAS compares values, not history; the ABA problem is repaired with version tags, safe memory reclamation (hazard pointers, epochs, RCU), or by avoiding node reuse.
2. The Treiber stack and the Michael–Scott queue are the canonical lock-free LIFO and FIFO; the queue's helping step is what keeps it lock-free.
3. Version tags must survive node recycling — resetting them reintroduces ABA, and ThreadSanitizer will not report it.
4. Test concurrent structures against the abstract data type's properties (exactly-once, FIFO), under a race detector and at high thread counts.
5. Lock-free structures guarantee progress, not speed: in this measurement a mutex queue was 2.5–4× faster at four producer/consumer pairs. Shard before you go lock-free.
6. Per-CPU data, SPSC queues, RCU and sharding are what high-performance systems actually use to avoid contention.
