long long hanoi_moves(int n) {
    return n == 0 ? 0 : 2 * hanoi_moves(n - 1) + 1;     /* move n-1 aside, move the largest, move n-1 back on top */
}

void hanoi(int n, int from, int to, int via) {
    if (n == 0) return;
    hanoi(n - 1, from, via, to);
    move(from, to);
    hanoi(n - 1, via, to, from);
}
