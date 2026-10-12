#ifndef DSU_H
#define DSU_H
#include <stdint.h>
#include <stdlib.h>

/* Disjoint-set union (union-find) with selectable heuristics so their effect can be measured. */
enum { DSU_NAIVE = 0, DSU_COMPRESS = 1, DSU_RANK = 2, DSU_BOTH = 3 };
typedef struct { int *p, *rank; int n, mode; unsigned long long steps; } DSU;   /* steps = parent pointers followed */

static inline void dsu_init(DSU *d, int n, int mode) {
    d->p = malloc((size_t)n * sizeof(int)); d->rank = calloc((size_t)n, sizeof(int)); d->n = n; d->mode = mode; d->steps = 0;
    for (int i = 0; i < n; i++) d->p[i] = i;
}
static inline void dsu_free(DSU *d) { free(d->p); free(d->rank); }
static inline int dsu_find(DSU *d, int x) {
    int r = x;
    while (d->p[r] != r) { r = d->p[r]; d->steps++; }
    if (d->mode & DSU_COMPRESS)                                      /* path compression: point every node on the path at the root */
        while (d->p[x] != r) { int nx = d->p[x]; d->p[x] = r; x = nx; }
    return r;
}
static inline int dsu_union(DSU *d, int a, int b) {                  /* returns 1 if a and b were in different sets */
    a = dsu_find(d, a); b = dsu_find(d, b);
    if (a == b) return 0;
    if (d->mode & DSU_RANK) {                                         /* union by rank: attach the shallower tree below the deeper one */
        if (d->rank[a] < d->rank[b]) { int t = a; a = b; b = t; }
        d->p[b] = a; if (d->rank[a] == d->rank[b]) d->rank[a]++;
    } else d->p[b] = a;
    return 1;
}
#endif
