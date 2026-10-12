#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lockfree.h"
#include "../../c/perf/timing.h"

enum { MAXT = 16 };
static MSQueue msq; static LockQueue lq; static TStack stack; static LNode *spool;
static int PER, NP, NC; static _Atomic long consumed_total;
static uint8_t *seen[MAXT]; static int seq_violations[MAXT];
#define MAKE(p, s) (((uint64_t)(p) << 32) | (uint32_t)(s))

typedef struct { int id, kind; } Arg;                  /* kind 0 = MS queue, 1 = mutex queue */
static void *producer(void *a) {
    Arg *g = a;
    for (int i = 0; i < PER; i++) {
        if (g->kind == 0) { while (!msq_enqueue(&msq, MAKE(g->id, i))) sched_yield(); } else lq_enqueue(&lq, MAKE(g->id, i));
    }
    return NULL;
}
static void *consumer(void *a) {
    Arg *g = a; int last[MAXT]; for (int p = 0; p < MAXT; p++) last[p] = -1; long bad = 0;
    while (atomic_load(&consumed_total) < (long)NP * PER) {
        uint64_t v; int ok = g->kind == 0 ? msq_dequeue(&msq, &v) : lq_dequeue(&lq, &v);
        if (!ok) continue;
        int p = (int)(v >> 32), s = (int)(uint32_t)v; if (s <= last[p]) bad++;          /* FIFO: per-producer sequence must be increasing for each consumer */
        last[p] = s; seen[p][s]++; atomic_fetch_add(&consumed_total, 1);
    }
    seq_violations[g->id] = (int)bad; return NULL;
}
static double run_queue(int kind, int np, int nc, int per) {
    NP = np; NC = nc; PER = per; atomic_store(&consumed_total, 0);
    for (int p = 0; p < np; p++) { seen[p] = calloc((size_t)per, 1); }
    if (kind == 0) msq_init(&msq, 4096); else lq_init(&lq);
    pthread_t th[2 * MAXT]; Arg args[2 * MAXT]; double t0 = now();
    for (int c = 0; c < nc; c++) { args[c] = (Arg){ c, kind }; pthread_create(&th[c], NULL, consumer, &args[c]); }
    for (int p = 0; p < np; p++) { args[MAXT + p] = (Arg){ p, kind }; pthread_create(&th[MAXT + p], NULL, producer, &args[MAXT + p]); }
    for (int p = 0; p < np; p++) pthread_join(th[MAXT + p], NULL);
    for (int c = 0; c < nc; c++) pthread_join(th[c], NULL);
    double dt = now() - t0;
    for (int p = 0; p < np; p++) { for (int i = 0; i < per; i++) assert(seen[p][i] == 1); free(seen[p]); }          /* every item delivered exactly once */
    for (int c = 0; c < nc; c++) assert(seq_violations[c] == 0);
    if (kind == 0) msq_destroy(&msq);
    return dt;
}

/* Treiber stack conservation test: threads push their own ids and pop concurrently; nothing may be lost or duplicated */
static void *stack_worker(void *a) {
    long id = (long)a; int mine[64]; int n = 0;
    for (int i = 0; i < 200000; i++) {
        if (n < 64 && (i & 1) == 0) { uint32_t x = tstack_pop_idx(&stack); if (x != NIL) mine[n++] = (int)x; }
        else if (n > 0) { tstack_push_idx(&stack, (uint32_t)mine[--n]); }
    }
    while (n > 0) tstack_push_idx(&stack, (uint32_t)mine[--n]);
    (void)id; return NULL;
}

int main(void) {
    int caps = 1024; spool = calloc((size_t)caps, sizeof(LNode)); stack.pool = spool; atomic_init(&stack.head, mk(0, NIL));
    for (uint32_t i = 0; i < (uint32_t)caps; i++) tstack_push_idx(&stack, i);
    pthread_t th[8]; for (long t = 0; t < 8; t++) pthread_create(&th[t], NULL, stack_worker, (void *)t); for (int t = 0; t < 8; t++) pthread_join(th[t], NULL);
    char *present = calloc((size_t)caps, 1); int count = 0; for (uint32_t x; (x = tstack_pop_idx(&stack)) != NIL; ) { assert(!present[x]); present[x] = 1; count++; }
    assert(count == caps);
    printf("Treiber stack (tagged indices): 8 threads x 200,000 push/pop operations over %d nodes; afterwards every node was present exactly once\n\n", caps);
    free(present); free(spool);

    printf("queue stress: each producer enqueues sequence numbers; every item must be dequeued exactly once and, per producer, in FIFO order\n");
    printf("%-26s %12s %12s\n", "queue (producers, consumers)", "ms", "M items/s");
    int cfg[][2] = { { 1, 1 }, { 2, 2 }, { 4, 4 } }; int per = 250000;
    for (int k = 0; k < 2; k++) for (int c = 0; c < 3; c++) {
        double dt = run_queue(k, cfg[c][0], cfg[c][1], per);
        printf("%-18s (%d, %d) %12.0f %12.2f\n", k ? "mutex linked queue" : "Michael-Scott lock-free", cfg[c][0], cfg[c][1], dt * 1e3, (double)per * cfg[c][0] / dt / 1e6);
    }
    return 0;
}
