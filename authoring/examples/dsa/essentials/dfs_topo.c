#include <stdio.h>

#define N 7
static const char *name[N] = {"A", "B", "C", "D", "E", "F", "G"};
static int adj[N][N];

static int visited[N], order[N], pos;

/* Depth-first search: go as deep as possible along one path before backing up. Recursion is the natural way to write it. */
static void dfs(int v, int depth) {
    visited[v] = 1;
    printf("%*svisit %s\n", depth * 2, "", name[v]);
    for (int w = 0; w < N; w++) if (adj[v][w] && !visited[w]) dfs(w, depth + 1);
}

/* The same traversal records a topological order: a vertex is finished only after everything it points to is finished. */
static void finish_order(int v) {
    visited[v] = 1;
    for (int w = 0; w < N; w++) if (adj[v][w] && !visited[w]) finish_order(w);
    order[pos++] = v;                                   /* post-order: after all descendants */
}

int main(void) {
    /* A -> B, A -> C, B -> D, C -> D, D -> E, F -> G (two separate pieces) */
    adj[0][1] = adj[0][2] = adj[1][3] = adj[2][3] = adj[3][4] = adj[5][6] = 1;

    printf("depth-first search from A (indentation = depth):\n");
    dfs(0, 0);
    printf("vertices never reached from A: ");
    for (int v = 0; v < N; v++) if (!visited[v]) printf("%s ", name[v]);
    printf("(a separate component)\n\n");

    for (int v = 0; v < N; v++) visited[v] = 0;
    for (int v = 0; v < N; v++) if (!visited[v]) finish_order(v);
    printf("task order (reverse of finishing order):");
    for (int i = N - 1; i >= 0; i--) printf(" %s", name[order[i]]);
    printf("\n");
    return 0;
}
