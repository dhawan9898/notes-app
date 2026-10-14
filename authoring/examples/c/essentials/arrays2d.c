#include <stdio.h>

#define ROWS 3
#define COLS 4

int main(void) {
    int grid[ROWS][COLS];                         /* 3 rows of 4 ints, stored row after row in memory */
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            grid[r][c] = r * 10 + c;              /* grid[row][column] */

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) printf("%3d", grid[r][c]);
        printf("\n");
    }
    printf("sizeof grid = %zu bytes = %d rows x %d cols x %zu bytes\n", sizeof grid, ROWS, COLS, sizeof grid[0][0]);

    int *flat = &grid[0][0];                      /* the same memory seen as one long row */
    printf("flat[5] = %d is grid[1][1] = %d\n", flat[5], grid[1][1]);
    return 0;
}
