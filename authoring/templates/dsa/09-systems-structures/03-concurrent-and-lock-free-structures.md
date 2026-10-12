---
title: Concurrent and Lock-Free Structures
track: dsa
module: Systems-Flavored Structures
moduleOrder: 9
order: 3
level: expert
summary: "The ABA problem replayed deterministically, a tagged Treiber stack and a Michael-Scott queue stress-tested under ThreadSanitizer (including the recycling bug the FIFO check caught), and an honest comparison with a mutex queue: lock-free buys progress guarantees, not necessarily speed."
sources:
  - label: Treiber — Systems Programming, Coping with Parallelism (IBM Research Report RJ 5118, 1986)
  - label: Michael & Scott — Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms (PODC 1996)
  - label: Herlihy & Wing — Linearizability, A Correctness Condition for Concurrent Objects (ACM TOPLAS 12(3), 1990)
  - label: Herlihy — Wait-Free Synchronization (ACM TOPLAS 13(1), 1991)
  - label: Michael — Hazard Pointers, Safe Memory Reclamation for Lock-Free Objects (IEEE TPDS 15(6), 2004)
  - label: McKenney — Is Parallel Programming Hard, And, If So, What Can You Do About It? (perfbook), Ch. on RCU and memory ordering
    url: https://mirrors.edge.kernel.org/pub/linux/kernel/people/paulmck/perfbook/perfbook.html
  - label: ISO/IEC 9899:2011 §7.17 (Atomics) and §5.1.2.4 (Multi-threaded executions and data races)
  - label: Herlihy & Shavit — The Art of Multiprocessor Programming, 2nd ed.
---

Shared data structures in a multithreaded program need a rule for who may touch what, and when. There are three families: **lock-based** (a mutex protects the structure), **lock-free** (some thread always makes progress even if others are stalled, using atomic read-modify-write instructions such as compare-and-swap) and **wait-free** (every thread finishes in a bounded number of steps). This note builds the classic lock-free stack and queue, shows how they break, and measures whether they were worth it. Prerequisites: the *Threads and Synchronization* note (mutexes, condition variables, memory model basics) and the SPSC ring buffer in *Stacks, Queues, Deques and Ring Buffers*.

## Correctness first: linearizability and memory ordering

A concurrent object is **linearizable** (Herlihy & Wing) if every operation appears to take effect atomically at a single instant between its invocation and its response, consistent with a legal sequential history. A queue test therefore checks sequential properties on a concurrent run: every item arrives exactly once, and items from one producer keep their order in the queue.

C11 atomics give the tools. `atomic_compare_exchange_*(p, &expected, desired)` stores `desired` only if `*p == expected`, otherwise it loads the current value into `expected`; the `_weak` form may fail spuriously and is meant for retry loops. Memory ordering (`memory_order_relaxed`, `acquire`/`release`, `seq_cst`) controls what other threads may see in which order; the structures below use the default sequentially consistent ordering for the shared pointers for clarity, with relaxed accesses only where a field is protected by a subsequent CAS. Weaker orders can be faster but are far harder to verify — the SPSC queue note shows an acquire/release design.

## The ABA problem

A CAS compares *values*, not history. In a lock-free stack the pop operation reads the head `A` and its successor `B`, then does `CAS(head, A, B)`. If between the read and the CAS other threads pop `A` and `B`, free `B`, and push `A` back, `head` is again `A` and the CAS succeeds — installing `B`, a node that is no longer part of the stack. The value matched; the world did not.

The program replays that interleaving deterministically (no race conditions, no luck needed), so the corruption is reproducible:

{{include dsa/concurrent/aba.c}}

{{run dsa/concurrent :: gcc -std=gnu11 -O1 -g -Wall -Wextra aba.c -o aba && ./aba}}

The final stack contains a freed node whose `next` is garbage, and `C` is lost. In real code the failure is intermittent: it needs a precise preemption, usually under load, and appears as a crash or lost data far from the cause.

**Cures** for ABA:

1. **Tagged pointers / version counters**: pair the pointer with a counter incremented on every successful CAS (double-width CAS, or pack an index and a tag into 64 bits as below). The recycled `A` has a different tag, so the stale CAS fails.
2. **Safe memory reclamation**: do not recycle a node while any thread might still hold a reference — *hazard pointers* (threads publish what they are about to dereference), *epoch-based reclamation* and *RCU* (defer frees until all readers have passed a quiescent point; the Linux kernel's workhorse), or reference counting.
3. **Avoid the pattern**: single-producer/single-consumer queues and per-CPU data need no CAS loops at all.

## A tagged Treiber stack and a Michael–Scott queue

The implementation addresses nodes by 32-bit **index** into a fixed pool, packing `(tag << 32) | index` in one 64-bit atomic word. The pool is *type-stable* — nodes are recycled, never returned to the operating system — so a thread that reads a node another thread just removed still reads valid memory. That is a simple, practical answer to the reclamation problem for pool-based designs; general-purpose containers need hazard pointers or epochs.

The **Treiber stack** (1986) is the lock-free LIFO: push links the new node under the current head and CASes the head; pop reads head and its next, CASes the head to next. The **Michael–Scott queue** (1996) is the classic lock-free FIFO and the basis of `java.util.concurrent.ConcurrentLinkedQueue`: a linked list with a permanent dummy node, separate `head` and `tail` pointers, and a **helping** rule — if a thread finds the tail lagging one node behind the true last node, it advances the tail itself before continuing — so that a thread preempted mid-operation cannot block the others. That helping is what makes it lock-free rather than merely "optimistic".

{{include dsa/concurrent/lockfree.h}}

### A real bug the tests caught

The first version of this code reused one `next` field for both the queue link *and* the free-list link, and on recycling reset the tag to zero (`mk(0, NIL)`). That reintroduced ABA on the node's own `next` field: a stalled enqueuer holding `(tag 0, NIL)` for a tail node could later succeed in its CAS after the node had been dequeued, recycled and reused with the *same* `(tag 0, NIL)` — linking a new item behind a node no longer at the end of the queue. The symptom appeared in the **FIFO check** as per-producer sequence violations, with ThreadSanitizer completely silent, because every access involved was an atomic operation (TSan finds data races, not logic races). The fix is the pair of comments in the header: a separate `fl` field for the free list, and a recycled node's queue tag is **incremented, never reset**.

This is the general lesson of lock-free programming: a design that survives a million-operation stress test can still be wrong, and tools that find data races do not find protocol errors. Test with the properties of the abstract data type (exactly-once delivery, FIFO order), at high thread counts, and then prove the algorithm or use a model checker.

## Stress test and measurement

The test first hammers the Treiber stack with eight threads (each repeatedly popping and pushing nodes of the shared pool) and checks afterwards that every pool node is on the stack exactly once. Then, for the Michael–Scott queue and for a mutex-protected linked queue, producers enqueue numbered items and consumers dequeue them, checking that **every item is delivered exactly once** and that **each consumer sees each producer's items in increasing order** (a necessary consequence of FIFO). It was run under ThreadSanitizer for correctness and in an optimized build for timing.

{{include dsa/concurrent/stress.c}}

{{run dsa/concurrent :: gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=thread stress.c -o stress_tsan -lpthread && ./stress_tsan | head -3 && gcc -std=gnu11 -O2 -Wall -Wextra stress.c -o stress -lpthread && ./stress}}

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
