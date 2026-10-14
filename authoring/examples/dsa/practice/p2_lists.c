#include <stdio.h>
#include <stdlib.h>

typedef struct node { int v; struct node *next; } Node;

static Node *push(Node *head, int v) { Node *n = malloc(sizeof *n); if (!n) exit(1); n->v = v; n->next = head; return n; }
static void print(const char *label, const Node *p) { printf("%-8s", label); for (; p; p = p->next) printf(" %d", p->v); printf("\n"); }
static void free_all(Node *p) { while (p) { Node *n = p->next; free(p); p = n; } }

/* Floyd's tortoise and hare: O(n) time, O(1) space cycle detection. */
static int has_cycle(const Node *head) {
    const Node *slow = head, *fast = head;
    while (fast && fast->next) {
        slow = slow->next; fast = fast->next->next;
        if (slow == fast) return 1;
    }
    return 0;
}

/* Merge two sorted lists by relinking nodes (no allocation). A dummy head removes the "empty result" special case. */
static Node *merge(Node *a, Node *b) {
    Node dummy = {0, NULL}, *tail = &dummy;
    while (a && b) {
        if (a->v <= b->v) { tail->next = a; a = a->next; }       /* <= keeps the merge stable */
        else              { tail->next = b; b = b->next; }
        tail = tail->next;
    }
    tail->next = a ? a : b;
    return dummy.next;
}

int main(void) {
    Node *a = NULL, *b = NULL;
    for (int v = 9; v >= 1; v -= 4) a = push(a, v);               /* 1 5 9 */
    for (int v = 8; v >= 2; v -= 3) b = push(b, v);               /* 2 5 8 */
    print("a", a); print("b", b);
    Node *m = merge(a, b);
    print("merged", m);

    printf("cycle in merged list? %d\n", has_cycle(m));
    Node *last = m; while (last->next) last = last->next;
    last->next = m->next->next;                                   /* tie the tail back into the middle */
    printf("after linking tail to the 3rd node: %d\n", has_cycle(m));
    last->next = NULL;                                            /* undo before freeing */
    free_all(m);
    return 0;
}
