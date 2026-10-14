#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

#define THREADS 4
#define ITERS 200000

static long plain_counter;                       /* shared, unprotected */
static long locked_counter;                      /* protected by a mutex */
static atomic_long atomic_counter;               /* updated with atomic instructions */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void *work(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERS; i++) {
        plain_counter++;                         /* data race: load, add, store can interleave between threads */
        pthread_mutex_lock(&lock); locked_counter++; pthread_mutex_unlock(&lock);
        atomic_fetch_add(&atomic_counter, 1);
    }
    return NULL;
}

int main(void) {
    pthread_t t[THREADS];
    for (int i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, work, NULL);
    for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
    long expected = (long)THREADS * ITERS;
    printf("expected      %ld\n", expected);
    printf("mutex         %ld %s\n", locked_counter, locked_counter == expected ? "(correct)" : "(WRONG)");
    printf("atomic        %ld %s\n", (long)atomic_counter, atomic_counter == expected ? "(correct)" : "(WRONG)");
    printf("unprotected   %ld %s\n", plain_counter, plain_counter == expected ? "(correct by luck this time)" : "(WRONG: updates were lost; the number differs on every run)");
    return 0;
}
