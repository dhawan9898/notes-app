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
