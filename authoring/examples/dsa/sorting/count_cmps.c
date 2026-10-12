#define COUNT_CMPS 1
#include <math.h>
#include <stdio.h>
#include "sorts.h"

unsigned long long g_cmps;
static void gen(int *a, size_t n, int kind) { uint64_t r = 88172645463325252ULL; for (size_t i = 0; i < n; i++) { r ^= r << 13; r ^= r >> 7; r ^= r << 17; a[i] = kind == 0 ? (int)(r >> 33) : kind == 1 ? (int)i : (int)(n - i); } }

int main(void) {
    size_t n = 100000;
    int *a = malloc(n * sizeof *a), *b = malloc(n * sizeof *b);
    double lg = log2((double)n);
    printf("comparisons to sort n = %zu (n*log2(n) = %.0f, information-theoretic minimum log2(n!) = %.0f)\n", n, n * lg, lgamma((double)n + 1) / log(2.0));
    printf("%-14s %14s %14s %14s\n", "algorithm", "random", "sorted", "reversed");
    struct { const char *name; int which; } algs[] = { {"merge", 0}, {"quick med3", 1}, {"quick 3-way", 2}, {"heap", 3}, {"introsort", 4} };
    for (int k = 0; k < 5; k++) {
        printf("%-14s", algs[k].name);
        for (int kind = 0; kind < 3; kind++) {
            gen(b, n, kind); memcpy(a, b, n * sizeof *a); g_cmps = 0;
            switch (algs[k].which) { case 0: merge_sort(a, n); break; case 1: quick_sort(a, n); break; case 2: quick3_sort(a, n); break; case 3: heap_sort(a, n); break; default: intro_sort(a, n); }
            printf(" %14llu", g_cmps);
        }
        puts("");
    }
    free(a); free(b);
    return 0;
}
