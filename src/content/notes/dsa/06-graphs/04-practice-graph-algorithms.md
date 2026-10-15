---
title: "Practice: Graph Algorithms"
track: dsa
module: Graphs
moduleOrder: 6
order: 4
level: advanced
summary: "Four exercises on the graphs module: counting islands in a grid with flood fill, the shortest path through a maze with breadth-first search, ordering dependent tasks with a topological sort that detects cycles, and Dijkstra's algorithm on a small weighted graph, each with a verified C answer."
prerequisites:
  - "Graph Representations and Traversals"
  - "Shortest Paths: Dijkstra, Bellman-Ford, Floyd-Warshall and A*"
  - "Union-Find, Spanning Trees, Connectivity and Max-Flow"
sources:
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 20 (Elementary Graph Algorithms) and Ch. 22 (Single-Source Shortest Paths)"
  - label: "Kahn, A. B. — Topological sorting of large networks (CACM, 1962)"
  - label: "Skiena — The Algorithm Design Manual, 3rd ed., Ch. 7-8"
---

Many "different" problems are the same graph problem in disguise: a grid is a graph, a set of tasks with prerequisites is a graph, a road map is a graph. Each exercise below asks you to recognise which graph algorithm applies. (Answers were also cross-checked against an independent Python implementation.)

## Exercises

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Islands.** A 6×8 grid contains `1` (land) and `0` (water). Count the islands (groups of land cells connected up, down, left or right) and the size of the largest. Which traversal do you use, and what happens to your marks if you reuse the grid afterwards?

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 1: count the islands (4-connected groups of 1s) and the size of the largest.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXR 8
#define MAXC 8

