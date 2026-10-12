#include <stdio.h>
#include <stdlib.h>
#include "vec.h"
#include "../../c/perf/timing.h"

struct node { int value; struct node *next; };

/* Insert at the FRONT of a sequence repeatedly: O(n) for an array, O(1) for a list. */
int main(void) {
    printf("%8s %14s %14s %14s\n", "n", "vec front ins", "list front ins", "vec push/pop");
    for (int n = 20000; n <= 160000; n *= 2) {
        Vec v; vec_init(&v, sizeof(int));
        double t0 = now(); for (int i = 0; i < n; i++) vec_insert(&v, 0, &i); double t1 = now();
        struct node *head = NULL;
        double t2 = now(); for (int i = 0; i < n; i++) { struct node *x = malloc(sizeof *x); x->value = i; x->next = head; head = x; } double t3 = now();
        Vec w; vec_init(&w, sizeof(int));
        double t4 = now(); for (int i = 0; i < n; i++) vec_push(&w, &i); double t5 = now();
        printf("%8d %11.2f ms %11.2f ms %11.2f ms\n", n, (t1 - t0) * 1e3, (t3 - t2) * 1e3, (t5 - t4) * 1e3);
        for (struct node *p = head, *q; p; p = q) { q = p->next; free(p); }
        vec_free(&v); vec_free(&w);
    }
    return 0;
}
