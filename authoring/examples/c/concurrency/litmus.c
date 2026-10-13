// Store-buffering litmus test:  T0: x=1; r0=y    T1: y=1; r1=x     Can both threads read 0?
// Sequential consistency says no. Real CPUs (store buffers) say yes unless a full fence is used.
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

#define ROUNDS 2000000

static _Alignas(128) atomic_int x;
static _Alignas(128) atomic_int y;           // separate cache lines
static _Alignas(128) atomic_int go;          // main thread bumps this once per round; both workers spin on it
static _Alignas(128) atomic_int done;
static int r0[ROUNDS], r1[ROUNDS];           // per-round results, examined after the threads finish
static int mo;                               // memory order under test: 0 relaxed, 1 release/acquire, 2 seq_cst

static void store(atomic_int *p, int v) {
    atomic_store_explicit(p, v, mo == 0 ? memory_order_relaxed : mo == 1 ? memory_order_release : memory_order_seq_cst);
}
static int load(atomic_int *p) {
    return atomic_load_explicit(p, mo == 0 ? memory_order_relaxed : mo == 1 ? memory_order_acquire : memory_order_seq_cst);
}
static void *thread(void *arg) {
    int me = *(int *)arg;
    for (int i = 1; i <= ROUNDS; i++) {
        while (atomic_load_explicit(&go, memory_order_acquire) != i) ;   // both spin on the same line
        if (me == 0) { store(&x, 1); r0[i - 1] = load(&y); }
        else         { store(&y, 1); r1[i - 1] = load(&x); }
        atomic_fetch_add(&done, 1);
    }
    return NULL;
}

int main(void) {
    const char *name[] = {"relaxed", "release/acquire", "seq_cst"};
    for (mo = 0; mo < 3; mo++) {
        atomic_store(&go, 0); atomic_store(&done, 0);
        pthread_t a, b; int id0 = 0, id1 = 1;
        pthread_create(&a, NULL, thread, &id0); pthread_create(&b, NULL, thread, &id1);
        for (int i = 1; i <= ROUNDS; i++) {
            atomic_store(&x, 0); atomic_store(&y, 0); atomic_store(&done, 0);
            atomic_store_explicit(&go, i, memory_order_release);        // release both workers
            while (atomic_load(&done) != 2) ;
        }
        pthread_join(a, NULL); pthread_join(b, NULL);
        int both_zero = 0;
        for (int i = 0; i < ROUNDS; i++) both_zero += (r0[i] == 0 && r1[i] == 0);
        printf("%-16s both threads read 0 in %d of %d rounds\n", name[mo], both_zero, ROUNDS);
    }
    return 0;
}
