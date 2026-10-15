void dijkstra(int w[N][N], int src, int dist[N]) {
    int done[N] = {0};
    for (int v = 0; v < N; v++) dist[v] = INF;
    dist[src] = 0;
    for (int round = 0; round < N; round++) {           /* O(V^2): finalise the closest unfinished vertex */
        int u = -1;
        for (int v = 0; v < N; v++)
            if (!done[v] && dist[v] != INF && (u < 0 || dist[v] < dist[u])) u = v;
        if (u < 0) break;
        done[u] = 1;
        for (int v = 0; v < N; v++)
            if (w[u][v] != INF && dist[u] + w[u][v] < dist[v]) dist[v] = dist[u] + w[u][v];
    }
}
