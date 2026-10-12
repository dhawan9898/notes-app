---
title: Stacks, Queues, Deques and Ring Buffers
track: dsa
module: Linear Structures
moduleOrder: 2
order: 3
level: advanced
summary: LIFO and FIFO structures from first principles — the ring buffer with free-running counters, a lock-free single-producer/single-consumer queue with C11 acquire/release ordering (measured against a mutex queue), and the monotonic stack and deque techniques that solve classic O(n) problems.
sources:
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., §10.1 (Simple array-based data structures: stacks and queues)
  - label: Lamport — Proving the Correctness of Multiprocess Programs (IEEE TSE, 1977); Lamport — Specifying Concurrent Program Modules (the SPSC ring buffer)
  - label: Linux kernel docs — kfifo (include/linux/kfifo.h) and Circular Buffers (Documentation/core-api/circular-buffers.rst)
    url: https://docs.kernel.org/core-api/circular-buffers.html
  - label: Preshing — An Introduction to Lock-Free Programming; Acquire and Release Semantics
    url: https://preshing.com/20120612/an-introduction-to-lock-free-programming/
  - label: ISO/IEC 9899:2011 §7.17 (atomics) and §5.1.2.4 (multi-threaded executions and data races)
  - label: Fischer, Heun — Theoretical and Practical Improvements on the RMQ-Problem (monotonic structures); LeetCode-style references for monotonic stack problems
---

These three structures restrict where you may insert and remove, and the restriction buys both simplicity and speed. A **stack** is last-in-first-out (LIFO); a **queue** is first-in-first-out (FIFO); a **deque** (double-ended queue) allows both ends. They appear everywhere: the call stack, undo history, depth-first search, breadth-first search, task schedulers, network packet queues, device driver rings, producer/consumer pipelines.

| Structure | Operations (all O(1)) | Typical implementation |
|---|---|---|
| Stack | `push`, `pop`, `peek` | dynamic array (top = last element) |
| Queue | `enqueue`, `dequeue`, `peek` | ring buffer (fixed or growing), or linked list with head+tail |
| Deque | push/pop at both ends | ring buffer, or array of blocks |
| Priority queue | insert, extract-min | binary heap (not FIFO: heap note) |

## Stacks

A stack on a dynamic array is trivial: `push` appends, `pop` removes the last element; both O(1) amortized, contiguous and cache-friendly (the linked version only wins when you cannot move elements). Uses:

- **Function calls**: the hardware/OS call stack (return addresses, locals; see the C stack-frames note) — recursion is an implicit stack.
- **DFS** (explicit stack instead of recursion avoids stack overflow on deep graphs), backtracking.
- **Parsing and evaluation**: matching brackets, converting infix to postfix (shunting-yard), evaluating postfix, XML/HTML tag matching.
- **Undo/redo** (two stacks), browser history, text-editor operations.
- **Memory allocation**: stack/arena allocators with mark/release.
- **Monotonic stack** technique (below).

Two programs from `patterns.c` show the classic uses — bracket matching with one stack and expression evaluation with two (values and operators). The shunting-yard algorithm handles precedence (`^` > `* /` > `+ -`) and associativity (`^` right-associative), reducing the operator stack while the top has higher precedence (or equal and the new operator is left-associative):

{{include dsa/queues/patterns.c}}

{{run dsa/queues :: gcc -std=gnu11 -O1 -Wall -Wextra -fsanitize=address,undefined patterns.c -o patterns -lm && ./patterns}}

`3 + 4 * 2 / (1 - 5) ^ 2 ^ 3` evaluates to 3.0001220703 (the textbook answer: `2 ^ 3 = 8`, `(−4)^8 = 65536`, `8/65536 = 0.0001220703`). Bounds on the fixed-size arrays (`st[256]`) are checked for the bracket stack; a production evaluator would validate the input grammar and stack depths (or use a growing stack) because malformed input like `)))` would otherwise underflow.

## Queues and the ring buffer

A queue needs O(1) removal at the front, which an array cannot offer by shifting. The **ring buffer** (circular buffer) solves it: a fixed array treated as circular, with a `head` index (next to remove) and a `tail` index (next free slot). Indices wrap around using modulo, or, when the capacity is a **power of two**, a bit mask (`i & (cap − 1)`), much cheaper than division.

