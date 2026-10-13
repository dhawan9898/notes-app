// Lost updates: plain int vs C11 atomic vs mutex, 4 threads x 1,000,000 increments.
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

#define THREADS 4
#define N 1000000

static int plain;                       // data race: undefined behaviour in C11
static atomic_int atom;
static int locked;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;

static void *work(void *arg) {
    int mode = *(int *)arg;
    for (int i = 0; i < N; i++) {
        if (mode == 0) { *(volatile int *)&plain += 1; }   // volatile stops the optimizer folding the loop; it does NOT make it atomic
        else if (mode == 1) atomic_fetch_add_explicit(&atom, 1, memory_order_relaxed);
        else { pthread_mutex_lock(&mu); locked++; pthread_mutex_unlock(&mu); }
    }
    return NULL;
}

int main(void) {
    const char *name[] = {"plain int (racy)", "atomic_fetch_add", "mutex"};
    for (int mode = 0; mode < 3; mode++) {
        pthread_t t[THREADS];
        for (int i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, work, &mode);
        for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
        int v = mode == 0 ? plain : mode == 1 ? atomic_load(&atom) : locked;
        printf("%-18s result %7d (expected %d)\n", name[mode], v, THREADS * N);
    }
    return 0;
}
