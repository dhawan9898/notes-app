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

```c
#include <stdio.h>
#include <string.h>

#define MAXF 8

/* Count page faults (cache misses) for a reference string under FIFO, LRU and Belady's optimal (OPT) replacement. */
static int simulate(const int *refs, int n, int frames, int policy) {       /* 0 = FIFO, 1 = LRU, 2 = OPT */
    int page[MAXF], stamp[MAXF], used = 0, faults = 0, clock = 0, next_victim = 0;
    for (int i = 0; i < n; i++) {
        int hit = -1;
        for (int f = 0; f < used; f++) if (page[f] == refs[i]) hit = f;
        clock++;
        if (hit >= 0) { if (policy == 1) stamp[hit] = clock; continue; }     /* hit: LRU refreshes the timestamp */
        faults++;
        if (used < frames) { page[used] = refs[i]; stamp[used] = clock; used++; continue; }
        int victim = 0;
        if (policy == 0) { victim = next_victim; next_victim = (next_victim + 1) % frames; }              /* oldest loaded */
        else if (policy == 1) { for (int f = 1; f < frames; f++) if (stamp[f] < stamp[victim]) victim = f; }   /* least recently used */
        else {                                                                                            /* used farthest in the future */
            int far = -1;
            for (int f = 0; f < frames; f++) {
                int when = n;                                                                             /* never used again */
                for (int j = i + 1; j < n; j++) if (refs[j] == page[f]) { when = j; break; }
                if (when > far) { far = when; victim = f; }
            }
        }
        page[victim] = refs[i]; stamp[victim] = clock;
    }
    return faults;
}

int main(void) {
    const int refs[] = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};                   /* the classic sequence that shows Belady's anomaly */
    int n = (int)(sizeof refs / sizeof refs[0]);
    printf("reference string: 1 2 3 4 1 2 5 1 2 3 4 5 (%d references)\n", n);
    printf("%-8s %6s %6s %6s\n", "frames", "FIFO", "LRU", "OPT");
    for (int frames = 3; frames <= 5; frames++)
        printf("%-8d %6d %6d %6d\n", frames, simulate(refs, n, frames, 0), simulate(refs, n, frames, 1), simulate(refs, n, frames, 2));
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p9_cache p9_cache.c && ./p9_cache; rm -f p9_cache
reference string: 1 2 3 4 1 2 5 1 2 3 4 5 (12 references)
frames     FIFO    LRU    OPT
3             9     10      7
4            10      8      6
5             5      5      5
```

Two surprises. First, **FIFO with 4 frames faults more often (10) than with 3 frames (9)**: this is **Belady's anomaly**. More memory should never hurt, but FIFO ignores recency and the extra frame changes *which* page gets evicted at a bad moment. LRU and OPT never show it, because they are **stack algorithms**: the set of pages cached with *n* frames is always a subset of the set with *n+1* frames. Second, with 3 frames LRU (10 misses) is worse than FIFO (9) on this particular string: no policy wins on every access pattern, and this one is crafted to cycle through more pages than the cache holds, the worst case for LRU. **OPT** gives the lower bound (7, 6 and 5 misses): it needs to know the *future* references, so it cannot be implemented online, but it is the benchmark against which real policies are measured; with 5 frames the whole working set fits and all three policies tie at 5 compulsory misses. Real systems approximate LRU cheaply (CLOCK, two-queue, ARC) because exact LRU needs bookkeeping on every hit.

**2.**

```c
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { unsigned char *base; size_t size, used; } Arena;

static int arena_init(Arena *a, size_t size) { a->base = malloc(size); a->size = size; a->used = 0; return a->base != NULL; }

/* Bump allocation: round the offset up to the alignment, hand out the next bytes. No per-object free, no bookkeeping. */
static void *arena_alloc(Arena *a, size_t n, size_t align) {
    size_t off = (a->used + (align - 1)) & ~(align - 1);
    if (off + n > a->size) return NULL;                    /* out of space: the caller decides what to do */
    a->used = off + n;
    return a->base + off;
}
static void arena_reset(Arena *a) { a->used = 0; }          /* frees EVERYTHING at once in O(1) */

typedef struct node { int value; struct node *next; } Node;

int main(void) {
    Arena a;
    if (!arena_init(&a, 4096)) return 1;
    Node *head = NULL;
    for (int i = 1; i <= 100; i++) {                        /* a 100-node list with no malloc call per node and no free loop */
        Node *n = arena_alloc(&a, sizeof *n, _Alignof(Node));
        n->value = i; n->next = head; head = n;
    }
    long sum = 0; for (Node *p = head; p; p = p->next) sum += p->value;
    printf("list sum = %ld using %zu of %zu arena bytes (%zu per node)\n", sum, a.used, a.size, sizeof(Node));

    unsigned char *c = arena_alloc(&a, 1, 1); int *x = arena_alloc(&a, sizeof *x, _Alignof(int));
    printf("after a 1-byte char, the next int is %zu bytes further on and aligned: %s\n", (size_t)((unsigned char *)x - c), ((uintptr_t)x % _Alignof(int)) == 0 ? "yes" : "NO");
    arena_reset(&a);
    printf("after reset: %zu bytes used; the whole list was released in one step\n", a.used);
    free(a.base);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p9_arena p9_arena.c && ./p9_arena; rm -f p9_arena
list sum = 5050 using 1600 of 4096 arena bytes (16 per node)
after a 1-byte char, the next int is 4 bytes further on and aligned: yes
after reset: 0 bytes used; the whole list was released in one step
```

