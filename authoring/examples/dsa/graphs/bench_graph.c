#define _GNU_SOURCE
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include "graph.h"
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

static void bfs_list(const AList *g, int src, int *dist, int *queue) {
    for (int i = 0; i < g->n; i++) dist[i] = -1;
    int h = 0, t = 0; queue[t++] = src; dist[src] = 0;
    while (h < t) { int u = queue[h++]; for (const ANode *x = g->head[u]; x; x = x->next) if (dist[x->v] < 0) { dist[x->v] = dist[u] + 1; queue[t++] = x->v; } }
}
static void dfs_rec(const CSR *g, int u, char *seen) {
    seen[u] = 1;
    for (int k = g->off[u]; k < g->off[u + 1]; k++) if (!seen[g->adj[k]]) dfs_rec(g, g->adj[k], seen);
}

int main(void) {
    int n = 1000000; size_t m = 8000000;
    Edge *e = malloc(m * sizeof *e); for (size_t i = 0; i < m; i++) e[i] = (Edge){ (int)(rnd() % (unsigned)n), (int)(rnd() % (unsigned)n) };
    int *dist = malloc((size_t)n * sizeof(int)), *q = malloc((size_t)n * sizeof(int));
    printf("random undirected graph: %d vertices, %zu edges (%zu directed arcs), BFS from vertex 0\n\n", n, m, 2 * m);
    printf("%-34s %10s %10s %12s\n", "representation", "build ms", "BFS ms", "memory");
    double t0 = now(); CSR c = csr_build(n, e, m, 0); double tb = now() - t0;
    bfs(&c, 0, dist, q); t0 = now(); for (int r = 0; r < 3; r++) bfs(&c, 0, dist, q); double tr = (now() - t0) / 3;
    int reached = 0; for (int i = 0; i < n; i++) reached += dist[i] >= 0;
    printf("%-34s %10.0f %10.1f %9.1f MB\n", "CSR (2 arrays)", tb * 1e3, tr * 1e3, ((double)(n + 1) * 4 + (double)c.m * 4) / 1e6);
    t0 = now(); AList p = alist_build(n, e, m, 0, 1); tb = now() - t0;
    bfs_list(&p, 0, dist, q); t0 = now(); for (int r = 0; r < 3; r++) bfs_list(&p, 0, dist, q); tr = (now() - t0) / 3;
    printf("%-34s %10.0f %10.1f %9.1f MB\n", "linked list, pooled nodes", tb * 1e3, tr * 1e3, ((double)n * 8 + (double)(2 * m) * 16) / 1e6);
    alist_free(&p, 1);
    t0 = now(); AList l = alist_build(n, e, m, 0, 0); tb = now() - t0;
    bfs_list(&l, 0, dist, q); t0 = now(); for (int r = 0; r < 3; r++) bfs_list(&l, 0, dist, q); tr = (now() - t0) / 3;
    printf("%-34s %10.0f %10.1f %9.1f MB (+malloc overhead)\n", "linked list, malloc per edge", tb * 1e3, tr * 1e3, ((double)n * 8 + (double)(2 * m) * 16) / 1e6);
    alist_free(&l, 0);
    printf("%-34s %10s %10s %9.1f GB\n", "adjacency matrix (1 byte/pair)", "-", "-", (double)n * (double)n / 1e9);
    printf("BFS reached %d of %d vertices, max distance ", reached, n);
    int mx = 0; for (int i = 0; i < n; i++) if (dist[i] > mx) mx = dist[i]; printf("%d\n", mx);
    csr_free(&c); free(e); free(dist); free(q);

    /* recursion depth: a path graph 0-1-2-...-(L-1) */
    printf("\nrecursive DFS on a path graph with L vertices, default 8 MB stack (run in a child process):\n");
    for (int L = 10000; L <= 1000000; L *= 10) {
        Edge *pe = malloc((size_t)(L - 1) * sizeof *pe); for (int i = 0; i + 1 < L; i++) pe[i] = (Edge){ i, i + 1 };
        CSR pg = csr_build(L, pe, (size_t)(L - 1), 0); char *seen = calloc((size_t)L, 1);
        fflush(stdout); pid_t pid = fork();
        if (pid == 0) { dfs_rec(&pg, 0, seen); _exit(0); }
        int st; waitpid(pid, &st, 0);
        printf("  L = %8d: %s\n", L, WIFSIGNALED(st) ? "crashed (stack overflow, SIGSEGV)" : "ok");
        free(pe); free(seen); csr_free(&pg);
    }
    return 0;
}
