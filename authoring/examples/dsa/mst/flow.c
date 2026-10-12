#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

typedef long long ll;
static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

/* Flow network in "paired edge" form: edge i and edge i^1 are a forward arc and its residual (reverse) arc. */
typedef struct { int n, m, cap_m; int *head, *nxt, *to; ll *cap; int *level, *it; } Flow;
static void fl_init(Flow *f, int n, int max_edges) {
    f->n = n; f->m = 0; f->cap_m = 2 * max_edges;
    f->head = malloc((size_t)n * sizeof(int)); memset(f->head, -1, (size_t)n * sizeof(int));
    f->nxt = malloc((size_t)f->cap_m * sizeof(int)); f->to = malloc((size_t)f->cap_m * sizeof(int)); f->cap = malloc((size_t)f->cap_m * sizeof(ll));
    f->level = malloc((size_t)n * sizeof(int)); f->it = malloc((size_t)n * sizeof(int));
}
static void fl_free(Flow *f) { free(f->head); free(f->nxt); free(f->to); free(f->cap); free(f->level); free(f->it); }
static void fl_add(Flow *f, int u, int v, ll c) {
    f->to[f->m] = v; f->cap[f->m] = c; f->nxt[f->m] = f->head[u]; f->head[u] = f->m++;
    f->to[f->m] = u; f->cap[f->m] = 0; f->nxt[f->m] = f->head[v]; f->head[v] = f->m++;          /* residual arc starts with capacity 0 */
}

/* ---- Edmonds-Karp: repeatedly augment along a SHORTEST (fewest edges) residual path found by BFS; O(V E^2) ---- */
static ll edmonds_karp(Flow *f, int src, int dst, long *augmentations) {
    ll total = 0; int *prev_e = malloc((size_t)f->n * sizeof(int)), *q = malloc((size_t)f->n * sizeof(int)); *augmentations = 0;
    for (;;) {
        for (int i = 0; i < f->n; i++) prev_e[i] = -2;
        prev_e[src] = -1; int h = 0, t = 0; q[t++] = src;
        while (h < t && prev_e[dst] == -2) { int u = q[h++]; for (int e = f->head[u]; e >= 0; e = f->nxt[e]) if (f->cap[e] > 0 && prev_e[f->to[e]] == -2) { prev_e[f->to[e]] = e; q[t++] = f->to[e]; } }
        if (prev_e[dst] == -2) break;                                      /* no augmenting path: the flow is maximum (max-flow min-cut) */
        ll push = (ll)1 << 60; for (int v = dst; v != src; v = f->to[prev_e[v] ^ 1]) if (f->cap[prev_e[v]] < push) push = f->cap[prev_e[v]];
        for (int v = dst; v != src; v = f->to[prev_e[v] ^ 1]) { f->cap[prev_e[v]] -= push; f->cap[prev_e[v] ^ 1] += push; }
        total += push; (*augmentations)++;
    }
    free(prev_e); free(q); return total;
}

/* ---- Dinic: BFS builds a level graph, then DFS pushes a BLOCKING flow along level-increasing arcs; O(V^2 E), unit networks O(E sqrt V) ---- */
static int dinic_bfs(Flow *f, int src, int dst) {
    for (int i = 0; i < f->n; i++) f->level[i] = -1;
    int *q = malloc((size_t)f->n * sizeof(int)), h = 0, t = 0; q[t++] = src; f->level[src] = 0;
    while (h < t) { int u = q[h++]; for (int e = f->head[u]; e >= 0; e = f->nxt[e]) if (f->cap[e] > 0 && f->level[f->to[e]] < 0) { f->level[f->to[e]] = f->level[u] + 1; q[t++] = f->to[e]; } }
    free(q); return f->level[dst] >= 0;
}
static ll dinic_dfs(Flow *f, int u, int dst, ll pushed) {
    if (u == dst) return pushed;
    for (int *e = &f->it[u]; *e >= 0; *e = f->nxt[*e]) {                    /* current-arc pointer: an arc that failed once is never retried in this phase */
        int v = f->to[*e];
        if (f->cap[*e] > 0 && f->level[v] == f->level[u] + 1) {
            ll d = dinic_dfs(f, v, dst, pushed < f->cap[*e] ? pushed : f->cap[*e]);
            if (d > 0) { f->cap[*e] -= d; f->cap[*e ^ 1] += d; return d; }
        }
    }
    return 0;
}
static ll dinic(Flow *f, int src, int dst, int *phases) {
    ll total = 0; *phases = 0;
    while (dinic_bfs(f, src, dst)) {
        memcpy(f->it, f->head, (size_t)f->n * sizeof(int)); (*phases)++;
        for (ll d; (d = dinic_dfs(f, src, dst, (ll)1 << 60)) > 0; ) total += d;
    }
    return total;
}

