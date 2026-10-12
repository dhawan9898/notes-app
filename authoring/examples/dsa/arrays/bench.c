#include <stdio.h>
#include <stdlib.h>
#include "vec.h"
#include "../../c/perf/timing.h"

struct node { int value; struct node *next; };

int main(void) {
    enum { N = 2000000 };
    double t0, t1;
    long sum = 0;

    t0 = now(); Vec v; vec_init(&v, sizeof(int)); for (int i = 0; i < N; i++) VEC_PUSH(int, &v, i); t1 = now();
    printf("append %d ints:     vector %7.1f ms   ", N, (t1 - t0) * 1e3);
    t0 = now(); struct node *head = NULL, *tail = NULL;
    for (int i = 0; i < N; i++) { struct node *n = malloc(sizeof *n); n->value = i; n->next = NULL; if (tail) tail->next = n; else head = n; tail = n; }
    t1 = now(); printf("linked list %7.1f ms\n", (t1 - t0) * 1e3);

    t0 = now(); for (int rep = 0; rep < 10; rep++) for (size_t i = 0; i < v.len; i++) sum += ((int *)v.data)[i]; t1 = now();
    double t_vec = t1 - t0;
    printf("traverse x10:          vector %7.1f ms   ", t_vec * 1e3);
    t0 = now(); for (int rep = 0; rep < 10; rep++) for (struct node *p = head; p; p = p->next) sum += p->value; t1 = now();
    printf("linked list %7.1f ms   (nodes allocated in order: best-case locality)\n", (t1 - t0) * 1e3);

    /* relink the nodes in random memory order: the realistic state of a list that has seen inserts and deletes */
    struct node **ptrs = malloc((size_t)N * sizeof *ptrs); size_t k = 0;
    for (struct node *p = head; p; p = p->next) ptrs[k++] = p;
    for (size_t i = k - 1; i > 0; i--) { size_t j = (size_t)rand() % (i + 1); struct node *t = ptrs[i]; ptrs[i] = ptrs[j]; ptrs[j] = t; }
    for (size_t i = 0; i + 1 < k; i++) ptrs[i]->next = ptrs[i + 1];
    ptrs[k - 1]->next = NULL; head = ptrs[0];
    t0 = now(); for (int rep = 0; rep < 3; rep++) for (struct node *p = head; p; p = p->next) sum += p->value; t1 = now();
    printf("traverse x3,  list nodes scattered in memory: %7.1f ms   (%.0f ns/element vs %.1f ns/element for the vector)\n", (t1 - t0) * 1e3, (t1 - t0) / (3.0 * N) * 1e9, t_vec / (10.0 * N) * 1e9);

    DO_NOT_OPTIMIZE(sum);
    printf("memory: vector %.0f MB, list %.0f MB (a 16-byte node occupies a 32-byte malloc chunk)\n", N * 4 / 1e6, N * 32.0 / 1e6);
    free(ptrs);
    for (struct node *p = head, *n; p; p = n) { n = p->next; free(p); }
    vec_free(&v);
    return 0;
}
