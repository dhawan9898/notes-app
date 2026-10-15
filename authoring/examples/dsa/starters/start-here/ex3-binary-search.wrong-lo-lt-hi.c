int bsearch_idx(const int *a, int n, int key) {
    int lo = 0, hi = n - 1;
    while (lo < hi) {               /* stops one step early: the last candidate is never compared */
        int mid = lo + (hi - lo) / 2;
        if (a[mid] == key) return mid;
        if (a[mid] < key) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}
