#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "avl.h"
#include "rb.h"
#include "bst.h"
#include "randomized.h"
#include "../../c/perf/timing.h"

static int cmp_int(const void *a, const void *b) { return (*(const int *)a > *(const int *)b) - (*(const int *)a < *(const int *)b); }

static void shuffle(int *a, int n) { for (int i = n - 1; i > 0; i--) { int j = (int)(rr() % (uint32_t)(i + 1)), t = a[i]; a[i] = a[j]; a[j] = t; } }

int main(void) {
    /* correctness of treap and skip list under random operations */
    { enum { R = 1500, OPS = 100000 }; static char present[R]; Treap *t = NULL; Skip sk; skip_init(&sk);
      for (int i = 0; i < OPS; i++) {
          int k = (int)(rr() % R), op = (int)(rr() % 3), f;
          if (op == 0) { t = treap_insert(t, k, &f); assert(f == !present[k]); assert(skip_insert(&sk, k) == f); present[k] = 1; }
          else if (op == 1) { t = treap_delete(t, k, &f); assert(f == present[k]); assert(skip_delete(&sk, k) == f); present[k] = 0; }
          else { assert(treap_find(t, k) == present[k] && skip_find(&sk, k) == present[k]); }
          if (i % 100 == 0) assert(treap_check(t, -1, R));
      }
      printf("treap and skip list: %d random ops matched a reference table (treap heap/BST order checked every 100 ops)\n", OPS);
      treap_free(t); skip_free(&sk); }

    printf("\nbuild n random keys, then n random lookups (ns per operation, single run, shuffled distinct keys)\n");
    printf("%9s | %-10s %9s %9s %7s | memory/node\n", "n", "structure", "insert", "lookup", "height");
    for (int n = 10000; n <= 1000000; n *= 10) {
        int *keys = malloc((size_t)n * sizeof *keys), *probe = malloc((size_t)n * sizeof *probe);
        for (int i = 0; i < n; i++) keys[i] = i * 3;
        shuffle(keys, n);
        memcpy(probe, keys, (size_t)n * sizeof *probe); shuffle(probe, n);
        volatile long sink = 0; double t0;

        BNode *b = NULL; t0 = now(); for (int i = 0; i < n; i++) bst_insert(&b, keys[i]); double ti = now() - t0;
        t0 = now(); for (int i = 0; i < n; i++) sink += bst_find(b, probe[i]) != NULL; double tl = now() - t0;
        printf("%9d | %-10s %9.0f %9.0f %7d | 24 B (malloc 32)\n", n, "plain BST", ti / n * 1e9, tl / n * 1e9, bst_height(b)); bst_free(b);

        AVL *a = NULL; int d; t0 = now(); for (int i = 0; i < n; i++) a = avl_insert(a, keys[i], &d); ti = now() - t0;
        t0 = now(); for (int i = 0; i < n; i++) sink += avl_find(a, probe[i]) != NULL; tl = now() - t0;
        printf("%9s | %-10s %9.0f %9.0f %7d | 24 B (malloc 32)\n", "", "AVL", ti / n * 1e9, tl / n * 1e9, a->h); avl_free(a);

        RBT r; rb_init(&r); t0 = now(); for (int i = 0; i < n; i++) rb_insert(&r, keys[i]); ti = now() - t0;
        t0 = now(); for (int i = 0; i < n; i++) sink += rb_find(&r, probe[i]); tl = now() - t0;
        printf("%9s | %-10s %9.0f %9.0f %7d | 32 B (malloc 48)\n", "", "red-black", ti / n * 1e9, tl / n * 1e9, rb_height(&r, r.root)); rb_free(&r);

        Treap *tr = NULL; t0 = now(); for (int i = 0; i < n; i++) tr = treap_insert(tr, keys[i], &d); ti = now() - t0;
        t0 = now(); for (int i = 0; i < n; i++) sink += treap_find(tr, probe[i]); tl = now() - t0;
        printf("%9s | %-10s %9.0f %9.0f %7d | 24 B (malloc 32)\n", "", "treap", ti / n * 1e9, tl / n * 1e9, treap_height(tr)); treap_free(tr);

        Skip sk; skip_init(&sk); t0 = now(); for (int i = 0; i < n; i++) skip_insert(&sk, keys[i]); ti = now() - t0;
        t0 = now(); for (int i = 0; i < n; i++) sink += skip_find(&sk, probe[i]); tl = now() - t0;
        printf("%9s | %-10s %9.0f %9.0f %7d | 8+8*2 B avg (levels: %d)\n", "", "skip list", ti / n * 1e9, tl / n * 1e9, sk.lvl, sk.lvl); skip_free(&sk);

        qsort(keys, (size_t)n, sizeof *keys, cmp_int);
        t0 = now(); for (int i = 0; i < n; i++) sink += bsearch(&probe[i], keys, (size_t)n, sizeof *keys, cmp_int) != NULL; tl = now() - t0;
        printf("%9s | %-10s %9s %9.0f %7s | 4 B\n\n", "", "sorted arr", "(static)", tl / n * 1e9, "-");
        free(keys); free(probe); (void)sink;
    }
    return 0;
}
