// Tree queries on a rooted tree with n vertices (0..n-1), root 0. All traversals are iterative (a path graph of
// 10^6 vertices would overflow the call stack if we recursed). Memory: O(n log n) for lifting and centroid distances.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int n, *adj_start, *adj;          // CSR adjacency
    int *par, *depth, *order;         // order: BFS order from the root; par[0] = -1
    int *size, *heavy;                // subtree size, heavy child (-1 for leaves)
} tree_t;

static void tree_build(tree_t *t, int n, const int (*edges)[2]) {
    t->n = n;
    t->adj_start = calloc(n + 2, sizeof(int)); t->adj = malloc(2 * (n > 1 ? n - 1 : 1) * sizeof(int));
    for (int i = 0; i < n - 1; i++) { t->adj_start[edges[i][0] + 1]++; t->adj_start[edges[i][1] + 1]++; }
    for (int i = 0; i < n; i++) t->adj_start[i + 1] += t->adj_start[i];
    int *fill = malloc(n * sizeof(int)); memcpy(fill, t->adj_start, n * sizeof(int));
    for (int i = 0; i < n - 1; i++) { int a = edges[i][0], b = edges[i][1]; t->adj[fill[a]++] = b; t->adj[fill[b]++] = a; }
    free(fill);
    t->par = malloc(n * sizeof(int)); t->depth = malloc(n * sizeof(int)); t->order = malloc(n * sizeof(int));
    t->size = malloc(n * sizeof(int)); t->heavy = malloc(n * sizeof(int));
    int head = 0, tail = 0; t->order[tail++] = 0; t->par[0] = -1; t->depth[0] = 0;
    while (head < tail) {
        int v = t->order[head++];
        for (int e = t->adj_start[v]; e < t->adj_start[v + 1]; e++) {
            int w = t->adj[e]; if (w == t->par[v]) continue;
            t->par[w] = v; t->depth[w] = t->depth[v] + 1; t->order[tail++] = w;
        }
    }
    for (int i = 0; i < n; i++) { t->size[i] = 1; t->heavy[i] = -1; }
    for (int i = n - 1; i > 0; i--) { int v = t->order[i], p = t->par[v]; t->size[p] += t->size[v]; }
    for (int i = n - 1; i > 0; i--) { int v = t->order[i], p = t->par[v]; if (t->heavy[p] < 0 || t->size[v] > t->size[t->heavy[p]]) t->heavy[p] = v; }
}

/* ---------- LCA 1: binary lifting, O(n log n) memory, O(log n) per query ---------- */
typedef struct { int log; int **up; const tree_t *t; } lift_t;
static void lift_build(lift_t *L, const tree_t *t) {
    int lg = 1; while ((1 << lg) < t->n) lg++;
    L->log = lg; L->t = t; L->up = malloc(lg * sizeof(int *));
    L->up[0] = malloc(t->n * sizeof(int));
    for (int v = 0; v < t->n; v++) L->up[0][v] = t->par[v] < 0 ? v : t->par[v];     // the root is its own ancestor
    for (int j = 1; j < lg; j++) {
        L->up[j] = malloc(t->n * sizeof(int));
        for (int v = 0; v < t->n; v++) L->up[j][v] = L->up[j - 1][L->up[j - 1][v]];
    }
}
static int lift_lca(const lift_t *L, int u, int v) {
    const tree_t *t = L->t;
    if (t->depth[u] < t->depth[v]) { int x = u; u = v; v = x; }
    int diff = t->depth[u] - t->depth[v];
    for (int j = 0; j < L->log; j++) if (diff >> j & 1) u = L->up[j][u];
    if (u == v) return u;
    for (int j = L->log - 1; j >= 0; j--) if (L->up[j][u] != L->up[j][v]) { u = L->up[j][u]; v = L->up[j][v]; }
    return L->up[0][u];
}

