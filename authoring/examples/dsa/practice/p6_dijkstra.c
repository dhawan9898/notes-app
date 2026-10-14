#include <limits.h>
#include <stdio.h>

#define N 5
#define INF INT_MAX

int main(void) {
    int w[N][N];
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) w[i][j] = INF;
    w[0][1] = 4; w[0][2] = 1; w[2][1] = 2; w[1][3] = 1; w[2][3] = 5; w[3][4] = 3;

    int dist[N], done[N] = {0};
    for (int i = 0; i < N; i++) dist[i] = INF;
    dist[0] = 0;
    for (int round = 0; round < N; round++) {                  /* O(V^2) Dijkstra: finalise the closest unfinished vertex */
        int u = -1;
        for (int i = 0; i < N; i++) if (!done[i] && dist[i] != INF && (u < 0 || dist[i] < dist[u])) u = i;
        if (u < 0) break;
        done[u] = 1;
        for (int v = 0; v < N; v++)
            if (w[u][v] != INF && dist[u] + w[u][v] < dist[v]) dist[v] = dist[u] + w[u][v];
    }
    for (int i = 0; i < N; i++) printf("distance 0 -> %d: %d\n", i, dist[i]);
    printf("direct edge 0->1 costs 4, but 0->2->1 costs %d: Dijkstra found the cheaper route\n", dist[1]);
    return 0;
}
