#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER, B = PTHREAD_MUTEX_INITIALIZER;

static int timed_lock(pthread_mutex_t *m) {            /* give up after 300 ms instead of hanging forever */
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    t.tv_nsec += 300 * 1000 * 1000;
    if (t.tv_nsec >= 1000000000L) { t.tv_sec++; t.tv_nsec -= 1000000000L; }
    return pthread_mutex_timedlock(m, &t);
}

static void *first_takes_a(void *arg) {                /* order A then B */
    (void)arg;
    pthread_mutex_lock(&A); usleep(100000);            /* let the other thread grab B */
    int rc = timed_lock(&B);
    printf("thread 1 holds A, wants B: %s\n", rc == ETIMEDOUT ? "TIMED OUT (this is a deadlock)" : "got it");
    if (rc == 0) pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    return NULL;
}
static void *second_takes_b(void *arg) {               /* the opposite order: B then A */
    (void)arg;
    pthread_mutex_lock(&B); usleep(100000);
    int rc = timed_lock(&A);
    printf("thread 2 holds B, wants A: %s\n", rc == ETIMEDOUT ? "TIMED OUT (this is a deadlock)" : "got it");
    if (rc == 0) pthread_mutex_unlock(&A);
    pthread_mutex_unlock(&B);
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, first_takes_a, NULL); pthread_create(&t2, NULL, second_takes_b, NULL);
    pthread_join(t1, NULL); pthread_join(t2, NULL);
    return 0;
}
