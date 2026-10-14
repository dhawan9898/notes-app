#include <stdio.h>
#include <stdlib.h>

/* Min-heap of ints in an array: parent of i is (i-1)/2, children 2i+1 and 2i+2. */
typedef struct { int *a; int n, cap; } Heap;

static void swap(int *x, int *y) { int t = *x; *x = *y; *y = t; }
static void push(Heap *h, int v) {
    int i = h->n++; h->a[i] = v;
    while (i > 0 && h->a[(i - 1) / 2] > h->a[i]) { swap(&h->a[(i - 1) / 2], &h->a[i]); i = (i - 1) / 2; }   /* sift up */
}
static int pop_min(Heap *h) {
    int top = h->a[0]; h->a[0] = h->a[--h->n];
    for (int i = 0;;) {                                                                                       /* sift down */
        int l = 2 * i + 1, r = l + 1, m = i;
        if (l < h->n && h->a[l] < h->a[m]) m = l;
        if (r < h->n && h->a[r] < h->a[m]) m = r;
        if (m == i) break;
        swap(&h->a[i], &h->a[m]); i = m;
    }
    return top;
}

int main(void) {
    int data[] = {42, 7, 99, 15, 3, 88, 61, 27, 54, 70, 12, 95};
    int n = (int)(sizeof data / sizeof data[0]), k = 4;

    /* Top-k largest with a min-heap of size k: O(n log k) time and O(k) space. */
    Heap h = {malloc((size_t)k * sizeof(int)), 0, k};
    for (int i = 0; i < n; i++) {
        if (h.n < k) push(&h, data[i]);
        else if (data[i] > h.a[0]) { pop_min(&h); push(&h, data[i]); }      /* beats the smallest of the current top k */
    }
    printf("the %d largest, smallest first:", k);
    while (h.n) printf(" %d", pop_min(&h));
    printf("\n");
    free(h.a);
    return 0;
}
