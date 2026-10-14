#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* How to time code honestly: repeat, use a monotonic clock, keep the best of several runs, and use the result so the
   compiler cannot delete the work. */
static double seconds(struct timespec a, struct timespec b) { return (double)(b.tv_sec - a.tv_sec) + (double)(b.tv_nsec - a.tv_nsec) / 1e9; }

static long sum_by_loop(const int *a, long n) { long s = 0; for (long i = 0; i < n; i++) s += a[i]; return s; }

static double best_time(const int *a, long n, long *result) {
    double best = 1e9;
    for (int run = 0; run < 7; run++) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        *result = sum_by_loop(a, n);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double s = seconds(t0, t1);
        if (s < best) best = s;                              /* the minimum is the least disturbed by other activity */
    }
    return best;
}

int main(void) {
    long n = 20 * 1000 * 1000;
    int *a = malloc((size_t)n * sizeof *a);
    if (!a) return 1;
    for (long i = 0; i < n; i++) a[i] = (int)(i % 7);

    long r1 = 0, r2 = 0;
    double t1 = best_time(a, n / 2, &r1), t2 = best_time(a, n, &r2);
    printf("sum of %ld ints = %ld, sum of %ld ints = %ld\n", n / 2, r1, n, r2);
    printf("best time for n/2: %.1f ms, for n: %.1f ms, ratio %.1f (a linear loop should give about 2)\n", t1 * 1e3, t2 * 1e3, t2 / t1);
    free(a);
    return 0;
}
