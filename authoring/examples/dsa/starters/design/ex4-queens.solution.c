static long long place(int n, int row, int cols, int diag1, int diag2) {
    if (row == n) return 1;
    long long total = 0;
    for (int c = 0; c < n; c++) {
        int d1 = row + c, d2 = row - c + n - 1;
        if ((cols >> c & 1) || (diag1 >> d1 & 1) || (diag2 >> d2 & 1)) continue;   /* prune: attacked square */
        total += place(n, row + 1, cols | 1 << c, diag1 | 1 << d1, diag2 | 1 << d2);
    }
    return total;
}

long long count_queens(int n) {
    return place(n, 0, 0, 0, 0);
}
