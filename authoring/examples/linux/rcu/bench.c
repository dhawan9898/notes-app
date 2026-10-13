// Read-side cost: 4 reader threads and 1 writer updating every 50 microseconds, for 1 second each, under three schemes.
#include "qsbr.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct { long a, b; } rec_t;
static _Atomic(rec_t *) shared;
static rec_t plain = {40, 60};                          // used by the lock-based variants
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;
static atomic_int stop, mode;
static atomic_long total_reads;

static void *reader(void *arg) {
    (void)arg;
    int id = rcu_register_thread(); long reads = 0, sink = 0;
    while (!atomic_load(&stop)) {
        for (int i = 0; i < 1000; i++) {
            switch (mode) {
            case 0: pthread_mutex_lock(&mu); sink += plain.a + plain.b; pthread_mutex_unlock(&mu); break;
            case 1: pthread_rwlock_rdlock(&rw); sink += plain.a + plain.b; pthread_rwlock_unlock(&rw); break;
            default: { rec_t *r = rcu_dereference(shared); sink += r->a + r->b; }
            }
            reads++;
        }
        rcu_quiescent_state(id);
    }
    rcu_thread_offline(id);
    atomic_fetch_add(&total_reads, reads + (sink == 12345));
    return NULL;
}
static void *writer(void *arg) {
    (void)arg; long v = 0;
    while (!atomic_load(&stop)) {
        long a = v++ % 101;
        switch (mode) {
        case 0: pthread_mutex_lock(&mu); plain.a = a; plain.b = 100 - a; pthread_mutex_unlock(&mu); break;
        case 1: pthread_rwlock_wrlock(&rw); plain.a = a; plain.b = 100 - a; pthread_rwlock_unlock(&rw); break;
        default: { rec_t *n = malloc(sizeof *n); n->a = a; n->b = 100 - a; rec_t *old = shared; rcu_assign_pointer(shared, n); synchronize_rcu(); free(old); }
        }
        struct timespec ts = {0, 50000}; nanosleep(&ts, NULL);
    }
    return NULL;
}
int main(void) {
    const char *name[] = {"pthread mutex", "pthread rwlock", "RCU (QSBR)"};
    double base = 0;
    for (int m = 0; m < 3; m++) {
        mode = m; atomic_store(&stop, 0); atomic_store(&total_reads, 0); atomic_store(&nreaders, 0);
        rec_t *f = malloc(sizeof *f); f->a = 40; f->b = 60; atomic_store(&shared, f);
        pthread_t r[4], w;
        for (int i = 0; i < 4; i++) pthread_create(&r[i], NULL, reader, NULL);
        pthread_create(&w, NULL, writer, NULL);
        struct timespec ts = {1, 0}; nanosleep(&ts, NULL); atomic_store(&stop, 1);
        for (int i = 0; i < 4; i++) pthread_join(r[i], NULL);
        pthread_join(w, NULL);
        double rate = atomic_load(&total_reads) / 1e6; if (m == 0) base = rate;
        printf("%-16s %8.1f million reads/s   (%.1fx the mutex)\n", name[m], rate, rate / base);
    }
    return 0;
}
