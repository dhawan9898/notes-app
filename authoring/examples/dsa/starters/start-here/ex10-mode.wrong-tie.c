int mode_of(const int *a, int n) {
    int count[100] = {0}, best = 0;
    for (int i = 0; i < n; i++) count[a[i]]++;
    for (int v = 1; v < 100; v++)
        if (count[v] >= count[best]) best = v;      /* >= keeps the largest value on a tie */
    return best;
}
