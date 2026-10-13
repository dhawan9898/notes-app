// Readers follow a shared pointer to a record whose invariant is a + b == 100. The writer replaces the record
// (copy, update, publish) and must not reclaim the old one until a grace period has elapsed.
#include "qsbr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct { long a, b; } rec_t;
static _Atomic(rec_t *) shared;
static atomic_int stop;
static atomic_long violations, reads_total;
static int safe_mode;                                      // 1: wait a grace period before reclaiming; 0: reclaim immediately (WRONG)

static void *reader(void *arg) {
    (void)arg;
    int id = rcu_register_thread();
    long reads = 0, bad = 0;
    while (!atomic_load(&stop)) {
        for (int i = 0; i < 1000; i++) {                  // a batch of read-side critical sections
            rec_t *r = rcu_dereference(shared);
            long a = r->a;
            for (volatile int k = 0; k < 20; k++) ;        // time spent holding the pointer: a window for the writer to misbehave
            long b = r->b;
            if (a + b != 100) bad++;
            reads++;
        }
        rcu_quiescent_state(id);                           // between batches: no pointer is held
    }
    rcu_thread_offline(id);
    atomic_fetch_add(&violations, bad); atomic_fetch_add(&reads_total, reads);
    return NULL;
}

static void *writer(void *arg) {
    (void)arg;
    long v = 0;
    while (!atomic_load(&stop)) {
        rec_t *n = malloc(sizeof *n); n->a = v % 101; n->b = 100 - n->a; v++;    // copy-update
        rec_t *old = shared;
        rcu_assign_pointer(shared, n);                    // publish
        if (safe_mode) synchronize_rcu();                 // wait until no reader can still hold 'old'
        old->a = -1000000; old->b = -1000000;             // poison, then free: a reader still using it would see a + b != 100
        free(old);
        struct timespec ts = {0, 50000}; nanosleep(&ts, NULL);
    }
    return NULL;
}

static void run(int safe) {
    safe_mode = safe; atomic_store(&stop, 0); atomic_store(&violations, 0); atomic_store(&reads_total, 0);
    atomic_store(&nreaders, 0);
    rec_t *first = malloc(sizeof *first); first->a = 40; first->b = 60; atomic_store(&shared, first);
    pthread_t r[4], w;
    for (int i = 0; i < 4; i++) pthread_create(&r[i], NULL, reader, NULL);
    pthread_create(&w, NULL, writer, NULL);
    struct timespec ts = {1, 0}; nanosleep(&ts, NULL);
    atomic_store(&stop, 1);
    for (int i = 0; i < 4; i++) pthread_join(r[i], NULL);
    pthread_join(w, NULL);
    printf("%-34s reads %9ld, invariant violations: %ld\n", safe ? "with synchronize_rcu before free:" : "freeing immediately (no grace period):", atomic_load(&reads_total), atomic_load(&violations));
}

int main(void) { run(1); run(0); return 0; }
