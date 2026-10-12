#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "heap.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }
static int cmp_int(const void *a, const void *b) { return (*(const int *)a > *(const int *)b) - (*(const int *)a < *(const int *)b); }

int main(void) {
    /* correctness: heapsort via the heap equals qsort */
    { enum { N = 5000 }; int a[N], b[N]; for (int i = 0; i < N; i++) a[i] = b[i] = (int)(rnd() % 1000);
      Heap h; heap_init(&h, 2, 0); heap_build(&h, a, N); assert(heap_check(&h));
      qsort(b, N, sizeof b[0], cmp_int); for (int i = 0; i < N; i++) assert(heap_pop(&h) == b[i]);
      heap_free(&h); printf("heap_build + %d pops == qsort: OK\n\n", N); }

    printf("comparisons to build a binary heap of n random keys\n%9s %14s %12s %14s %12s\n", "n", "n pushes", "per element", "Floyd build", "per element");
    for (size_t n = 1000; n <= 1000000; n *= 10) {
        int *a = malloc(n * sizeof *a); for (size_t i = 0; i < n; i++) a[i] = (int)rnd();
        Heap p; heap_init(&p, 2, 0); for (size_t i = 0; i < n; i++) heap_push(&p, a[i]);
        Heap f; heap_init(&f, 2, 0); heap_build(&f, a, n); assert(heap_check(&p) && heap_check(&f));
        printf("%9zu %14llu %12.2f %14llu %12.2f\n", n, p.cmps, (double)p.cmps / (double)n, f.cmps, (double)f.cmps / (double)n);
        free(a); heap_free(&p); heap_free(&f);
    }
    printf("\npushes in SORTED order cost O(1) each (nothing sifts); pushes in REVERSE sorted order cost log2 n each:\n");
    for (int rev = 0; rev < 2; rev++) {
        size_t n = 1000000; Heap h; heap_init(&h, 2, 0);
        for (size_t i = 0; i < n; i++) heap_push(&h, rev ? (int)(n - i) : (int)i);
        printf("  %-14s %.2f comparisons per push\n", rev ? "descending" : "ascending", (double)h.cmps / (double)n); heap_free(&h);
    }
    return 0;
}
