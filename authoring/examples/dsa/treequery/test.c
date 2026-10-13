// Differential test of every structure in treeq.h against naive implementations, on trees of several shapes.
#include "treeq.h"
#include <stdio.h>

static uint64_t rng = 0x2545F4914F6CDD1DULL;
static uint64_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

enum { RANDOM, PATH, STAR, BINARY, CATERPILLAR, SHAPES };
static const char *shape_name[] = {"random", "path", "star", "binary", "caterpillar"};

static void gen(int shape, int n, int (*e)[2]) {                 // random labels: shuffle so root 0 is not special
    int *perm = malloc(n * sizeof(int)); for (int i = 0; i < n; i++) perm[i] = i;
    for (int i = n - 1; i > 0; i--) { int j = rnd() % (i + 1); int x = perm[i]; perm[i] = perm[j]; perm[j] = x; }
    for (int i = 1; i < n; i++) {
        int p = shape == RANDOM ? (int)(rnd() % i) : shape == PATH ? i - 1 : shape == STAR ? 0 : shape == BINARY ? (i - 1) / 2 : (i % 2 ? (i - 1) / 2 * 2 : i - 1);
        if (shape == CATERPILLAR) p = i % 2 ? (i > 1 ? i - 2 : 0) : i - 1;     // spine of even vertices, leaves hanging from them
        e[i - 1][0] = perm[p]; e[i - 1][1] = perm[i];
    }
    free(perm);
}

static int naive_lca(const tree_t *t, int u, int v) {
    while (t->depth[u] > t->depth[v]) u = t->par[u];
    while (t->depth[v] > t->depth[u]) v = t->par[v];
    while (u != v) { u = t->par[u]; v = t->par[v]; }
    return u;
}
static long long naive_path_sum(const tree_t *t, const long long *val, int u, int v) {
    long long s = 0;
    while (u != v) { if (t->depth[u] >= t->depth[v]) { s += val[u]; u = t->par[u]; } else { s += val[v]; v = t->par[v]; } }
    return s + val[u];
}
static int naive_nearest(const tree_t *t, const char *marked, int src) {
    int n = t->n; int *d = malloc(n * sizeof(int)), *q = malloc(n * sizeof(int)); for (int i = 0; i < n; i++) d[i] = -1;
    int h = 0, tl = 0; q[tl++] = src; d[src] = 0; int ans = INF;
    while (h < tl) { int v = q[h++]; if (marked[v] && d[v] < ans) ans = d[v];
        for (int e = t->adj_start[v]; e < t->adj_start[v + 1]; e++) { int w = t->adj[e]; if (d[w] < 0) { d[w] = d[v] + 1; q[tl++] = w; } } }
    free(d); free(q); return ans;
}

int main(void) {
    long checks = 0;
    for (int shape = 0; shape < SHAPES; shape++)
        for (int n = 1; n <= 300; n += (n < 20 ? 1 : 37)) {
            int (*e)[2] = malloc((n > 1 ? n - 1 : 1) * sizeof *e); gen(shape, n, e);
            tree_t t; tree_build(&t, n, (const int (*)[2])e);
            lift_t L; lift_build(&L, &t); rmq_t R; rmq_build(&R, &t); hld_t H; hld_build(&H, &t); cent_t C; cent_build(&C, &t);
            long long *val = calloc(n, sizeof(long long)); char *marked = calloc(n, 1);
            for (int op = 0; op < 400; op++) {
                int u = rnd() % n, v = rnd() % n, w = naive_lca(&t, u, v);
                if (lift_lca(&L, u, v) != w || rmq_lca(&R, u, v) != w || hld_lca(&H, u, v) != w) { printf("LCA mismatch (%s, n=%d)\n", shape_name[shape], n); return 1; }
                int x = rnd() % n; long long d = (long long)(rnd() % 2001) - 1000; val[x] += d; hld_add(&H, x, d);
                if (hld_path_sum(&H, u, v) != naive_path_sum(&t, val, u, v)) { printf("path sum mismatch (%s, n=%d)\n", shape_name[shape], n); return 1; }
                if (op % 3 == 0) { int m = rnd() % n; marked[m] = 1; cent_mark(&C, m); }
                if (cent_nearest(&C, u) != naive_nearest(&t, marked, u)) { printf("nearest mismatch (%s, n=%d)\n", shape_name[shape], n); return 1; }
                checks += 4;
            }
            free(e); free(val); free(marked);
        }
    printf("%ld cross-checks against naive implementations passed (5 tree shapes, n = 1..300)\n", checks);
    return 0;
}