/* ===== Your code: edit below this line ===== */
/* grid has rows strings of cols characters '0' or '1'; you may change it (for example to mark visited cells). */
void islands(char grid[][MAXC + 1], int rows, int cols, int *count, int *largest) {
    (void)grid; (void)rows; (void)cols;
    *count = 0;
    *largest = 0;
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

int main(void) {
    char g[6][MAXC + 1] = {"11000111", "11000101", "00100000", "00011100", "10011100", "10000001"};
    int n = -1, big = -1;
    islands(g, 6, 8, &n, &big);
    check_int("islands in the 6x8 grid", n, 6);
    check_int("  largest island", big, 6);
    char diag[2][MAXC + 1] = {"10", "01"};
    islands(diag, 2, 2, &n, &big);
    check_int("islands in {10, 01} (diagonals do not connect)", n, 2);
    char water[1][MAXC + 1] = {"000"};
    islands(water, 1, 3, &n, &big);
    check_int("islands in all water", n, 0);
    return summary();
}
```

</details>

**2. Shortest path in a maze.** `S` is the start, `E` the exit, `#` a wall and `.` a free cell; moves go up, down, left or right at cost 1. Find the number of steps of the shortest path. Why does BFS give the shortest path here and DFS does not?

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 2: the length of the shortest path from S to E in a grid maze (breadth-first search).
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Moves go up, down, left or right; '#' is a wall. Return the number of steps, or -1 if E cannot be reached. */
int shortest_path(const char *maze[], int rows, int cols) {
    (void)maze; (void)rows; (void)cols;
    return -1;
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

int main(void) {
    const char *maze[] = {"S.#......", ".##.####.", "....#....", ".####.##.", "......#.E"};
    check_int("shortest_path(5x9 maze)", shortest_path(maze, 5, 9), 16);
    const char *next_to[] = {"SE"};
    check_int("shortest_path(\"SE\")", shortest_path(next_to, 1, 2), 1);
    const char *walled[] = {"S#E"};
    check_int("shortest_path(\"S#E\") (unreachable)", shortest_path(walled, 1, 3), -1);
    const char *open[] = {"S...", "....", "...E"};
    check_int("shortest_path(open 3x4 room)", shortest_path(open, 3, 4), 5);
    return summary();
}
```

</details>

**3. Dependency order.** Six morning tasks have prerequisites (wake before shower before dress before leave, wake before coffee before breakfast before leave). Output a valid order with a topological sort. What does your algorithm do if you add "leave before wake"?

<details class="starter">
<summary>Starter program with tests</summary>

```c
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
```

</details>

**4. Weighted shortest paths.** For the 5-vertex weighted directed graph in the answer program, compute the shortest distance from vertex 0 to every vertex with Dijkstra's algorithm. Why does Dijkstra fail on graphs with negative edge weights?

<details class="starter">
<summary>Starter program with tests</summary>

```c
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
```

</details>

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

```c
#include <stdio.h>
#include <string.h>

#define R 6
#define C 8

static char grid[R][C + 1] = {
    "11000111",
    "11000101",
    "00100000",
    "00011100",
    "10011100",
    "10000001",
};

/* Flood fill with an explicit stack: marks one island, returns its size. */
static int fill(int r0, int c0) {
    int stack[R * C][2], top = 0, size = 0;
    stack[top][0] = r0; stack[top][1] = c0; top++;
    grid[r0][c0] = '0';
    while (top) {
        top--; int r = stack[top][0], c = stack[top][1]; size++;
        const int dr[4] = {1, -1, 0, 0}, dc[4] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            int nr = r + dr[d], nc = c + dc[d];
            if (nr >= 0 && nr < R && nc >= 0 && nc < C && grid[nr][nc] == '1') { grid[nr][nc] = '0'; stack[top][0] = nr; stack[top][1] = nc; top++; }
        }
    }
    return size;
}

int main(void) {
    int islands = 0, biggest = 0;
    for (int r = 0; r < R; r++)
        for (int c = 0; c < C; c++)
            if (grid[r][c] == '1') { int s = fill(r, c); islands++; if (s > biggest) biggest = s; }
    printf("islands (4-connected groups of 1s): %d, largest has %d cells\n", islands, biggest);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p6_grid p6_grid.c && ./p6_grid; rm -f p6_grid
islands (4-connected groups of 1s): 6, largest has 6 cells
```

Scan every cell; each time you meet an unvisited land cell you have found a *new* island, so count it and flood-fill (DFS with an explicit stack, or BFS) to mark all connected land as visited. The program marks cells by overwriting `1` with `0`, which **destroys the input**; use a separate visited array if the grid is needed later. Each cell is pushed at most once (it is marked when pushed, not when popped, which prevents duplicates on the stack), so the cost is O(rows × columns). The explicit stack avoids deep recursion: a large all-land grid would otherwise overflow the call stack. The island sizes are 4, 5, 1, 6, 2 and 1.

**2.**

```c
#include <stdio.h>
#include <string.h>

#define R 5
#define C 9

static const char *maze[R] = {
    "S.#......",
    ".##.####.",
    "....#....",
    ".####.##.",
    "......#.E",
};

int main(void) {
    int dist[R][C], q[R * C][2], head = 0, tail = 0, er = 0, ec = 0;
    memset(dist, -1, sizeof dist);
    for (int r = 0; r < R; r++) for (int c = 0; c < C; c++) {
        if (maze[r][c] == 'S') { dist[r][c] = 0; q[tail][0] = r; q[tail][1] = c; tail++; }
        if (maze[r][c] == 'E') { er = r; ec = c; }
    }
    const int dr[4] = {1, -1, 0, 0}, dc[4] = {0, 0, 1, -1};
    while (head < tail) {                                          /* BFS: cells come off the queue in order of distance */
        int r = q[head][0], c = q[head][1]; head++;
        for (int d = 0; d < 4; d++) {
            int nr = r + dr[d], nc = c + dc[d];
            if (nr < 0 || nr >= R || nc < 0 || nc >= C || maze[nr][nc] == '#' || dist[nr][nc] >= 0) continue;
            dist[nr][nc] = dist[r][c] + 1; q[tail][0] = nr; q[tail][1] = nc; tail++;
        }
    }
    printf("shortest path from S to E: %d steps\n", dist[er][ec]);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p6_maze p6_maze.c && ./p6_maze; rm -f p6_maze
shortest path from S to E: 16 steps
```

BFS explores in rings: every cell at distance *d* is dequeued before any cell at distance *d + 1*, so the first time a cell is reached it is by a shortest path, and `dist[][] >= 0` doubles as the visited marker. DFS dives down one path to the end and may reach `E` by a long detour first, so its first success is not minimal. BFS finds shortest paths only when every edge has the **same cost**; with different costs use Dijkstra. Time and space are O(cells).

**3.**

```c
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
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p6_topo p6_topo.c && ./p6_topo; rm -f p6_topo
plan: wake -> shower -> coffee -> dress -> breakfast -> leave
with leave -> wake: impossible, there is a cycle
```

Kahn's algorithm keeps an *in-degree* (number of unmet prerequisites) per task. It repeatedly takes any task with in-degree 0, outputs it, and decrements its successors' counts. If the output ends up with fewer tasks than the graph has, the remaining ones wait on each other: a **cycle**, so no valid order exists (the second run reports it). A topological order exists exactly when the graph is a DAG (directed acyclic), and it is usually not unique: this run put `coffee` before `dress`, but `dress` first is equally valid. Time is O(V + E).

**4.**

```c
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
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p6_dijkstra p6_dijkstra.c && ./p6_dijkstra; rm -f p6_dijkstra
distance 0 -> 0: 0
distance 0 -> 1: 3
distance 0 -> 2: 1
distance 0 -> 3: 4
distance 0 -> 4: 7
direct edge 0->1 costs 4, but 0->2->1 costs 3: Dijkstra found the cheaper route
```

Dijkstra repeatedly finalises the unfinished vertex with the smallest known distance and *relaxes* its outgoing edges. The direct edge 0→1 costs 4 but going 0→2→1 costs 1 + 2 = 3, so the distances are 0, 3, 1, 4 and 7. This version scans all vertices to find the minimum, so it is O(V²); with a binary heap (see the heap exercise) it is O((V + E) log V). The correctness argument needs **non-negative weights**: once a vertex is finalised, no later path can be shorter, because every extra edge only adds cost. A negative edge could make a longer-looking path cheaper after the vertex was already finalised, so use **Bellman-Ford** (O(VE), also detects negative cycles) instead.

</details>

## Key takeaways

1. Recognise the model first: grids and task lists are graphs; flood fill finds connected components in O(cells).
2. BFS gives shortest paths when all edges cost the same; Dijkstra when costs are non-negative; Bellman-Ford when negative edges exist.
3. Kahn's topological sort orders a DAG in O(V + E) and detects cycles by emitting fewer than V nodes.
4. Mark nodes as visited when you *enqueue or push* them, not when you remove them, to avoid duplicate work.
