// Wake-up latency: sleep until an absolute time every 1 ms and record how late the thread actually runs (like cyclictest).
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <time.h>
#include <errno.h>

#define N 3000
static int cmp(const void *a, const void *b) { long x = *(const long *)a, y = *(const long *)b; return (x > y) - (x < y); }

int main(int argc, char **argv) {
    int slack_ns = -1, fifo = 0, lock = 0, cpu = -1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--slack")) slack_ns = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fifo")) fifo = 1;
        else if (!strcmp(argv[i], "--mlock")) lock = 1;
        else if (!strcmp(argv[i], "--cpu")) cpu = atoi(argv[++i]);
    }
    if (slack_ns >= 0) prctl(PR_SET_TIMERSLACK, slack_ns);
    if (cpu >= 0) { cpu_set_t s; CPU_ZERO(&s); CPU_SET(cpu, &s); sched_setaffinity(0, sizeof s, &s); }
    if (lock && mlockall(MCL_CURRENT | MCL_FUTURE) != 0) printf("mlockall: %s\n", strerror(errno));
    if (fifo) { struct sched_param p = { .sched_priority = 50 }; if (sched_setscheduler(0, SCHED_FIFO, &p) != 0) printf("SCHED_FIFO refused: %s\n", strerror(errno)); }
    static long late[N];
    struct timespec next; clock_gettime(CLOCK_MONOTONIC, &next);
    for (int i = 0; i < N; i++) {
        next.tv_nsec += 1000000; if (next.tv_nsec >= 1000000000) { next.tv_nsec -= 1000000000; next.tv_sec++; }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        late[i] = (now.tv_sec - next.tv_sec) * 1000000000L + (now.tv_nsec - next.tv_nsec);
    }
    qsort(late, N, sizeof *late, cmp);
    printf("wake-up latency over %d wakeups (us): min %ld  median %ld  p99 %ld  max %ld\n", N, late[0] / 1000, late[N / 2] / 1000, late[N * 99 / 100] / 1000, late[N - 1] / 1000);
    return 0;
}
