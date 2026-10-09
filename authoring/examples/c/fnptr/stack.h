#ifndef STACK_H
#define STACK_H
#include <stddef.h>
typedef struct Stack Stack;                    /* opaque: callers cannot see or depend on the layout */
Stack *stack_new(size_t initial_capacity);     /* returns NULL on failure */
int    stack_push(Stack *s, int value);        /* 0 on success, -1 on allocation failure */
int    stack_pop(Stack *s, int *out);          /* 0 on success, -1 if empty */
void   stack_free(Stack *s);                   /* accepts NULL */
#endif
