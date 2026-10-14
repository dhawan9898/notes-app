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
