#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* 2D prefix sums (summed-area table) and the difference array: the two offline tricks that need no tree. */
enum { R = 300, C = 400 };
static int grid[R][C];
static long long P[R + 1][C + 1];                          /* P[i][j] = sum of grid[0..i)[0..j) */

static long long rect(int r1, int c1, int r2, int c2) {    /* sum over rows [r1, r2), columns [c1, c2) in four lookups */
    return P[r2][c2] - P[r1][c2] - P[r2][c1] + P[r1][c1];
}

int main(void) {
    srand(7);
    for (int i = 0; i < R; i++) for (int j = 0; j < C; j++) grid[i][j] = rand() % 100 - 20;
    for (int i = 0; i < R; i++) for (int j = 0; j < C; j++) P[i + 1][j + 1] = grid[i][j] + P[i][j + 1] + P[i + 1][j] - P[i][j];   /* inclusion-exclusion */
    for (int t = 0; t < 2000; t++) {
        int r1 = rand() % R, r2 = r1 + 1 + rand() % (R - r1), c1 = rand() % C, c2 = c1 + 1 + rand() % (C - c1); long long want = 0;
        for (int i = r1; i < r2; i++) for (int j = c1; j < c2; j++) want += grid[i][j];
        assert(rect(r1, c1, r2, c2) == want);
    }
    printf("2D prefix sums: 2000 random rectangle sums match brute force (%d x %d grid; each query is 4 lookups)\n", R, C);

    /* difference array: apply many range additions in O(1) each, materialize once with a prefix sum */
    enum { N = 100000, UPD = 50000 };
    static int diff[N + 1], direct[N], via[N];
    for (int u = 0; u < UPD; u++) {
        int l = rand() % N, r = l + 1 + rand() % (N - l), v = rand() % 11 - 5;
        for (int i = l; i < r; i++) direct[i] += v;       /* O(length) */
        diff[l] += v; diff[r] -= v;                       /* O(1) */
    }
    int run = 0; for (int i = 0; i < N; i++) { run += diff[i]; via[i] = run; }
    for (int i = 0; i < N; i++) assert(via[i] == direct[i]);
    printf("difference array: %d range additions applied in O(1) each, final array identical to the O(length) version\n", UPD);
    return 0;
}
