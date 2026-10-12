#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "heap.h"
#include "indexed.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }
static int cmp_int(const void *a, const void *b) { return (*(const int *)a > *(const int *)b) - (*(const int *)a < *(const int *)b); }

/* ---- 1. k-way merge: a heap holds the current head of each of the k sorted runs ---- */
typedef struct { int val, run; } Head;
static void merge_sift(Head *h, int n, int i, unsigned long long *cmps) {
    Head x = h[i];
    for (;;) {
        int c = 2 * i + 1; if (c >= n) break;
        if (c + 1 < n) { (*cmps)++; if (h[c + 1].val < h[c].val) c++; }
        (*cmps)++; if (x.val <= h[c].val) break;
        h[i] = h[c]; i = c;
    }
    h[i] = x;
}
static unsigned long long kway_merge(int **runs, int *len, int k, int *out) {
    Head *h = malloc((size_t)k * sizeof *h); int *idx = calloc((size_t)k, sizeof *idx), n = 0; unsigned long long cmps = 0;
    for (int r = 0; r < k; r++) if (len[r]) h[n++] = (Head){ runs[r][0], r };
    for (int i = n / 2 - 1; i >= 0; i--) merge_sift(h, n, i, &cmps);
    int o = 0;
    while (n) {
        out[o++] = h[0].val; int r = h[0].run;
        if (++idx[r] < len[r]) h[0] = (Head){ runs[r][idx[r]], r }; else h[0] = h[--n];
        if (n) merge_sift(h, n, 0, &cmps);
    }
    free(h); free(idx); return cmps;
}

int main(void) {
    /* ---- indexed heap: randomized test against an O(n) scan ---- */
    { enum { ID = 500, OPS = 200000 }; IPQ q; ipq_init(&q, ID); long ref[ID]; char in[ID] = {0}; int count = 0;
      for (int i = 0; i < OPS; i++) {
          int id = (int)(rnd() % ID), op = (int)(rnd() % 3);
          if (op == 0 && !in[id]) { long p = (long)(rnd() % 100000); ipq_insert(&q, id, p); ref[id] = p; in[id] = 1; count++; }
          else if (op == 1 && in[id]) { long p = ref[id] - (long)(rnd() % 1000); ipq_decrease(&q, id, p); ref[id] = p; }
          else if (op == 2 && count) {
              long best = 1L << 60; for (int j = 0; j < ID; j++) if (in[j] && ref[j] < best) best = ref[j];
              int got = ipq_extract(&q); assert(ref[got] == best && in[got]); in[got] = 0; count--;
          }
          assert(q.n == count);
      }
      ipq_free(&q); printf("indexed heap: %d random insert/decrease-key/extract-min ops match a linear scan\n\n", OPS); }

    /* ---- k-way merge: comparisons vs the k*N naive scan, and vs n*log2(k) bound ---- */
    printf("k-way merge of k sorted runs, 1,000,000 elements total\n%8s %16s %14s %16s\n", "k", "heap compares", "per element", "log2(k)*2 bound");
    int total = 1000000; int *all = malloc((size_t)total * sizeof *all), *out = malloc((size_t)total * sizeof *out);
    for (int k = 2; k <= 1024; k *= 4) {
        for (int i = 0; i < total; i++) all[i] = (int)rnd();
        int **runs = malloc((size_t)k * sizeof *runs), *len = malloc((size_t)k * sizeof *len);
        for (int r = 0; r < k; r++) { len[r] = total / k; runs[r] = all + (size_t)r * (size_t)(total / k); qsort(runs[r], (size_t)len[r], sizeof(int), cmp_int); }
        int merged = k * (total / k);
        unsigned long long c = kway_merge(runs, len, k, out);
        for (int i = 1; i < merged; i++) assert(out[i - 1] <= out[i]);
        printf("%8d %16llu %14.2f %16.1f\n", k, c, (double)c / merged, 2 * log2(k));
        free(runs); free(len);
    }
    free(all); free(out);

    /* ---- running median: max-heap of the lower half, min-heap of the upper half ---- */
    { enum { N = 200000 }; int *v = malloc(N * sizeof *v), *sorted = malloc(N * sizeof *sorted);
      Heap hi; heap_init(&hi, 2, 0); Heap lo; heap_init(&lo, 2, 0);             /* lo stores NEGATED keys so the min-heap acts as a max-heap */
      int checked = 0;
      for (int i = 0; i < N; i++) {
          v[i] = (int)(rnd() % 1000000);
          if (lo.n == 0 || v[i] <= -heap_top(&lo)) heap_push(&lo, -v[i]); else heap_push(&hi, v[i]);
          if (lo.n > hi.n + 1) heap_push(&hi, -heap_pop(&lo));
          else if (hi.n > lo.n) heap_push(&lo, -heap_pop(&hi));
          if (i % 997 == 0) {                                                   /* verify the lower median against a full sort */
              memcpy(sorted, v, (size_t)(i + 1) * sizeof *v); qsort(sorted, (size_t)(i + 1), sizeof *sorted, cmp_int);
              assert(-heap_top(&lo) == sorted[i / 2]); checked++;
          }
      }
      printf("\nrunning median over %d values: lower median verified against a full sort at %d checkpoints (O(log n) per update, O(1) query)\n", N, checked);
      heap_free(&hi); heap_free(&lo); free(v); free(sorted); }

    /* ---- streaming top-k: a size-k min-heap; how often does a new item enter it? ---- */
    { int k = 100; size_t n = 10000000; Heap h; heap_init(&h, 2, 0); size_t replaced = 0;
      for (size_t i = 0; i < n; i++) {
          int x = (int)rnd();
          if ((int)h.n < k) heap_push(&h, x);
          else if (x > heap_top(&h)) { h.a[0] = x; sift_down(&h, 0); replaced++; }
      }
      printf("\ntop-%d of %zu random items: the heap root was replaced %zu times (expected about k*ln(n/k) = %.0f); %.4f%% of items touched the heap\n",
             k, n, replaced, k * log((double)n / k), 100.0 * (double)replaced / (double)n);
      heap_free(&h); }
    return 0;
}
