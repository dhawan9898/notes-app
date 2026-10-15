// A pointer into a growable array goes stale when realloc moves the block.
#include <stdio.h>
#include <stdlib.h>

struct vec { int *data; size_t len, cap; };

static int push(struct vec *v, int x) {
    if (v->len == v->cap) {
        size_t cap = v->cap ? 2 * v->cap : 4;
        int *d = realloc(v->data, cap * sizeof *d);   // may return a different address; the old block is freed
        if (!d) return -1;
        v->data = d;
        v->cap = cap;
    }
    v->data[v->len++] = x;
    return 0;
}

int main(int argc, char **argv) {
    struct vec v = {0};
    push(&v, 10);
    int *first = &v.data[0];      // pointer to an element: valid only until the next reallocation
    size_t index = 0;             // an index stays valid across reallocations
    for (int i = 1; i < 100; i++) push(&v, 10 + i);
    printf("by index:   v.data[%zu] = %d\n", index, v.data[index]);
    if (argc > 1) printf("by pointer: *first = %d\n", *first);   // use after free if the block moved
    free(v.data);
    return 0;
}
