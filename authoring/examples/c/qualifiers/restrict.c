void add_alias(int *dst, const int *a, const int *b, int n) {
    for (int i = 0; i < n; i++) dst[i] = a[i] + b[i];
}
void add_restrict(int *restrict dst, const int *restrict a, const int *restrict b, int n) {
    for (int i = 0; i < n; i++) dst[i] = a[i] + b[i];
}
