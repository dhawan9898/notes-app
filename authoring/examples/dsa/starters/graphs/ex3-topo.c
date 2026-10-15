// Exercise 3: a valid order for tasks with prerequisites, or detect that none exists.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 6
static const char *name[N] = {"wake", "shower", "dress", "breakfast", "coffee", "leave"};

/* ===== Your code: edit below this line ===== */
/* edge[a][b] == 1 means task a must happen before task b. Fill order[0..N) and return 1, or return 0 if there is a cycle. */
int topo_order(int edge[N][N], int order[N]) {
    (void)edge;
    for (int i = 0; i < N; i++) order[i] = i;
    return 1;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

/* 1 if order is a permutation of 0..N-1 that puts every a before b for each edge a -> b */
static int respects(int edge[N][N], const int order[N]) {
    int pos[N], seen[N] = {0};
    for (int i = 0; i < N; i++) { if (order[i] < 0 || order[i] >= N || seen[order[i]]++) return 0; pos[order[i]] = i; }
    for (int a = 0; a < N; a++)
        for (int b = 0; b < N; b++) if (edge[a][b] && pos[a] > pos[b]) return 0;
    return 1;
}

int main(void) {
    int edge[N][N] = {{0}};
    edge[0][1] = edge[1][2] = edge[2][5] = 1;      /* wake -> shower -> dress -> leave */
    edge[0][4] = edge[4][3] = edge[3][5] = 1;      /* wake -> coffee -> breakfast -> leave */
    int order[N] = {0};
    check_int("topo_order(morning tasks) succeeds", topo_order(edge, order), 1);
    check_int("  and the order respects every prerequisite", respects(edge, order), 1);
    printf("     order:");
    for (int i = 0; i < N; i++) printf(" %s", order[i] >= 0 && order[i] < N ? name[order[i]] : "?");
    printf("\n");
    edge[5][0] = 1;                                /* leave before wake: a cycle */
    check_int("topo_order(with leave -> wake) reports the cycle", topo_order(edge, order), 0);
    return summary();
}
