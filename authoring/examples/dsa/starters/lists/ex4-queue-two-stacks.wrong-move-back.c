void enqueue(Queue *q, int v) {
    spush(&q->in, v);
}

int dequeue(Queue *q) {
    while (q->in.n > 0) spush(&q->out, spop(&q->in));
    int v = spop(&q->out);
    while (q->out.n > 0) spush(&q->in, spop(&q->out));     /* moves everything back every time: O(n) per dequeue */
    return v;
}
