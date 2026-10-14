#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER, B = PTHREAD_MUTEX_INITIALIZER;

static void *ordered(void *arg) {                      /* both threads take A then B: a global lock order */
    (void)arg;
    pthread_mutex_lock(&A); usleep(1000); pthread_mutex_lock(&B);
    pthread_mutex_unlock(&B); pthread_mutex_unlock(&A);
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, ordered, NULL); pthread_create(&t2, NULL, ordered, NULL);
    pthread_join(t1, NULL); pthread_join(t2, NULL);
    printf("both threads finished: a fixed lock order (A before B) cannot deadlock\n");
    return 0;
}
