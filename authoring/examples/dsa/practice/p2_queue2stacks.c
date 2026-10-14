#include <stdio.h>

#define CAP 16

typedef struct { int a[CAP]; int n; } Stack;
static void spush(Stack *s, int v) { s->a[s->n++] = v; }
static int  spop(Stack *s)         { return s->a[--s->n]; }

/* A FIFO queue built from two LIFO stacks. Moving items between them reverses their order, which is exactly what is needed. */
typedef struct { Stack in, out; long moves; } Queue;

static void enqueue(Queue *q, int v) { spush(&q->in, v); }
static int dequeue(Queue *q) {
    if (q->out.n == 0)
        while (q->in.n > 0) { spush(&q->out, spop(&q->in)); q->moves++; }   /* refill only when the output stack is empty */
    return spop(&q->out);
}

int main(void) {
    Queue q = {0};
    for (int i = 1; i <= 4; i++) enqueue(&q, i * 10);
    int first = dequeue(&q);                        /* separate statements: argument evaluation order is unspecified */
    int second = dequeue(&q);
    printf("dequeue: %d %d\n", first, second);
    enqueue(&q, 50); enqueue(&q, 60);
    printf("then:   ");
    for (int i = 0; i < 4; i++) printf(" %d", dequeue(&q));
    printf("\nitems moved between stacks: %ld for 6 items (each item moves at most once)\n", q.moves);
    return 0;
}
