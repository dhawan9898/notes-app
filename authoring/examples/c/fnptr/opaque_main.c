#include "stack.h"
#include <stdio.h>
int main(void) {
    Stack *s = stack_new(2);
    for (int i = 1; i <= 5; i++) stack_push(s, i * i);
    int v; printf("popped:"); while (stack_pop(s, &v) == 0) printf(" %d", v); puts("");
    stack_free(s);
    return 0;
}
