#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

/* Append n ints, growing capacity with the given policy; return seconds and count how often the block moved. */
static double fill(size_t n, int geometric, int *moves) {
    double t0 = now();
    int *a = NULL; size_t cap = 0; void *prev = NULL; *moves = 0;
    for (size_t i = 0; i < n; i++) {
        if (i == cap) {
            cap = geometric ? (cap ? cap * 2 : 16) : cap + 1024;     /* x2 vs +1024 */
            int *t = realloc(a, cap * sizeof *a);
            if (!t) exit(1);
            a = t;
            if ((void *)a != prev) (*moves)++;
            prev = a;
        }
        a[i] = (int)i;
    }
    double dt = now() - t0;
    free(a);
    return dt;
}

int main(void) {
    int m;
    for (size_t n = 250000; n <= 1000000; n *= 2) {
        double g = fill(n, 1, &m);
        printf("n=%8zu  doubling: %7.2f ms (moved %2d times)   ", n, g * 1e3, m);
        double c = fill(n, 0, &m);
        printf("+1024: %8.2f ms (moved %4d times)\n", c * 1e3, m);
    }
    return 0;
}