/* ---------- LCA 2: preorder + sparse table, O(n log n) build, O(1) per query ---------- */
// In preorder, for tin[u] < tin[v] the LCA is the parent of the shallowest vertex in preorder positions (tin[u], tin[v]].
typedef struct { int *tin, **sp, *lg; const tree_t *t; } rmq_t;
static int shallower(const tree_t *t, int a, int b) { return t->depth[a] <= t->depth[b] ? a : b; }
static void rmq_build(rmq_t *R, const tree_t *t) {
    int n = t->n; R->t = t; R->tin = malloc(n * sizeof(int));
    int *stack = malloc(n * sizeof(int)), *pre = malloc(n * sizeof(int)), sp = 0, cnt = 0;
    stack[sp++] = 0;
    while (sp) {                                   // iterative preorder
        int v = stack[--sp]; R->tin[v] = cnt; pre[cnt++] = v;
        for (int e = t->adj_start[v]; e < t->adj_start[v + 1]; e++) if (t->adj[e] != t->par[v]) stack[sp++] = t->adj[e];
    }
    R->lg = malloc((n + 1) * sizeof(int)); R->lg[1] = 0;
    for (int i = 2; i <= n; i++) R->lg[i] = R->lg[i / 2] + 1;
    int K = R->lg[n] + 1; R->sp = malloc(K * sizeof(int *));
    R->sp[0] = pre;
    for (int k = 1; k < K; k++) {
        R->sp[k] = malloc(n * sizeof(int));
        for (int i = 0; i + (1 << k) <= n; i++) R->sp[k][i] = shallower(t, R->sp[k - 1][i], R->sp[k - 1][i + (1 << (k - 1))]);
    }
    free(stack);
}
static int rmq_lca(const rmq_t *R, int u, int v) {
    if (u == v) return u;
    int l = R->tin[u], r = R->tin[v]; if (l > r) { int x = l; l = r; r = x; }
    l++;                                           // range (tin[u], tin[v]]
    int k = R->lg[r - l + 1];
    return R->t->par[shallower(R->t, R->sp[k][l], R->sp[k][r - (1 << k) + 1])];
}

/* ---------- Heavy-light decomposition: path queries as O(log n) contiguous ranges ---------- */
typedef struct { const tree_t *t; int *head, *pos; long long *bit; } hld_t;     // bit: Fenwick tree over positions
static void bit_add(long long *b, int n, int i, long long d) { for (i++; i <= n; i += i & -i) b[i] += d; }
static long long bit_sum(const long long *b, int i) { long long s = 0; for (i++; i > 0; i -= i & -i) s += b[i]; return s; }   // sum of [0..i]

static void hld_build(hld_t *H, const tree_t *t) {
    int n = t->n; H->t = t; H->head = malloc(n * sizeof(int)); H->pos = malloc(n * sizeof(int)); H->bit = calloc(n + 1, sizeof(long long));
    int cur = 0;
    for (int i = 0; i < n; i++) {
        int v = t->order[i];
        if (t->par[v] >= 0 && t->heavy[t->par[v]] == v) continue;       // not the start of a chain
        for (int w = v; w >= 0; w = t->heavy[w]) { H->head[w] = v; H->pos[w] = cur++; }   // lay a whole chain out contiguously
    }
}
static void hld_add(hld_t *H, int v, long long d) { bit_add(H->bit, H->t->n, H->pos[v], d); }
// sum of vertex values on the path u..v (inclusive)
static long long hld_path_sum(const hld_t *H, int u, int v) {
    const tree_t *t = H->t; long long s = 0;
    while (H->head[u] != H->head[v]) {
        if (t->depth[H->head[u]] < t->depth[H->head[v]]) { int x = u; u = v; v = x; }
        s += bit_sum(H->bit, H->pos[u]) - bit_sum(H->bit, H->pos[H->head[u]] - 1);   // whole chain prefix up to u
        u = t->par[H->head[u]];
    }
    if (t->depth[u] > t->depth[v]) { int x = u; u = v; v = x; }
    return s + bit_sum(H->bit, H->pos[v]) - bit_sum(H->bit, H->pos[u] - 1);
}
static int hld_lca(const hld_t *H, int u, int v) {          // HLD also gives an LCA with no extra tables
    const tree_t *t = H->t;
    while (H->head[u] != H->head[v]) {
        if (t->depth[H->head[u]] < t->depth[H->head[v]]) { int x = u; u = v; v = x; }
        u = t->par[H->head[u]];
    }
    return t->depth[u] < t->depth[v] ? u : v;
}

