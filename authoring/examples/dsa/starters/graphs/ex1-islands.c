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
