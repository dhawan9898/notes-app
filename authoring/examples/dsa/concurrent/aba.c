#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

/* Deterministic replay of the ABA problem on a Treiber stack (lock-free LIFO, pointer version).  We run the "threads" by hand,
 * in the exact interleaving that breaks it, so the result is reproducible rather than a once-a-week crash. */
typedef struct Node { const char *name; struct Node *next; } Node;
static _Atomic(Node *) head;

static void show(const char *label) { printf("%-44s stack:", label); for (Node *n = atomic_load(&head); n; n = n->next) printf(" %s", n->name); printf("\n"); }

int main(void) {
    Node C = { "C", NULL }, B = { "B", &C }, A = { "A", &B };
    atomic_store(&head, &A);
    show("initial");

    /* Thread 1 starts pop(): reads head and head->next, then is descheduled before its compare-and-swap */
    Node *old = atomic_load(&head), *next = old->next;                 /* old = A, next = B */
    printf("thread 1: read head = %s, next = %s, then it is preempted...\n", old->name, next->name);

    /* Thread 2 runs: pop A, pop B (B is now owned by someone else and may be freed or reused), push A back */
    Node *a = atomic_load(&head); atomic_store(&head, a->next);        /* pop A */
    Node *b = atomic_load(&head); atomic_store(&head, b->next);        /* pop B -> B is "freed" */
    a->next = atomic_load(&head); atomic_store(&head, a);              /* push A again; A->next = C */
    show("thread 2: popped A, popped B, pushed A");
    B.next = (Node *)0xDEAD;                                           /* B was recycled: its next now holds garbage */
    printf("thread 2: B has been freed and reused (B.next now garbage)\n");

    /* Thread 1 resumes: CAS(head, A, B) SUCCEEDS because head is again A -- the value A is back, but the world has changed */
    Node *expected = old;
    if (atomic_compare_exchange_strong(&head, &expected, next)) printf("thread 1: CAS(head, A -> B) succeeded!  head is now B, a node that is no longer in the stack, whose next is garbage\n");
    printf("head = %s, head->next = %p  (C was lost; the stack is corrupt)\n\n", atomic_load(&head)->name, (void *)atomic_load(&head)->next);
    return 0;
}
