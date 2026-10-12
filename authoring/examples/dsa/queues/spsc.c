#define _GNU_SOURCE
#include <pthread.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../c/perf/timing.h"

/* Single-producer / single-consumer lock-free queue.
   The producer owns `tail`, the consumer owns `head`; each only READS the other's index.
   release on the writer's store publishes the slot contents; acquire on the reader's load makes them visible. */
#define CAP 1024
typedef struct {
    alignas(64) _Atomic size_t head;      /* written by the consumer only */
    alignas(64) _Atomic size_t tail;      /* written by the producer only (own cache line: no false sharing) */
    alignas(64) size_t head_cache;        /* producer's private copy of head: reload only when the queue looks full */
    alignas(64) size_t tail_cache;        /* consumer's private copy of tail: reload only when the queue looks empty */
    alignas(64) uint32_t buf[CAP];
} Spsc;

static int spsc_push(Spsc *q, uint32_t v) {
    size_t t = atomic_load_explicit(&q->tail, memory_order_relaxed);
    if (t - q->head_cache == CAP) {                                   /* looks full: refresh our view of head */
        q->head_cache = atomic_load_explicit(&q->head, memory_order_acquire);
        if (t - q->head_cache == CAP) return 0;
    }
    q->buf[t % CAP] = v;
    atomic_store_explicit(&q->tail, t + 1, memory_order_release);     /* publish the element */
    return 1;
}
static int spsc_pop(Spsc *q, uint32_t *out) {
    size_t h = atomic_load_explicit(&q->head, memory_order_relaxed);
    if (h == q->tail_cache) {                                         /* looks empty: refresh our view of tail */
        q->tail_cache = atomic_load_explicit(&q->tail, memory_order_acquire);
        if (h == q->tail_cache) return 0;
    }
    *out = q->buf[h % CAP];
    atomic_store_explicit(&q->head, h + 1, memory_order_release);     /* free the slot */
    return 1;
}

#define N 20000000u
static Spsc q;
static uint64_t consumer_sum; static unsigned consumer_errors;

static void *producer(void *arg) { (void)arg; for (uint32_t i = 0; i < N; ) if (spsc_push(&q, i)) i++; return NULL; }
static void *consumer(void *arg) {
    (void)arg; uint32_t expect = 0, v;
    while (expect < N) if (spsc_pop(&q, &v)) { if (v != expect) consumer_errors++; consumer_sum += v; expect++; }
    return NULL;
}

/* Baseline: the same hand-off through a mutex-protected ring. */
typedef struct { pthread_mutex_t m; uint32_t buf[CAP]; size_t head, tail; } MutexQ;
static MutexQ mq = { .m = PTHREAD_MUTEX_INITIALIZER };
static uint64_t msum;
static void *mproducer(void *arg) { (void)arg; for (uint32_t i = 0; i < N / 10; ) { pthread_mutex_lock(&mq.m); if (mq.tail - mq.head < CAP) { mq.buf[mq.tail++ % CAP] = i; i++; } pthread_mutex_unlock(&mq.m); } return NULL; }
static void *mconsumer(void *arg) { (void)arg; uint32_t got = 0; while (got < N / 10) { pthread_mutex_lock(&mq.m); if (mq.head != mq.tail) { msum += mq.buf[mq.head++ % CAP]; got++; } pthread_mutex_unlock(&mq.m); } return NULL; }

int main(void) {
    pthread_t p, c;
    double t0 = now();
    pthread_create(&p, NULL, producer, NULL); pthread_create(&c, NULL, consumer, NULL);
    pthread_join(p, NULL); pthread_join(c, NULL);
    double t1 = now();
    uint64_t expect = (uint64_t)N * (N - 1) / 2;
    printf("SPSC lock-free : %u items in %6.1f ms = %5.1f M items/s, order errors=%u, checksum %s\n", N, (t1 - t0) * 1e3, N / (t1 - t0) / 1e6, consumer_errors, consumer_sum == expect ? "ok" : "BAD");
    t0 = now();
    pthread_create(&p, NULL, mproducer, NULL); pthread_create(&c, NULL, mconsumer, NULL);
    pthread_join(p, NULL); pthread_join(c, NULL);
    t1 = now();
    uint64_t e2 = (uint64_t)(N / 10) * (N / 10 - 1) / 2;
    printf("mutex queue    : %u items in %6.1f ms = %5.1f M items/s, checksum %s\n", N / 10, (t1 - t0) * 1e3, N / 10 / (t1 - t0) / 1e6, msum == e2 ? "ok" : "BAD");
    return 0;
}
