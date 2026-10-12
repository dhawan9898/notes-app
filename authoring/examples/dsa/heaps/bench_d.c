#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include "heap.h"
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

/* workload 0: pop the min, push a random key.  Popped keys are small, so the heap's contents drift upward and a fresh
 *             random key is usually SMALL relative to the heap: it sifts up almost to the root (push-dominated).
 * workload 1: event queue: pop the min m, push m + random delay.  The new key is larger than everything popped, so it
 *             stays near a leaf and the sift-down of the pop dominates. */
static double run(size_t n, unsigned d, int workload) {
    Heap h; heap_init(&h, d, 1); heap_reserve(&h, n + 1);
    for (size_t i = 0; i < n; i++) heap_push(&h, (int)(rnd() >> 2));
    long sink = 0; double t0 = now();
    for (int i = 0; i < 2000000; i++) {
        int m = heap_pop(&h); sink += m;
        heap_push(&h, workload ? m + (int)(rnd() % 1000000) : (int)(rnd() >> 2));
    }
    double dt = now() - t0; heap_free(&h); if (sink == 42) puts("");
    return dt / 2000000 * 1e9;
}

int main(void) {
    const char *name[] = { "A: pop min, push a random key (sift-up heavy)", "B: event queue, push min + delay (sift-down heavy)" };
    for (int w = 0; w < 2; w++) {
        printf("%s -- ns per (pop + push)\n%10s %9s | %6s %6s %6s %6s\n", name[w], "n", "size", "d=2", "d=4", "d=8", "d=16");
        for (size_t n = 1000; n <= 33000000; n *= 8)
            printf("%10zu %7.1fMB | %6.0f %6.0f %6.0f %6.0f\n", n, (double)n * 4 / 1e6, run(n, 2, w), run(n, 4, w), run(n, 8, w), run(n, 16, w));
        printf("\n");
    }
    return 0;
}
