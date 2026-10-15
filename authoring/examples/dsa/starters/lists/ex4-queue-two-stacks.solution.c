void enqueue(Queue *q, int v) {
    spush(&q->in, v);
}

int dequeue(Queue *q) {
    if (q->out.n == 0)                              /* refill only when the output stack is empty */
        while (q->in.n > 0) spush(&q->out, spop(&q->in));
    return spop(&q->out);
}
