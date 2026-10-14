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
