int sum_first_n(const int *v, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) total += v[i];
    return total;
}
