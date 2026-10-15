static void sift_down(int *h, int n, int i) {
    for (;;) {
        int s = i, l = 2 * i + 1, r = l + 1;
        if (l < n && h[l] < h[s]) s = l;
        if (r < n && h[r] < h[s]) s = r;
        if (s == i) return;
        int t = h[i]; h[i] = h[s]; h[s] = t;
        i = s;
    }
}

void top_k(const int *data, int n, int k, int *out) {
    int heap[64];                                       /* a min-heap of the k largest seen so far */
    for (int i = 0; i < k; i++) heap[i] = data[i];
    for (int i = k / 2 - 1; i >= 0; i--) sift_down(heap, k, i);
    for (int i = k; i < n; i++)
        if (data[i] > heap[0]) { heap[0] = data[i]; sift_down(heap, k, 0); }   /* beat the smallest kept value */
    for (int m = k; m > 0; m--) {                       /* pop the minimum repeatedly: ascending order */
        out[k - m] = heap[0];
        heap[0] = heap[m - 1];
        sift_down(heap, m - 1, 0);
    }
}
