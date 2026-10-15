static long long sort_count(int *a, int *tmp, int lo, int hi) {
    if (hi - lo < 2) return 0;
    int mid = lo + (hi - lo) / 2;
    long long inv = sort_count(a, tmp, lo, mid) + sort_count(a, tmp, mid, hi);
    int i = lo, j = mid, k = lo;
    while (i < mid && j < hi) {
        if (a[i] <= a[j]) tmp[k++] = a[i++];
        else { tmp[k++] = a[j++]; inv += mid - i; }     /* a[j] is smaller than all of a[i..mid) */
    }
    while (i < mid) tmp[k++] = a[i++];
    while (j < hi) tmp[k++] = a[j++];
    memcpy(a + lo, tmp + lo, (size_t)(hi - lo) * sizeof *a);
    return inv;
}

long long count_inversions(int *a, int *tmp, int n) {
    return sort_count(a, tmp, 0, n);
}
