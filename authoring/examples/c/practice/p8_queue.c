#include <pthread.h>
#include <stdio.h>

#define CAP 4
#define ITEMS 12

static int buf[CAP], head, count;
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t not_full = PTHREAD_COND_INITIALIZER, not_empty = PTHREAD_COND_INITIALIZER;

static void put(int v) {
    pthread_mutex_lock(&m);
    while (count == CAP) pthread_cond_wait(&not_full, &m);        /* always re-check in a loop: wakeups can be spurious */
    buf[(head + count) % CAP] = v; count++;
    pthread_cond_signal(&not_empty);
    pthread_mutex_unlock(&m);
}
static int get(void) {
    pthread_mutex_lock(&m);
    while (count == 0) pthread_cond_wait(&not_empty, &m);
    int v = buf[head]; head = (head + 1) % CAP; count--;
    pthread_cond_signal(&not_full);
    pthread_mutex_unlock(&m);
    return v;
}

static void *producer(void *arg) { (void)arg; for (int i = 1; i <= ITEMS; i++) put(i * i); return NULL; }

int main(void) {
    pthread_t p;
    pthread_create(&p, NULL, producer, NULL);
    long sum = 0;
    for (int i = 0; i < ITEMS; i++) sum += get();
    pthread_join(p, NULL);
    printf("consumed %d items through a %d-slot buffer, sum of squares = %ld (expected 650)\n", ITEMS, CAP, sum);
    return 0;
}
