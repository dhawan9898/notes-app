// Message passing: the producer fills a buffer, then publishes a flag with a release store;
// the consumer spins on the flag with an acquire load, then reads the buffer.
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

static int data[4];
static atomic_int ready;

static void *producer(void *a) {
    (void)a;
    for (int i = 0; i < 4; i++) data[i] = 100 + i;                 // plain writes
    atomic_store_explicit(&ready, 1, memory_order_release);        // publishes everything above
    return NULL;
}
static void *consumer(void *a) {
    (void)a;
    while (!atomic_load_explicit(&ready, memory_order_acquire)) ;  // pairs with the release store
    printf("consumer sees %d %d %d %d\n", data[0], data[1], data[2], data[3]);
    return NULL;
}
int main(void) {
    pthread_t p, c;
    pthread_create(&c, NULL, consumer, NULL);
    pthread_create(&p, NULL, producer, NULL);
    pthread_join(p, NULL); pthread_join(c, NULL);
    return 0;
}
