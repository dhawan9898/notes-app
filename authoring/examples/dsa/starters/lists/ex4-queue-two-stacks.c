// Exercise 4: a FIFO queue built from two LIFO stacks, O(1) amortised per operation.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAP 64
typedef struct { int a[CAP]; int n; } Stack;
static long pushes;     /* the tests count every push, to check that items are not shuffled back and forth */
void spush(Stack *s, int v) { pushes++; s->a[s->n++] = v; }
int spop(Stack *s) { return s->a[--s->n]; }
typedef struct { Stack in, out; } Queue;

/* ===== Your code: edit below this line ===== */
/* Use only spush and spop on q->in and q->out. */
void enqueue(Queue *q, int v) {
    (void)q; (void)v;
}

int dequeue(Queue *q) {
    (void)q;
    return -1;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static void print_arr(const int *v, int n) {
    printf("{");
    for (int i = 0; i < n; i++) printf(i ? ", %d" : "%d", v[i]);
    printf("}");
}

static void check_arr(const char *what, const int *got, int got_n, const int *want, int want_n) {
    int same = got_n == want_n;
    for (int i = 0; same && i < want_n; i++) same = got[i] == want[i];
    checks++;
    if (!same) failed++;
    printf("%s %s", same ? "PASS" : "FAIL", what);
    if (same) { printf(" = "); print_arr(got, got_n); }
    else { printf(": expected "); print_arr(want, want_n); printf(", got "); print_arr(got, got_n); }
    printf("\n");
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    Queue q = {0};
    for (int i = 1; i <= 4; i++) enqueue(&q, i * 10);
    int got[8], k = 0;
    got[k++] = dequeue(&q);
    got[k++] = dequeue(&q);
    enqueue(&q, 50);
    enqueue(&q, 60);
    for (int i = 0; i < 4; i++) got[k++] = dequeue(&q);
    const int want[] = {10, 20, 30, 40, 50, 60};
    check_arr("dequeue order", got, k, want, 6);
    pushes = 0;
    Queue r = {0};
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < 5; i++) enqueue(&r, i);
        for (int i = 0; i < 5; i++) (void)dequeue(&r);
    }
    printf("     100 operations on 50 items made %ld pushes\n", pushes);
    check_int("each item pushed at most twice (once per stack)", pushes <= 100, 1);
    return summary();
}
