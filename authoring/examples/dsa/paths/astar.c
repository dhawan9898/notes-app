#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

/* Shortest path on a 4-connected grid with obstacles: BFS vs Dijkstra-with-heap vs A* (Manhattan heuristic).
 * All unit-cost moves, so BFS is exact; the point is HOW MANY cells each algorithm has to expand. */
typedef struct { int f, cell; } Item;
static void up(Item *h, size_t i) { Item x = h[i]; while (i && h[(i - 1) / 2].f > x.f) { h[i] = h[(i - 1) / 2]; i = (i - 1) / 2; } h[i] = x; }
static void down(Item *h, size_t n, size_t i) {
    Item x = h[i];
    for (;;) { size_t c = 2 * i + 1; if (c >= n) break; if (c + 1 < n && h[c + 1].f < h[c].f) c++; if (x.f <= h[c].f) break; h[i] = h[c]; i = c; }
    h[i] = x;
}

static int W, H; static char *wall;
static int manhattan(int a, int b) { return abs(a % W - b % W) + abs(a / W - b / W); }

/* weighted search with heuristic weight hw: hw = 0 -> Dijkstra, hw = 1 -> A* (consistent heuristic -> optimal) */
static int search(int src, int dst, int hw, size_t *expanded) {
    int N = W * H; int *g = malloc((size_t)N * sizeof *g); char *closed = calloc((size_t)N, 1); for (int i = 0; i < N; i++) g[i] = 1 << 30;
    Item *heap = malloc((size_t)N * 4 * sizeof *heap);
    size_t n = 0;
    *expanded = 0;
    g[src] = 0; heap[n++] = (Item){ hw * manhattan(src, dst), src };
    static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    int ans = -1;
    while (n) {
        Item top = heap[0]; heap[0] = heap[--n]; if (n) down(heap, n, 0);
        if (closed[top.cell]) continue;
        closed[top.cell] = 1;
        (*expanded)++;
        if (top.cell == dst) {
            ans = g[dst];
            break;
        }
        int x = top.cell % W, y = top.cell / W;
        for (int d = 0; d < 4; d++) {
            int nx = x + dx[d], ny = y + dy[d]; if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            int v = ny * W + nx; if (wall[v] || closed[v]) continue;
            if (g[top.cell] + 1 < g[v]) { g[v] = g[top.cell] + 1; heap[n] = (Item){ g[v] + hw * manhattan(v, dst), v }; up(heap, n++); }
        }
    }
    free(g); free(closed); free(heap); return ans;
}
static int bfs(int src, int dst, size_t *expanded) {
    int N = W * H; int *dist = malloc((size_t)N * sizeof *dist), *q = malloc((size_t)N * sizeof *q); for (int i = 0; i < N; i++) dist[i] = -1;
    int h = 0, t = 0; q[t++] = src; dist[src] = 0; *expanded = 0; int ans = -1;
    static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
    while (h < t) {
        int u = q[h++]; (*expanded)++; if (u == dst) { ans = dist[u]; break; }
        for (int d = 0; d < 4; d++) { int nx = u % W + dx[d], ny = u / W + dy[d]; if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue; int v = ny * W + nx; if (!wall[v] && dist[v] < 0) { dist[v] = dist[u] + 1; q[t++] = v; } }
    }
    free(dist); free(q); return ans;
}

int main(void) {
    W = H = 2000; wall = calloc((size_t)W * H, 1); srand(5);
    printf("%dx%d grid, source top-left, target bottom-right; each row is a different obstacle density\n\n", W, H);
    printf("%-10s %-10s | %9s | %12s %8s | %12s %8s | %12s %8s\n", "obstacles", "", "path len", "BFS cells", "ms", "Dijkstra", "ms", "A*", "ms");
    int densities[] = { 0, 20, 30, 35 };
    for (int di = 0; di < 4; di++) {
        for (int i = 0; i < W * H; i++) wall[i] = rand() % 100 < densities[di];
        wall[0] = wall[1] = wall[W] = wall[W * H - 1] = wall[W * H - 2] = wall[W * H - 1 - W] = 0;   /* keep the endpoints' neighbours open */
        size_t e1, e2, e3; double t0;
        t0 = now(); int a = bfs(0, W * H - 1, &e1); double t1 = now() - t0;
        t0 = now(); int b = search(0, W * H - 1, 0, &e2); double t2 = now() - t0;
        t0 = now(); int c = search(0, W * H - 1, 1, &e3); double t3 = now() - t0;
        assert(a == b && b == c);                                         /* the same optimal length, found three ways */
        printf("%8d %% %-10s | %9d | %12zu %8.0f | %12zu %8.0f | %12zu %8.0f\n", densities[di], "", a, e1, t1 * 1e3, e2, t2 * 1e3, e3, t3 * 1e3);
    }
    printf("\n(path length -1 means the target is unreachable; equal lengths in every row were asserted)\n");
    free(wall); return 0;
}
