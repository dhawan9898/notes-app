int two_sum_sorted(const int *a, int n, int target, int *i_out, int *j_out) {
    int i = 0, j = n - 1;
    while (i < j) {
        int s = a[i] + a[j];
        if (s == target) { *i_out = i; *j_out = j; return 1; }
        if (s < target) i++; else j--;
    }
    return 0;
}