Two ways to distinguish *full* from *empty* (both have `head == tail` if you store wrapped indices):

1. Waste one slot (full when `(tail + 1) % cap == head`).
2. **Free-running counters** (recommended): store `head` and `tail` as unbounded unsigned integers that only increase; the slot is `index & mask`, `count = tail − head` (unsigned subtraction is correct even when the counters wrap past 2⁶⁴, as long as the capacity is a power of two and divides 2⁶⁴), empty is `head == tail`, full is `count == capacity`. No wasted slot, no ambiguity:

{{include dsa/queues/ring.h}}

Because decrementing `head` (push-front) and `tail` (pop-back) are as easy as incrementing, the same structure is a **deque**. A differential test applies 300,000 random deque operations to the ring and to a naive array model, comparing results after each step (counters wrapped around the capacity thousands of times):

{{include dsa/queues/test_ring.c}}

{{run dsa/queues :: gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined test_ring.c -o test_ring && ./test_ring}}

Design choices:

- **Fixed capacity** (embedded systems, network drivers, kernel `kfifo`) gives predictable memory and back-pressure: a full queue tells the producer to wait or drop. **Growable** rings reallocate and re-linearize (copy head..tail to the start of a larger buffer).
- **Overwrite-oldest** mode (a "flight recorder"/log buffer) advances `head` when full.
- **Bytes vs elements**: byte-oriented rings (`kfifo`, pipe buffers, UART/NIC receive rings) must handle wrap-around by splitting `memcpy` into two segments.
- A linked-list queue (head and tail pointers) needs an allocation per element; use it only when elements must stay put or the length is unbounded and rarely large.
- **Two stacks make a queue**: push onto `in`; to dequeue, if `out` is empty pour all of `in` into `out` (reversing order), then pop `out`. Each element moves at most twice, so operations are **amortized O(1)** (an application of the accounting method from the complexity note).
- **Blocking queues** add a mutex and condition variables (`not_empty`, `not_full`) — the producer/consumer from the Linux threads note — and are the right default between threads when throughput requirements are modest.

## Lock-free single-producer / single-consumer queue

When exactly **one** thread produces and exactly **one** consumes (a driver and its interrupt handler, a logging thread, an audio callback, a pipeline stage), a ring buffer needs **no locks and no atomic read-modify-write instructions**: each index is written by only one thread. Correctness rests on memory ordering:

- The producer writes the element, *then* publishes it with a **release** store to `tail`. The consumer's **acquire** load of `tail` that observes the new value is guaranteed to see the element written before it (release/acquire pairing establishes happens-before).
- Symmetrically, the consumer reads the element, then releases the slot with a release store to `head`; the producer's acquire load of `head` sees the slot as free only after the read finished.
- The indices live on **separate cache lines** (`alignas(64)`) so the two cores do not invalidate each other's lines on every write (false sharing; see the C performance note).
- Each side keeps a **cached copy of the other's index** and reloads it only when the queue appears full/empty, so most operations touch no shared line at all.

{{include dsa/queues/spsc.c}}

{{run dsa/queues :: gcc -std=gnu11 -O2 -Wall -Wextra -pthread spsc.c -o spsc && ./spsc}}

Twenty million items passed from one thread to another **in order, with no corruption** (`order errors=0`, checksum matches) at tens of millions of items per second. The mutex-protected queue managed a few million per second on the same machine; the lock-free queue was roughly 5–10× faster here, and noisy because the benchmark runs on a shared virtual machine. An earlier version without the cached indices re-read the other thread's index on every operation and reached only ~2 M items/s — each access bounced a cache line between cores — so the cached-index optimization alone was worth about 10×. ThreadSanitizer, run on a smaller version, reported no data races (the atomics carry the synchronization).

Cautions:

