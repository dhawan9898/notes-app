static int fill(char grid[][MAXC + 1], int rows, int cols, int r, int c) {
    if (r < 0 || r >= rows || c < 0 || c >= cols || grid[r][c] != '1') return 0;
    grid[r][c] = '0';                                   /* sink it so it is counted once */
    return 1 + fill(grid, rows, cols, r + 1, c) + fill(grid, rows, cols, r - 1, c)
             + fill(grid, rows, cols, r, c + 1) + fill(grid, rows, cols, r, c - 1);
}

void islands(char grid[][MAXC + 1], int rows, int cols, int *count, int *largest) {
    *count = 0;
    *largest = 0;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            if (grid[r][c] == '1') {
                int size = fill(grid, rows, cols, r, c);
                ++*count;
                if (size > *largest) *largest = size;
            }
}
