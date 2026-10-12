#ifndef INDEXED_H
#define INDEXED_H
#include <stdlib.h>

/* Indexed min-priority queue over ids 0..cap-1 with decrease-key in O(log n).  pos[id] = heap slot or -1.
 * This is the structure Dijkstra and Prim want: "lower the priority of vertex v" without searching for it. */
typedef struct { int *heap; int *pos; long *prio; int n; } IPQ;

static void ipq_init(IPQ *q, int cap) {
    q->heap = malloc((size_t)cap * sizeof(int)); q->pos = malloc((size_t)cap * sizeof(int)); q->prio = malloc((size_t)cap * sizeof(long)); q->n = 0;
    for (int i = 0; i < cap; i++) q->pos[i] = -1;
}
static void ipq_free(IPQ *q) { free(q->heap); free(q->pos); free(q->prio); }
static inline void ipq_swap(IPQ *q, int i, int j) { int a = q->heap[i], b = q->heap[j]; q->heap[i] = b; q->heap[j] = a; q->pos[b] = i; q->pos[a] = j; }
static void ipq_up(IPQ *q, int i) { while (i > 0 && q->prio[q->heap[(i - 1) / 2]] > q->prio[q->heap[i]]) { ipq_swap(q, i, (i - 1) / 2); i = (i - 1) / 2; } }
static void ipq_down(IPQ *q, int i) {
    for (;;) {
        int l = 2 * i + 1, r = l + 1, m = i;
        if (l < q->n && q->prio[q->heap[l]] < q->prio[q->heap[m]]) m = l;
        if (r < q->n && q->prio[q->heap[r]] < q->prio[q->heap[m]]) m = r;
        if (m == i) return;
        ipq_swap(q, i, m); i = m;
    }
}
static inline int ipq_contains(const IPQ *q, int id) { return q->pos[id] >= 0; }
static void ipq_insert(IPQ *q, int id, long p) { q->prio[id] = p; q->heap[q->n] = id; q->pos[id] = q->n; q->n++; ipq_up(q, q->n - 1); }
static void ipq_decrease(IPQ *q, int id, long p) { q->prio[id] = p; ipq_up(q, q->pos[id]); }          /* requires p <= current priority */
static int ipq_extract(IPQ *q) {
    int top = q->heap[0]; ipq_swap(q, 0, q->n - 1); q->pos[top] = -1; q->n--;
    if (q->n) ipq_down(q, 0);
    return top;
}
#endif