/* ---------- Centroid decomposition: nearest marked vertex, with updates ---------- */
typedef struct { int n, *cpar, *level; int **dist; int *best; int maxlevel; } cent_t;   // dist[l][v]: distance from v to its level-l centroid ancestor
#define INF 0x3fffffff
static void cent_build(cent_t *C, const tree_t *t) {
    int n = t->n; C->n = n; C->cpar = malloc(n * sizeof(int)); C->level = malloc(n * sizeof(int)); C->best = malloc(n * sizeof(int));
    int L = 1; while ((1 << L) <= n) L++; L += 1; C->maxlevel = L;
    C->dist = malloc(L * sizeof(int *)); for (int l = 0; l < L; l++) C->dist[l] = malloc(n * sizeof(int));
    char *removed = calloc(n, 1);
    int *q = malloc(n * sizeof(int)), *p = malloc(n * sizeof(int)), *sz = malloc(n * sizeof(int));
    int *wstart = malloc(n * sizeof(int)), *wpar = malloc(n * sizeof(int)), *wlev = malloc(n * sizeof(int)), ws = 0;   // explicit work stack
    wstart[ws] = 0; wpar[ws] = -1; wlev[ws] = 0; ws++;
    while (ws) {
        ws--; int s = wstart[ws], cp = wpar[ws], lev = wlev[ws];
        int h = 0, tl = 0; q[tl++] = s; p[s] = -1;                            // BFS over the current component
        while (h < tl) { int v = q[h++]; for (int e = t->adj_start[v]; e < t->adj_start[v + 1]; e++) { int w = t->adj[e]; if (!removed[w] && w != p[v]) { p[w] = v; q[tl++] = w; } } }
        for (int i = tl - 1; i >= 0; i--) { int v = q[i]; sz[v] = 1; }
        for (int i = tl - 1; i > 0; i--) sz[p[q[i]]] += sz[q[i]];
        int c = s;                                                            // walk toward the heavy side until balanced
        for (;;) { int nx = -1; for (int e = t->adj_start[c]; e < t->adj_start[c + 1]; e++) { int w = t->adj[e]; if (!removed[w] && w != p[c] && sz[w] * 2 > tl) nx = w; } if (nx < 0) break; c = nx; }
        C->cpar[c] = cp; C->level[c] = lev; C->best[c] = INF;
        h = 0; tl = 0; q[tl++] = c; p[c] = -1; C->dist[lev][c] = 0;           // distances from the centroid to its whole component
        while (h < tl) { int v = q[h++]; for (int e = t->adj_start[v]; e < t->adj_start[v + 1]; e++) { int w = t->adj[e]; if (!removed[w] && w != p[v]) { p[w] = v; C->dist[lev][w] = C->dist[lev][v] + 1; q[tl++] = w; } } }
        removed[c] = 1;
        for (int e = t->adj_start[c]; e < t->adj_start[c + 1]; e++) { int w = t->adj[e]; if (!removed[w]) { wstart[ws] = w; wpar[ws] = c; wlev[ws] = lev + 1; ws++; } }
    }
    free(removed); free(q); free(p); free(sz); free(wstart); free(wpar); free(wlev);
}
static void cent_mark(cent_t *C, int v) {
    for (int a = v; a >= 0; a = C->cpar[a]) { int d = C->dist[C->level[a]][v]; if (d < C->best[a]) C->best[a] = d; }
}
static int cent_nearest(const cent_t *C, int v) {              // distance to the nearest marked vertex (INF if none)
    int r = INF;
    for (int a = v; a >= 0; a = C->cpar[a]) if (C->best[a] < INF) { int d = C->best[a] + C->dist[C->level[a]][v]; if (d < r) r = d; }
    return r;
}
