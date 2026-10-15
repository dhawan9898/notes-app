int dedupe(int *a, int n) {
    if (n == 0) return 0;
    int w = 1;                              /* a[0 .. w-1] holds the distinct values seen so far */
    for (int r = 1; r < n; r++)
        if (a[r] != a[w - 1]) a[w++] = a[r];
    return w;
}
