#include <stdatomic.h>
#include <stdio.h>
#include <threads.h>

#define N_THREADS 4
#define ITERS 500000

static _Atomic long atomic_counter;
static long plain_counter;
static long locked_counter;
static mtx_t lock;
static once_flag once = ONCE_FLAG_INIT;
static thread_local int tls_calls;                     /* C23 spelling; _Thread_local in C11 */

static void init_once(void) { puts("init_once ran exactly once"); }

static int worker(void *arg) {
    (void)arg;
    call_once(&once, init_once);
    for (int i = 0; i < ITERS; i++) {
        atomic_fetch_add_explicit(&atomic_counter, 1, memory_order_relaxed);
        plain_counter++;                                /* data race on purpose */
        mtx_lock(&lock); locked_counter++; mtx_unlock(&lock);
        tls_calls++;
    }
    return tls_calls;                                   /* each thread has its own copy */
}

int main(void) {
    mtx_init(&lock, mtx_plain);
    thrd_t t[N_THREADS];
    for (int i = 0; i < N_THREADS; i++) thrd_create(&t[i], worker, NULL);
    int sum_tls = 0;
    for (int i = 0; i < N_THREADS; i++) { int r; thrd_join(t[i], &r); sum_tls += r; }
    printf("expected %d\natomic  %ld\nmutex   %ld\nplain   %ld (data race: may lose updates)\nsum of thread-local counts %d\n",
           N_THREADS * ITERS, (long)atomic_counter, locked_counter, plain_counter, sum_tls);
    mtx_destroy(&lock);
    return 0;
}
