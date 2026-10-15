static int fill(char grid[][MAXC + 1], int rows, int cols, int r, int c) {
    if (r < 0 || r >= rows || c < 0 || c >= cols || grid[r][c] != '1') return 0;
    grid[r][c] = '0';
    int size = 1;
    for (int dr = -1; dr <= 1; dr++)                    /* 8 neighbours: diagonals wrongly connect */
        for (int dc = -1; dc <= 1; dc++) size += fill(grid, rows, cols, r + dr, c + dc);
    return size;
}

void islands(char grid[][MAXC + 1], int rows, int cols, int *count, int *largest) {
    *count = 0;
    *largest = 0;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            if (grid[r][c] == '1') { int s = fill(grid, rows, cols, r, c); ++*count; if (s > *largest) *largest = s; }
}
