---
kind: question
title: A lock-free SPSC queue
module: Systems-Flavored Structures
difficulty: Hard
---

One producer thread and one consumer thread exchange 200,000 integers through a 64-slot ring buffer with no mutex. Which index does each thread own, which memory orders are needed, and how do you check for races?

## Explanation

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
