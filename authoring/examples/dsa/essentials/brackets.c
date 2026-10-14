#include <stdio.h>
#include <string.h>

#define MAX 64

typedef struct { char data[MAX]; int top; } Stack;     /* top = number of items; the next free slot */

static void push(Stack *s, char c)  { s->data[s->top++] = c; }
static char pop(Stack *s)           { return s->data[--s->top]; }
static int  empty(const Stack *s)   { return s->top == 0; }

/* Are the brackets balanced? Last opened must be first closed: exactly what a stack gives you. */
static int balanced(const char *text) {
    Stack s = {.top = 0};
    for (const char *p = text; *p; p++) {
        if (strchr("([{", *p)) {
            if (s.top == MAX) return 0;                /* too deep for our fixed-size stack */
            push(&s, *p);
        } else if (strchr(")]}", *p)) {
            if (empty(&s)) return 0;                   /* a closer with nothing open */
            char open = pop(&s);
            if ((*p == ')' && open != '(') || (*p == ']' && open != '[') || (*p == '}' && open != '{')) return 0;
        }
    }
    return empty(&s);                                  /* anything still open is an error */
}

int main(void) {
    const char *tests[] = {"(a[b]{c})", "(]", "((())", "}{", ""};
    for (int i = 0; i < 5; i++) printf("%-12s -> %s\n", tests[i][0] ? tests[i] : "(empty)", balanced(tests[i]) ? "balanced" : "NOT balanced");
    return 0;
}
