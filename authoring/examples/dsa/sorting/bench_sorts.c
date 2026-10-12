#include <stdio.h>
#include "sorts.h"
#include "../../c/perf/timing.h"

unsigned long long g_cmps;
static int cmp_int(const void *x, const void *y) { int a = *(const int *)x, b = *(const int *)y; return (a > b) - (a < b); }
typedef void (*sort_fn)(int *, size_t);
static void merge_wrap(int *a, size_t n) { merge_sort(a, n); }
static void libc_wrap(int *a, size_t n) { qsort(a, n, sizeof *a, cmp_int); }

static const char *kinds[] = { "random", "sorted", "reversed", "few distinct", "organ pipe" };
static void gen(int *a, size_t n, int kind) {
    uint64_t r = 88172645463325252ULL;
    for (size_t i = 0; i < n; i++) {
        r ^= r << 13; r ^= r >> 7; r ^= r << 17;
        switch (kind) { case 0: a[i] = (int)(r >> 33); break; case 1: a[i] = (int)i; break; case 2: a[i] = (int)(n - i); break;
                        case 3: a[i] = (int)(r % 8); break; default: a[i] = (int)(i < n / 2 ? i : n - i); }
    }
}

int main(void) {
    struct { const char *name; sort_fn fn; } algs[] = { {"merge", merge_wrap}, {"quick med3", quick_sort}, {"quick 3-way", quick3_sort}, {"heap", heap_sort}, {"introsort", intro_sort}, {"libc qsort", libc_wrap} };
    size_t n = 1000000;
    int *a = malloc(n * sizeof *a), *b = malloc(n * sizeof *b);
    printf("time in ms to sort %zu ints (lower is better)\n%-14s", n, "input");
    for (size_t k = 0; k < 6; k++) printf("%12s", algs[k].name);
    puts("");
    for (int kind = 0; kind < 5; kind++) {
        gen(b, n, kind);
        printf("%-14s", kinds[kind]);
        for (size_t k = 0; k < 6; k++) {
            memcpy(a, b, n * sizeof *a);
            double t0 = now(); algs[k].fn(a, n); double t1 = now();
            for (size_t i = 1; i < n; i++) if (a[i - 1] > a[i]) { printf(" UNSORTED!"); break; }
            printf("%12.1f", (t1 - t0) * 1e3);
        }
        puts("");
    }
    size_t m = 20000;
    int *c = malloc(m * sizeof *c); gen(c, m, 0);
    double t0 = now(); insertion_sort(c, m); double t1 = now();
    gen(c, m, 0); /* random */
    printf("\ninsertion sort on only %zu random ints: %.1f ms (O(n^2): the 1,000,000-element version would take ~%.0f s)\n", m, (t1 - t0) * 1e3, (t1 - t0) * (double)n / m * (double)n / m);
    free(a); free(b); free(c);
    return 0;
}
