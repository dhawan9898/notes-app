#include <stdio.h>

#define N 7

/* An undirected graph as an adjacency matrix: adj[a][b] = 1 means a road between a and b. */
static int adj[N][N];

static void road(int a, int b) { adj[a][b] = adj[b][a] = 1; }

int main(void) {
    const char *name[N] = {"Home", "Shop", "School", "Park", "Cafe", "Gym", "Library"};
    road(0, 1); road(0, 2); road(1, 3); road(2, 3); road(3, 4); road(4, 5);   /* Library (6) is unreachable */

    /* Breadth-first search: visit everything 1 step away, then 2 steps, ... using a queue. */
    int dist[N], queue[N], head = 0, tail = 0;
    for (int i = 0; i < N; i++) dist[i] = -1;          /* -1 = not reached yet */
    dist[0] = 0; queue[tail++] = 0;
    while (head < tail) {
        int v = queue[head++];
        for (int w = 0; w < N; w++)
            if (adj[v][w] && dist[w] < 0) { dist[w] = dist[v] + 1; queue[tail++] = w; }
    }
    for (int i = 0; i < N; i++) {
        if (dist[i] < 0) printf("%-8s unreachable from Home\n", name[i]);
        else printf("%-8s %d step(s) from Home\n", name[i], dist[i]);
    }
    return 0;
}