- It is correct only for **one producer and one consumer**. Multiple producers/consumers need CAS loops on the indices or per-slot sequence numbers (Vyukov's bounded MPMC queue) and then suffer ABA/ordering subtleties (concurrency note).
- Spinning when empty or full burns CPU; use backoff (`pause`/`sched_yield`), futex/eventfd wake-ups, or a hybrid.
- Use `memory_order_seq_cst` (the default) first, optimize to acquire/release only when you can justify each ordering; verify with TSan and stress tests, including on ARM/POWER where weaker hardware ordering exposes mistakes x86 hides.
- Ring buffers like this are everywhere in systems code: NIC RX/TX descriptor rings, `io_uring` submission/completion queues, `perf` mmap buffers, `kfifo`, audio/video pipelines, LMAX Disruptor.

## Monotonic stack and deque

A **monotonic stack** keeps its elements in sorted (increasing or decreasing) order by popping elements that can never be the answer. Each element is pushed and popped at most once, so the algorithm is O(n) even though it has a nested `while`. Three standard problems, each verified against brute force on 3,000 random inputs (the run above printed `all equal`):

1. **Next greater element**: scan left to right; while the stack top is smaller than the current value, the current value is the answer for the popped index. The stack holds indices still waiting for an answer, in decreasing value order.
2. **Largest rectangle in a histogram** (`{2,1,5,6,2,3}` → 10): when a bar is popped because a shorter bar arrived, its maximal width extends from the new stack top to the current index; process a sentinel height 0 at the end to flush. The same idea solves "maximal rectangle of 1s in a binary matrix", "trapping rain water", and "stock span" problems.
3. **Sliding window maximum** with a **monotonic deque** of indices: before adding index `i`, pop from the back while `a[back] ≤ a[i]` (they can never be the maximum while `i` is in the window), drop the front if it left the window, and the front is the maximum — O(n) total instead of O(nk). Window minimum, shortest subarray with sum ≥ K, and DP optimizations (monotone queue optimization) use the same structure.

The invariant (elements in the stack are monotone, and every popped element has had its answer determined) is the proof; write it down before coding.

## BFS and queues, DFS and stacks

Graph traversal choice is a data-structure choice: a **queue** gives breadth-first order (levels, shortest paths in unweighted graphs), a **stack** gives depth-first order (cycle detection, topological sort, backtracking). Replacing the queue with a priority queue gives Dijkstra; with a deque (0-1 BFS) handles 0/1 edge weights (graph notes).

## Other queue-like structures worth knowing

| Structure | Behavior | Use |
|---|---|---|
| Priority queue / heap | extract min/max | scheduling, Dijkstra, top-k |
| Bucket / calendar queue, timing wheel | O(1) timer insertion | network timers, `epoll` timeouts, kernel timer wheel |
| Work-stealing deque | owner pushes/pops one end, thieves steal the other | task schedulers (Cilk, Go runtime, Tokio, Java ForkJoin) |
| Skip list / queue with delete | ordered, concurrent | LSM memtables |
| Message queues (POSIX mq, Kafka, RabbitMQ) | durable FIFO between processes/machines | architecture-level queues |
| FIFO + backpressure + drop policies | bounded queue semantics | admission control, `tc` qdiscs, bufferbloat (QoS note) |

## Pitfalls checklist

1. Check `empty` before `pop`/`peek`, `full` before `push` (or define the failure behavior).
2. Power-of-two capacity for mask arithmetic; assert it at initialization.
3. Never compare wrapped indices to decide full/empty; use counters or a separate count.
4. Overflow of the counters is fine for unsigned types — but mixing them with signed arithmetic is not.
5. Align and separate hot indices for concurrent rings; verify with TSan; test on weak-memory hardware.
6. In stacks of pointers or records, free elements on `pop`/`clear`; in rings of non-trivial elements, run destructors/zero secrets.
7. Bound recursion/stack growth on untrusted input (parsers, evaluators).

## Key takeaways

- Stack = LIFO on a dynamic array; queue/deque = ring buffer. Use a power-of-two capacity and free-running unsigned counters: `slot = i & mask`, `count = tail − head`, no wasted slot, no ambiguity.
- A queue can be built from two stacks with amortized O(1) operations; amortized analysis is the proof.
- A single-producer/single-consumer ring needs no locks: release-store the index you own, acquire-load the other's, keep indices on separate cache lines, and cache the remote index. Measured: ~20–35 M items/s lock-free vs a few M/s with a mutex, and 10× lower without index caching; validated order and checksums, and TSan-clean.
- Monotonic stacks/deques solve next-greater, histogram-rectangle and sliding-window-maximum problems in O(n); verify with brute-force comparison on random inputs.
- Stack vs queue is also the DFS vs BFS decision; priority queues (heaps) generalize both.
