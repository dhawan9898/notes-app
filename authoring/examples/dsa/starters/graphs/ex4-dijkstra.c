// Exercise 4: shortest distances from one vertex with Dijkstra's algorithm.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 5
#define INF INT_MAX     /* no edge / unreachable */

/* ===== Your code: edit below this line ===== */
/* w[u][v] is the weight of edge u -> v (>= 0) or INF. Fill dist[v] with the shortest distance from src, INF if unreachable. */
void dijkstra(int w[N][N], int src, int dist[N]) {
    for (int v = 0; v < N; v++) dist[v] = w[src][v];    /* direct edges only */
    dist[src] = 0;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static void print_arr(const int *v, int n) {
    printf("{");
    for (int i = 0; i < n; i++) printf(i ? ", %d" : "%d", v[i]);
    printf("}");
}

static void check_arr(const char *what, const int *got, int got_n, const int *want, int want_n) {
    int same = got_n == want_n;
    for (int i = 0; same && i < want_n; i++) same = got[i] == want[i];
    checks++;
    if (!same) failed++;
    printf("%s %s", same ? "PASS" : "FAIL", what);
    if (same) { printf(" = "); print_arr(got, got_n); }
    else { printf(": expected "); print_arr(want, want_n); printf(", got "); print_arr(got, got_n); }
    printf("\n");
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    int w[N][N];
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) w[i][j] = INF;
    w[0][1] = 4; w[0][2] = 1; w[2][1] = 2; w[1][3] = 1; w[2][3] = 5; w[3][4] = 3;
    int dist[N];
    dijkstra(w, 0, dist);
    const int want[N] = {0, 3, 1, 4, 7};
    check_arr("dist from 0", dist, N, want, N);
    dijkstra(w, 3, dist);
    check_int("dist from 3 to 4", dist[4], 3);
    check_int("dist from 3 to 0 (unreachable) is INF", dist[0] == INF, 1);
    return summary();
}