Allocation is **pointer bumping**: round the current offset up to the requested alignment with `(used + (align - 1)) & ~(align - 1)` (valid for powers of two), check that the object fits, advance `used`, and return `base + offset`. That is a handful of instructions with no search and no per-object header, so allocation is O(1) and objects allocated together sit next to each other in memory (excellent cache behaviour). The 100 nodes use 1,600 bytes (16 per node, as `sizeof(Node)` shows) with no `malloc` calls, and `arena_reset` releases all of them in O(1); there is no per-object `free`, so there is nothing to forget. After allocating a single `char`, the next `int` lands 4 bytes later and is correctly aligned (the padding is visible in the offset). Use arenas for data with a **common lifetime**: a request in a server, one compiler pass, a parser's syntax tree, a game frame. Do not use them for objects that need to be freed individually at different times, and never keep a pointer into an arena after reset. The allocator itself must also be sized (or chained) to avoid returning `NULL` unexpectedly.

**3.**

```c
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

#define CAP 64                                             /* a power of two */
#define ITEMS 200000

/* Single-producer single-consumer ring buffer without locks: the producer owns 'tail', the consumer owns 'head'. */
static int buf[CAP];
static atomic_size_t head, tail;

static int push(int v) {
    size_t t = atomic_load_explicit(&tail, memory_order_relaxed);
    if (t - atomic_load_explicit(&head, memory_order_acquire) == CAP) return 0;        /* full */
    buf[t % CAP] = v;
    atomic_store_explicit(&tail, t + 1, memory_order_release);                         /* publish the item after writing it */
    return 1;
}
static int pop(int *v) {
    size_t h = atomic_load_explicit(&head, memory_order_relaxed);
    if (h == atomic_load_explicit(&tail, memory_order_acquire)) return 0;              /* empty */
    *v = buf[h % CAP];
    atomic_store_explicit(&head, h + 1, memory_order_release);                         /* give the slot back */
    return 1;
}

static void *producer(void *arg) { (void)arg; for (int i = 1; i <= ITEMS; i++) while (!push(i)) { } return NULL; }

int main(void) {
    pthread_t p;
    pthread_create(&p, NULL, producer, NULL);
    long sum = 0; int v, expect = 1, in_order = 1;
    for (int got = 0; got < ITEMS; ) if (pop(&v)) { if (v != expect++) in_order = 0; sum += v; got++; }
    pthread_join(p, NULL);
    printf("received %d items through a %d-slot lock-free ring, sum %ld (expected %ld), order preserved: %s\n", ITEMS, CAP, sum, (long)ITEMS * (ITEMS + 1) / 2, in_order ? "yes" : "NO");
    return 0;
}
```

```text
$ gcc -std=gnu17 -O2 -Wall -Wextra -pthread -o p9_spsc p9_spsc.c && ./p9_spsc; rm -f p9_spsc
received 200000 items through a 64-slot lock-free ring, sum 20000100000 (expected 20000100000), order preserved: yes
```

The structure relies on **ownership**: only the producer writes `tail`, only the consumer writes `head`, and each thread only *reads* the other's index. Capacity is a power of two and the indices count up without wrapping (`index % CAP` picks the slot), so "full" is `tail − head == CAP` and "empty" is `head == tail`. The memory orders carry the correctness: the producer writes the slot and then does a **release** store of `tail`; the consumer's **acquire** load of `tail` that sees the new value is therefore guaranteed to also see the slot contents. Likewise the consumer's release store of `head` guarantees the producer sees the slot as free only after it has been read. Relaxed loads of your *own* index are enough, since no one else changes it. All 200,000 values arrive, in order, and the sum matches the formula n(n+1)/2. To verify, build with `-fsanitize=thread`: ThreadSanitizer reports **no data race** for this program (the check printed 0 warnings), and when the release and acquire orders are all weakened to `memory_order_relaxed` it does report a data race on the slot (in `pop`), which shows the orders are doing real work. This design works only for **one** producer and **one** consumer; with several of either you need compare-and-swap loops and must face the ABA problem and memory reclamation, as discussed in *Concurrent and Lock-Free Structures*.

</details>

## Key takeaways

1. Replacement policies are judged by simulation on reference strings; FIFO can exhibit Belady's anomaly, LRU and OPT cannot, and OPT is only a theoretical bound.
2. An arena allocates by bumping an aligned offset and frees everything in one step; use it for objects with a shared lifetime.
3. A single-producer single-consumer ring needs no locks if each index has one writer and the data write is published with release and observed with acquire; confirm with ThreadSanitizer.
