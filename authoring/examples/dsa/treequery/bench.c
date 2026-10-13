// Build and query cost on n = 1,000,000 vertices: a path (the worst case for depth) and a random tree.
#include "treeq.h"
#include <stdio.h>
#include <time.h>

static uint64_t rng = 0x9E3779B97F4A7C15ULL;
static uint64_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

static void bench(const char *name, int path) {
    int n = 1000000, Q = 1000000;
    int (*e)[2] = malloc((n - 1) * sizeof *e);
    for (int i = 1; i < n; i++) { e[i - 1][0] = path ? i - 1 : (int)(rnd() % i); e[i - 1][1] = i; }
    tree_t t; double t0 = now(); tree_build(&t, n, (const int (*)[2])e); double d_tree = now() - t0;
    lift_t L; t0 = now(); lift_build(&L, &t); double d_lift = now() - t0;
    rmq_t R;  t0 = now(); rmq_build(&R, &t);  double d_rmq = now() - t0;
    hld_t H;  t0 = now(); hld_build(&H, &t);  double d_hld = now() - t0;
    cent_t C; t0 = now(); cent_build(&C, &t); double d_cent = now() - t0;
    int *us = malloc(Q * sizeof(int)), *vs = malloc(Q * sizeof(int));
    for (int i = 0; i < Q; i++) { us[i] = rnd() % n; vs[i] = rnd() % n; }
    long long sink = 0;
    t0 = now(); for (int i = 0; i < Q; i++) sink += lift_lca(&L, us[i], vs[i]); double q_lift = (now() - t0) / Q * 1e9;
    t0 = now(); for (int i = 0; i < Q; i++) sink += rmq_lca(&R, us[i], vs[i]);  double q_rmq  = (now() - t0) / Q * 1e9;
    t0 = now(); for (int i = 0; i < Q; i++) sink += hld_lca(&H, us[i], vs[i]);  double q_hld  = (now() - t0) / Q * 1e9;
    t0 = now(); for (int i = 0; i < Q; i++) sink += hld_path_sum(&H, us[i], vs[i]); double q_sum = (now() - t0) / Q * 1e9;
    t0 = now(); for (int i = 0; i < 1000; i++) cent_mark(&C, us[i]); double m = (now() - t0) / 1000 * 1e9;
    t0 = now(); for (int i = 0; i < Q; i++) sink += cent_nearest(&C, vs[i]); double q_near = (now() - t0) / Q * 1e9;
    int maxdepth = 0; for (int i = 0; i < n; i++) if (t.depth[i] > maxdepth) maxdepth = t.depth[i];
    int chains = 0; for (int i = 0; i < n; i++) if (H.head[i] == i) chains++;
    printf("%s: n=%d, depth %d, %d heavy chains\n", name, n, maxdepth, chains);
    printf("  build (ms):  tree %.0f  lifting %.0f  sparse-table %.0f  HLD %.0f  centroid %.0f\n", d_tree * 1e3, d_lift * 1e3, d_rmq * 1e3, d_hld * 1e3, d_cent * 1e3);
    printf("  query (ns):  LCA lifting %.0f  LCA sparse-table %.0f  LCA via HLD %.0f  HLD path sum %.0f  centroid nearest %.0f  centroid mark %.0f\n", q_lift, q_rmq, q_hld, q_sum, q_near, m);
    if (sink == 42) puts("");
    free(e); free(us); free(vs);
}

int main(void) { bench("path", 1); bench("random tree", 0); return 0; }
