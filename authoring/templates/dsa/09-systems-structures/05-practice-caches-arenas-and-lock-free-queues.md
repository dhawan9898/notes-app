---
title: "Practice: Caches, Arenas and a Lock-Free Queue"
track: dsa
module: Systems-Flavored Structures
moduleOrder: 9
order: 5
level: expert
summary: "Three exercises on the systems-structures module: simulating FIFO, LRU and optimal cache replacement and reproducing Belady's anomaly, building an arena allocator with alignment and O(1) reset, and writing a single-producer single-consumer lock-free ring buffer checked with ThreadSanitizer, each with a verified C answer."
prerequisites:
  - "Caches: LRU, LFU, CLOCK and Belady's OPT"
  - "Allocators as Data Structures"
  - "Concurrent and Lock-Free Structures"
sources:
  - label: Belady, L. A. — A study of replacement algorithms for a virtual-storage computer (IBM Systems Journal, 1966)
  - label: Belady, Nelson, Shedler — An anomaly in space-time characteristics of certain programs running in a paging machine (CACM, 1969)
  - label: Herlihy & Shavit — The Art of Multiprocessor Programming, 2nd ed., Ch. 3 and 10
  - label: Hanson — C Interfaces and Implementations, Ch. 6 (Memory management: arenas)
---

These three structures turn up in operating systems, databases and servers. Predict first, then simulate: replacement-policy results in particular are not what intuition says.

## Exercises

**1. Replacement policies.** For the reference string `1 2 3 4 1 2 5 1 2 3 4 5` count the cache misses (page faults) of **FIFO**, **LRU** and Belady's **OPT** (evict the item used farthest in the future) with 3, 4 and 5 frames. Which policy gets *worse* when you add a frame, and what is that called? Why is OPT not implementable in practice?

**2. An arena allocator.** Implement `arena_alloc(arena, size, alignment)` and `arena_reset`. Build a 100-node linked list from the arena without calling `malloc` per node, then release it all. How do you align the returned pointer, and when is an arena the right tool?

**3. A lock-free SPSC queue.** One producer thread and one consumer thread exchange 200,000 integers through a 64-slot ring buffer with no mutex. Which index does each thread own, which memory orders are needed, and how do you check for races?

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

{{include dsa/practice/p9_cache.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p9_cache p9_cache.c && ./p9_cache; rm -f p9_cache}}

Two surprises. First, **FIFO with 4 frames faults more often (10) than with 3 frames (9)**: this is **Belady's anomaly**. More memory should never hurt, but FIFO ignores recency and the extra frame changes *which* page gets evicted at a bad moment. LRU and OPT never show it, because they are **stack algorithms**: the set of pages cached with *n* frames is always a subset of the set with *n+1* frames. Second, with 3 frames LRU (10 misses) is worse than FIFO (9) on this particular string: no policy wins on every access pattern, and this one is crafted to cycle through more pages than the cache holds, the worst case for LRU. **OPT** gives the lower bound (7, 6 and 5 misses): it needs to know the *future* references, so it cannot be implemented online, but it is the benchmark against which real policies are measured; with 5 frames the whole working set fits and all three policies tie at 5 compulsory misses. Real systems approximate LRU cheaply (CLOCK, two-queue, ARC) because exact LRU needs bookkeeping on every hit.

**2.**

{{include dsa/practice/p9_arena.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p9_arena p9_arena.c && ./p9_arena; rm -f p9_arena}}

Allocation is **pointer bumping**: round the current offset up to the requested alignment with `(used + (align - 1)) & ~(align - 1)` (valid for powers of two), check that the object fits, advance `used`, and return `base + offset`. That is a handful of instructions with no search and no per-object header, so allocation is O(1) and objects allocated together sit next to each other in memory (excellent cache behaviour). The 100 nodes use 1,600 bytes (16 per node, as `sizeof(Node)` shows) with no `malloc` calls, and `arena_reset` releases all of them in O(1); there is no per-object `free`, so there is nothing to forget. After allocating a single `char`, the next `int` lands 4 bytes later and is correctly aligned (the padding is visible in the offset). Use arenas for data with a **common lifetime**: a request in a server, one compiler pass, a parser's syntax tree, a game frame. Do not use them for objects that need to be freed individually at different times, and never keep a pointer into an arena after reset. The allocator itself must also be sized (or chained) to avoid returning `NULL` unexpectedly.

**3.**

{{include dsa/practice/p9_spsc.c}}

{{run dsa/practice :: gcc -std=gnu17 -O2 -Wall -Wextra -pthread -o p9_spsc p9_spsc.c && ./p9_spsc; rm -f p9_spsc}}

The structure relies on **ownership**: only the producer writes `tail`, only the consumer writes `head`, and each thread only *reads* the other's index. Capacity is a power of two and the indices count up without wrapping (`index % CAP` picks the slot), so "full" is `tail − head == CAP` and "empty" is `head == tail`. The memory orders carry the correctness: the producer writes the slot and then does a **release** store of `tail`; the consumer's **acquire** load of `tail` that sees the new value is therefore guaranteed to also see the slot contents. Likewise the consumer's release store of `head` guarantees the producer sees the slot as free only after it has been read. Relaxed loads of your *own* index are enough, since no one else changes it. All 200,000 values arrive, in order, and the sum matches the formula n(n+1)/2. To verify, build with `-fsanitize=thread`: ThreadSanitizer reports **no data race** for this program (the check printed 0 warnings), and when the release and acquire orders are all weakened to `memory_order_relaxed` it does report a data race on the slot (in `pop`), which shows the orders are doing real work. This design works only for **one** producer and **one** consumer; with several of either you need compare-and-swap loops and must face the ABA problem and memory reclamation, as discussed in *Concurrent and Lock-Free Structures*.

</details>

## Key takeaways

1. Replacement policies are judged by simulation on reference strings; FIFO can exhibit Belady's anomaly, LRU and OPT cannot, and OPT is only a theoretical bound.
2. An arena allocates by bumping an aligned offset and frees everything in one step; use it for objects with a shared lifetime.
3. A single-producer single-consumer ring needs no locks if each index has one writer and the data write is published with release and observed with acquire; confirm with ThreadSanitizer.
