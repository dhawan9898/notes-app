int topo_order(int edge[N][N], int order[N]) {
    int indeg[N] = {0}, queue[N], head = 0, tail = 0;      /* Kahn: repeatedly take a task with no pending prerequisite */
    for (int a = 0; a < N; a++)
        for (int b = 0; b < N; b++) if (edge[a][b]) indeg[b]++;
    for (int v = 0; v < N; v++) if (indeg[v] == 0) queue[tail++] = v;
    while (head < tail) {
        int v = queue[head++];
        order[head - 1] = v;
        for (int w = 0; w < N; w++)
            if (edge[v][w] && --indeg[w] == 0) queue[tail++] = w;
    }
    return tail == N;                                       /* fewer than N placed: the rest are on a cycle */
}