/* ---- brute-force min cut: enumerate every vertex set S containing src and not dst; max-flow = min over S of capacity leaving S ---- */
static ll brute_min_cut(int n, int m, const int (*ed)[3], int src, int dst) {
    ll best = -1;
    for (unsigned mask = 0; mask < (1u << n); mask++) {
        if (!(mask >> src & 1) || (mask >> dst & 1)) continue;
        ll cut = 0; for (int i = 0; i < m; i++) if ((mask >> ed[i][0] & 1) && !(mask >> ed[i][1] & 1)) cut += ed[i][2];
        if (best < 0 || cut < best) best = cut;
    }
    return best;
}

int main(void) {
    for (int round = 0; round < 2000; round++) {
        int n = 3 + (int)(rnd() % 8), m = 1 + (int)(rnd() % (unsigned)(3 * n)); int ed[40][3]; Flow a, b; fl_init(&a, n, m); fl_init(&b, n, m);
        for (int i = 0; i < m; i++) { int u = (int)(rnd() % (unsigned)n), v = (int)(rnd() % (unsigned)n); if (u == v) v = (u + 1) % n; int c = 1 + (int)(rnd() % 9); ed[i][0] = u; ed[i][1] = v; ed[i][2] = c; fl_add(&a, u, v, c); fl_add(&b, u, v, c); }
        long aug; int ph; ll ek = edmonds_karp(&a, 0, n - 1, &aug), dn = dinic(&b, 0, n - 1, &ph), cut = brute_min_cut(n, m, (const int (*)[3])ed, 0, n - 1);
        assert(ek == dn && dn == cut);                                      /* max-flow min-cut theorem, checked on 2000 networks */
        fl_free(&a); fl_free(&b);
    }
    printf("2000 random flow networks: Edmonds-Karp = Dinic = brute-force minimum cut (max-flow min-cut theorem)\n\n");

    /* bipartite matching as max flow: source -> left (cap 1), left -> right (cap 1), right -> sink (cap 1) */
    int L = 3000, R = 3000, deg = 4; Flow f; fl_init(&f, L + R + 2, L * deg + L + R); int src = L + R, dst = L + R + 1;
    char (*adj)[3000] = calloc((size_t)L, sizeof *adj);
    for (int u = 0; u < L; u++) fl_add(&f, src, u, 1);
    for (int v = 0; v < R; v++) fl_add(&f, L + v, dst, 1);
    for (int u = 0; u < L; u++) for (int k = 0; k < deg; k++) { int v = (int)(rnd() % (unsigned)R); if (!adj[u][v]) { adj[u][v] = 1; fl_add(&f, u, L + v, 1); } }
    int ph; double t0 = now(); ll match = dinic(&f, src, dst, &ph);
    printf("bipartite matching, %d x %d vertices, ~%d random edges per left vertex: maximum matching %lld (Dinic: %d phases, %.1f ms)\n", L, R, deg, match, ph, (now() - t0) * 1e3);

    /* Dinic vs Edmonds-Karp on a layered random network */
    printf("\n%-34s %10s %12s | %12s %10s | %14s %10s\n", "network", "max flow", "", "EK augments", "EK ms", "Dinic phases", "Dinic ms");
    int sizes[] = { 2000, 8000 };
    for (int si = 0; si < 2; si++) {
        int n = sizes[si], m = n * 6; Flow a, b; fl_init(&a, n, m); fl_init(&b, n, m);
        for (int i = 0; i < m; i++) { int u = (int)(rnd() % (unsigned)(n - 1)), v = u + 1 + (int)(rnd() % (unsigned)(n - 1 - u < 40 ? n - 1 - u : 40)); int c = 1 + (int)(rnd() % 100); fl_add(&a, u, v, c); fl_add(&b, u, v, c); }
        long aug; t0 = now(); ll ek = edmonds_karp(&a, 0, n - 1, &aug); double te = now() - t0;
        t0 = now(); ll dn = dinic(&b, 0, n - 1, &ph); double td = now() - t0; assert(ek == dn);
        printf("%-34s %10lld %12s | %12ld %10.0f | %14d %10.1f\n", si ? "DAG-like, 8000 vertices, 48k arcs" : "DAG-like, 2000 vertices, 12k arcs", ek, "", aug, te * 1e3, ph, td * 1e3);
        fl_free(&a); fl_free(&b);
    }
    fl_free(&f); free(adj);
    return 0;
}
