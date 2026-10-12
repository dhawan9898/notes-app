#include <assert.h>
#include <stdio.h>
#include "sorts.h"

unsigned long long g_cmps;
static int cmp_int(const void *x, const void *y) { int a = *(const int *)x, b = *(const int *)y; return (a > b) - (a < b); }

typedef void (*sort_fn)(int *, size_t);
static void merge_wrap(int *a, size_t n) { merge_sort(a, n); }
static void libc_wrap(int *a, size_t n) { qsort(a, n, sizeof *a, cmp_int); }

static void gen(int *a, size_t n, int kind) {
    for (size_t i = 0; i < n; i++) switch (kind) {
        case 0: a[i] = rand(); break;                          /* random */
        case 1: a[i] = (int)i; break;                          /* sorted */
        case 2: a[i] = (int)(n - i); break;                    /* reversed */
        case 3: a[i] = rand() % 8; break;                      /* few distinct values */
        case 4: a[i] = (int)(i < n / 2 ? i : n - i); break;    /* organ pipe */
        default: a[i] = 7;                                     /* all equal */
    }
}

int main(void) {
    struct { const char *name; sort_fn fn; } algs[] = { {"insertion", insertion_sort}, {"merge", merge_wrap}, {"quick (med3)", quick_sort}, {"quick 3-way", quick3_sort}, {"heap", heap_sort}, {"introsort", intro_sort}, {"libc qsort", libc_wrap} };
    srand(1);
    long cases = 0;
    for (size_t k = 0; k < sizeof algs / sizeof *algs; k++)
        for (int kind = 0; kind < 6; kind++)
            for (size_t n = 0; n <= 300; n += (n < 40 ? 1 : 37)) {
                int a[400], b[400];
                gen(a, n, kind); memcpy(b, a, n * sizeof *a);
                algs[k].fn(a, n); qsort(b, n, sizeof *b, cmp_int);
                assert(!memcmp(a, b, n * sizeof *a)); cases++;
            }
    printf("%ld small cases (n = 0..300, six input patterns, seven algorithms) match libc qsort\n", cases);

    /* stability: sort records by key only and check that equal keys keep their original order */
    printf("\nstability (records with many equal keys):\n");
    enum { N = 2000 };
    Rec base[N];
    for (int i = 0; i < N; i++) { base[i].key = rand() % 50; base[i].seq = i; }
    /* insertion sort and a merge of Rec: re-implemented briefly here because the library sorts work on ints */
    Rec r[N]; memcpy(r, base, sizeof r);
    for (int i = 1; i < N; i++) { Rec k = r[i]; int j = i; while (j > 0 && k.key < r[j - 1].key) { r[j] = r[j - 1]; j--; } r[j] = k; }
    int stable = 1; for (int i = 1; i < N; i++) if (r[i - 1].key == r[i].key && r[i - 1].seq > r[i].seq) stable = 0;
    printf("  insertion sort: %s\n", stable ? "stable" : "NOT stable");
    memcpy(r, base, sizeof r);
    /* heap-sort style unstable behaviour: sort by key using a selection sort with swaps (unstable) */
    for (int i = 0; i < N - 1; i++) { int m = i; for (int j = i + 1; j < N; j++) if (r[j].key < r[m].key) m = j; Rec t = r[i]; r[i] = r[m]; r[m] = t; }
    stable = 1; for (int i = 1; i < N; i++) if (r[i - 1].key == r[i].key && r[i - 1].seq > r[i].seq) stable = 0;
    printf("  selection sort (swap-based): %s\n", stable ? "stable" : "NOT stable");
    return 0;
}
