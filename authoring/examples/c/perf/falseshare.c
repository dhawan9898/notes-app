#define _GNU_SOURCE
#include <pthread.h>
#include <stdalign.h>
#include <stdio.h>
#include "timing.h"

#define THREADS 4
#define ITERS 50000000L
struct packed_counters { volatile long v[THREADS]; };                                    /* all in one 64-byte cache line */
struct padded_counter  { alignas(64) volatile long v; };                                 /* one cache line each */
static struct packed_counters packed;
static struct padded_counter padded[THREADS];

static void *work_packed(void *arg) { long id = (long)arg; for (long i = 0; i < ITERS; i++) packed.v[id]++; return NULL; }
static void *work_padded(void *arg) { long id = (long)arg; for (long i = 0; i < ITERS; i++) padded[id].v++; return NULL; }

static double run(void *(*fn)(void *)) {
    pthread_t t[THREADS];
    double t0 = now();
    for (long i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, fn, (void *)i);
    for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
    return now() - t0;
}

int main(void) {
    double a = run(work_packed), b = run(work_padded);
    printf("4 threads, counters adjacent in memory : %6.0f ms\n4 threads, one cache line per counter: %6.0f ms  (%.1fx faster, same work)\n", a * 1e3, b * 1e3, a / b);
    return 0;
}
