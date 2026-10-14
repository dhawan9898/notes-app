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

**1. Islands.** A 6×8 grid contains `1` (land) and `0` (water). Count the islands (groups of land cells connected up, down, left or right) and the size of the largest. Which traversal do you use, and what happens to your marks if you reuse the grid afterwards?

**2. Shortest path in a maze.** `S` is the start, `E` the exit, `#` a wall and `.` a free cell; moves go up, down, left or right at cost 1. Find the number of steps of the shortest path. Why does BFS give the shortest path here and DFS does not?

**3. Dependency order.** Six morning tasks have prerequisites (wake before shower before dress before leave, wake before coffee before breakfast before leave). Output a valid order with a topological sort. What does your algorithm do if you add "leave before wake"?

**4. Weighted shortest paths.** For the 5-vertex weighted directed graph in the answer program, compute the shortest distance from vertex 0 to every vertex with Dijkstra's algorithm. Why does Dijkstra fail on graphs with negative edge weights?

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
