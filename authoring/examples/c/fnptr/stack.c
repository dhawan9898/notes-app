#include "stack.h"
#include <stdlib.h>
struct Stack { int *data; size_t len, cap; };
Stack *stack_new(size_t cap) {
    Stack *s = malloc(sizeof *s);
    if (!s) return NULL;
    s->data = cap ? malloc(cap * sizeof *s->data) : NULL;
    if (cap && !s->data) { free(s); return NULL; }
    s->len = 0; s->cap = cap;
    return s;
}
int stack_push(Stack *s, int v) {
    if (s->len == s->cap) {
        size_t ncap = s->cap ? s->cap * 2 : 4;
        int *p = realloc(s->data, ncap * sizeof *p);
        if (!p) return -1;
        s->data = p; s->cap = ncap;
    }
    s->data[s->len++] = v;
    return 0;
}
int stack_pop(Stack *s, int *out) { if (!s->len) return -1; *out = s->data[--s->len]; return 0; }
void stack_free(Stack *s) { if (s) { free(s->data); free(s); } }
