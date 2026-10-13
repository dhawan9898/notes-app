// A test-and-set spinlock built from C11 atomics, used by 4 threads.
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

static atomic_flag lock = ATOMIC_FLAG_INIT;
static long counter;                       // protected by the spinlock: an ordinary variable

static void spin_lock(void) {
    while (atomic_flag_test_and_set_explicit(&lock, memory_order_acquire))
        ;                                  // production code adds a CPU pause hint and backoff
}
static void spin_unlock(void) { atomic_flag_clear_explicit(&lock, memory_order_release); }

static void *work(void *a) {
    (void)a;
    for (int i = 0; i < 500000; i++) { spin_lock(); counter++; spin_unlock(); }
    return NULL;
}
int main(void) {
    pthread_t t[4];
    for (int i = 0; i < 4; i++) pthread_create(&t[i], NULL, work, NULL);
    for (int i = 0; i < 4; i++) pthread_join(t[i], NULL);
    printf("counter = %ld (expected %d)\n", counter, 4 * 500000);
    return 0;
}
