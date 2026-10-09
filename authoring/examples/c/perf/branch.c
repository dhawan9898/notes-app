#include <stdio.h>
#include <stdlib.h>
#include "timing.h"

static int cmp(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

int main(void) {
    enum { N = 1 << 24 };
    int *v = malloc(N * sizeof *v);
    srand(1);
    for (int i = 0; i < N; i++) v[i] = rand() % 256;

    long s; double t0, t1;
    t0 = now(); s = 0;
    for (int rep = 0; rep < 10; rep++) for (int i = 0; i < N; i++) if (v[i] >= 128) s += v[i];     /* data-dependent branch */
    t1 = now(); DO_NOT_OPTIMIZE(s);
    printf("unsorted, branch   : %7.1f ms\n", (t1 - t0) * 1e3);

    qsort(v, N, sizeof *v, cmp);
    t0 = now(); s = 0;
    for (int rep = 0; rep < 10; rep++) for (int i = 0; i < N; i++) if (v[i] >= 128) s += v[i];     /* same code, predictable branch */
    t1 = now(); DO_NOT_OPTIMIZE(s);
    printf("sorted,   branch   : %7.1f ms   <- identical work, the branch is now predictable\n", (t1 - t0) * 1e3);

    t0 = now(); s = 0;
    for (int rep = 0; rep < 10; rep++) for (int i = 0; i < N; i++) { int m = -(v[i] >= 128); s += v[i] & m; }   /* branchless */
    t1 = now(); DO_NOT_OPTIMIZE(s);
    printf("sorted,   branchless: %6.1f ms\n", (t1 - t0) * 1e3);
    free(v); return 0;
}
