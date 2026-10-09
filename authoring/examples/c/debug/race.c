#include <pthread.h>
#include <stdio.h>
static int counter;
static void *inc(void *arg) { (void)arg; for (int i = 0; i < 100000; i++) counter++; return NULL; }
int main(void) {
    pthread_t a, b;
    pthread_create(&a, NULL, inc, NULL);
    pthread_create(&b, NULL, inc, NULL);
    pthread_join(a, NULL); pthread_join(b, NULL);
    printf("counter=%d\n", counter);
    return 0;
}
