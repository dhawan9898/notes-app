#include <stdio.h>
#include <string.h>

#define N 6

static const char *name[N] = {"wake", "shower", "dress", "breakfast", "coffee", "leave"};
static int edge[N][N];                      /* edge[a][b] = 1: a must happen before b */

static int topo_order(int out[N]) {         /* Kahn's algorithm: repeatedly take a task with no unmet prerequisites */
    int indeg[N] = {0}, queue[N], head = 0, tail = 0, count = 0;
    for (int a = 0; a < N; a++) for (int b = 0; b < N; b++) if (edge[a][b]) indeg[b]++;
    for (int v = 0; v < N; v++) if (indeg[v] == 0) queue[tail++] = v;
    while (head < tail) {
        int v = queue[head++]; out[count++] = v;
        for (int w = 0; w < N; w++) if (edge[v][w] && --indeg[w] == 0) queue[tail++] = w;
    }
    return count == N;                      /* fewer than N tasks emitted means a cycle blocked the rest */
}

static void report(const char *title) {
    int order[N];
    printf("%s: ", title);
    if (!topo_order(order)) { printf("impossible, there is a cycle\n"); return; }
    for (int i = 0; i < N; i++) printf("%s%s", name[order[i]], i < N - 1 ? " -> " : "\n");
}

int main(void) {
    edge[0][1] = edge[1][2] = edge[2][5] = 1;            /* wake -> shower -> dress -> leave */
    edge[0][4] = edge[4][3] = edge[3][5] = 1;            /* wake -> coffee -> breakfast -> leave */
    report("plan");
    edge[5][0] = 1;                                      /* 'leave' must come before 'wake': a circular dependency */
    report("with leave -> wake");
    return 0;
}
