int shortest_path(const char *maze[], int rows, int cols) {
    int dist[32][32], qr[1024], qc[1024], head = 0, tail = 0;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) {
            dist[r][c] = -1;
            if (maze[r][c] == 'S') { dist[r][c] = 0; qr[tail] = r; qc[tail++] = c; }
        }
    const int dr[4] = {1, -1, 0, 0}, dc[4] = {0, 0, 1, -1};
    while (head < tail) {                               /* BFS: cells leave the queue in order of distance */
        int r = qr[head], c = qc[head++];
        if (maze[r][c] == 'E') return dist[r][c];
        for (int d = 0; d < 4; d++) {
            int nr = r + dr[d], nc = c + dc[d];
            if (nr < 0 || nr >= rows || nc < 0 || nc >= cols || maze[nr][nc] == '#' || dist[nr][nc] >= 0) continue;
            dist[nr][nc] = dist[r][c] + 1;
            qr[tail] = nr; qc[tail++] = nc;
        }
    }
    return -1;
}
