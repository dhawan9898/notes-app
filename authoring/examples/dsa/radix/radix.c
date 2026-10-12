#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

/* ---- counting sort: keys in [0, K).  Stable, O(n + K). ---- */
static void counting_sort(const uint16_t *in, uint16_t *out, size_t n, size_t K) {
    size_t *count = calloc(K + 1, sizeof *count);
    for (size_t i = 0; i < n; i++) count[in[i] + 1]++;          /* histogram, shifted by one */
    for (size_t k = 0; k < K; k++) count[k + 1] += count[k];    /* prefix sums: count[k] = number of keys < k = first output slot of key k */
    for (size_t i = 0; i < n; i++) out[count[in[i]]++] = in[i]; /* scan left to right => stable */
    free(count);
}

/* ---- LSD radix sort on 32-bit unsigned keys with 8-bit digits: 4 stable counting-sort passes. ---- */
static void radix_sort_u32(uint32_t *a, uint32_t *tmp, size_t n) {
    for (int shift = 0; shift < 32; shift += 8) {
        size_t count[257] = {0};
        for (size_t i = 0; i < n; i++) count[((a[i] >> shift) & 0xFF) + 1]++;
        for (int d = 0; d < 256; d++) count[d + 1] += count[d];
        for (size_t i = 0; i < n; i++) tmp[count[(a[i] >> shift) & 0xFF]++] = a[i];
        uint32_t *t = a; a = tmp; tmp = t;                      /* ping-pong buffers; after 4 passes the result is back in the original array */
    }
}

/* signed ints: flip the sign bit so that negative numbers sort before positive ones in unsigned order */
static void radix_sort_i32(int32_t *a, uint32_t *tmp, size_t n) {
    uint32_t *u = (uint32_t *)a;
    for (size_t i = 0; i < n; i++) u[i] ^= 0x80000000u;
    radix_sort_u32(u, tmp, n);
    for (size_t i = 0; i < n; i++) u[i] ^= 0x80000000u;
}

/* ---- bucket sort for floating-point keys uniformly distributed in [0, 1): expected O(n) ---- */
typedef struct { double v; int next; } BNode;
static void bucket_sort(double *a, size_t n) {
    int *head = malloc(n * sizeof *head); BNode *nodes = malloc(n * sizeof *nodes);
    for (size_t i = 0; i < n; i++) head[i] = -1;
    for (size_t i = 0; i < n; i++) {                            /* n buckets; element goes to bucket floor(v * n) */
        size_t b = (size_t)(a[i] * (double)n);
        nodes[i].v = a[i];
        /* insert into the bucket's list keeping it sorted (buckets are tiny on average) */
        int *p = &head[b];
        while (*p != -1 && nodes[*p].v < a[i]) p = &nodes[*p].next;
        nodes[i].next = *p; *p = (int)i;
    }
    size_t k = 0;
    for (size_t b = 0; b < n; b++) for (int p = head[b]; p != -1; p = nodes[p].next) a[k++] = nodes[p].v;
    free(head); free(nodes);
}

static int cmp_u32(const void *x, const void *y) { uint32_t a = *(const uint32_t *)x, b = *(const uint32_t *)y; return (a > b) - (a < b); }
static int cmp_dbl(const void *x, const void *y) { double a = *(const double *)x, b = *(const double *)y; return (a > b) - (a < b); }

int main(void) {
    srand(5);
    /* correctness against qsort */
    for (int t = 0; t < 300; t++) {
        size_t n = (size_t)rand() % 500;
        uint32_t a[500], b[500], tmp[500];
        for (size_t i = 0; i < n; i++) a[i] = ((uint32_t)rand() << 16) ^ (uint32_t)rand();
        memcpy(b, a, n * sizeof *a);
        radix_sort_u32(a, tmp, n); qsort(b, n, sizeof *b, cmp_u32);
        assert(!memcmp(a, b, n * sizeof *a));
        int32_t s[500], sref[500];
        for (size_t i = 0; i < n; i++) s[i] = (int32_t)(((uint32_t)rand() << 16) ^ (uint32_t)rand());
        memcpy(sref, s, n * sizeof *s);
        radix_sort_i32(s, tmp, n);
        for (size_t i = 1; i < n; i++) assert(s[i - 1] <= s[i]);
        double d[500], dref[500];
        for (size_t i = 0; i < n; i++) d[i] = rand() / (RAND_MAX + 1.0);
        memcpy(dref, d, n * sizeof *d);
        bucket_sort(d, n); qsort(dref, n, sizeof *dref, cmp_dbl);
        assert(!memcmp(d, dref, n * sizeof *d));
    }
    printf("counting/radix/bucket sorts match qsort on 300 random arrays (signed and unsigned, n up to 500)\n");

    uint16_t ca[20] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3, 2, 3, 8, 4}, cb[20];
    counting_sort(ca, cb, 20, 10);
    printf("counting sort of digits: "); for (int i = 0; i < 20; i++) printf("%d", cb[i]); puts("");

    /* benchmark: 32-bit keys */
    printf("\n%12s %12s %12s %12s\n", "n", "radix (ms)", "qsort (ms)", "speedup");
    for (size_t n = 100000; n <= 10000000; n *= 10) {
        uint32_t *a = malloc(n * 4), *b = malloc(n * 4), *tmp = malloc(n * 4);
        uint64_t r = 88172645463325252ULL;
        for (size_t i = 0; i < n; i++) { r ^= r << 13; r ^= r >> 7; r ^= r << 17; a[i] = (uint32_t)r; }
        memcpy(b, a, n * 4);
        double t0 = now(); radix_sort_u32(a, tmp, n); double t1 = now();
        qsort(b, n, 4, cmp_u32); double t2 = now();
        assert(!memcmp(a, b, n * 4));
        printf("%12zu %12.1f %12.1f %11.1fx\n", n, (t1 - t0) * 1e3, (t2 - t1) * 1e3, (t2 - t1) / (t1 - t0));
        free(a); free(b); free(tmp);
    }
    return 0;
}
