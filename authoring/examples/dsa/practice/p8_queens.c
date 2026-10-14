#include <stdio.h>

static int n, solutions;
static int col_used[16], diag1[32], diag2[32];

static void place(int row) {
    if (row == n) { solutions++; return; }
    for (int c = 0; c < n; c++) {
        if (col_used[c] || diag1[row + c] || diag2[row - c + n - 1]) continue;       /* attacked: prune this whole branch */
        col_used[c] = diag1[row + c] = diag2[row - c + n - 1] = 1;                   /* choose */
        place(row + 1);                                                              /* explore */
        col_used[c] = diag1[row + c] = diag2[row - c + n - 1] = 0;                   /* un-choose: backtrack */
    }
}

int main(void) {
    for (n = 4; n <= 8; n++) { solutions = 0; place(0); printf("%d-queens: %d solutions\n", n, solutions); }
    return 0;
}
