#include <stdio.h>

#define CAP 4

typedef struct { int data[CAP]; int head, count; } Queue;   /* a ring: indexes wrap around */

static int enqueue(Queue *q, int v) {
    if (q->count == CAP) return 0;                          /* full */
    q->data[(q->head + q->count) % CAP] = v;                 /* write after the last item, wrapping */
    q->count++;
    return 1;
}
static int dequeue(Queue *q, int *out) {
    if (q->count == 0) return 0;                             /* empty */
    *out = q->data[q->head];
    q->head = (q->head + 1) % CAP;
    q->count--;
    return 1;
}

int main(void) {
    Queue q = {.head = 0, .count = 0};
    for (int v = 1; v <= 5; v++) printf("enqueue %d: %s\n", v, enqueue(&q, v) ? "ok" : "full");
    int x;
    dequeue(&q, &x); printf("dequeued %d (first in, first out)\n", x);
    dequeue(&q, &x); printf("dequeued %d\n", x);
    enqueue(&q, 6); enqueue(&q, 7);                          /* reuses the freed slots by wrapping around */
    printf("remaining in order:");
    while (dequeue(&q, &x)) printf(" %d", x);
    printf("\n");
    return 0;
}
