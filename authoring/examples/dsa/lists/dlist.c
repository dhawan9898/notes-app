#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Circular doubly linked list with a SENTINEL head: no NULL checks, no special cases for empty/first/last. */
typedef struct DNode { int value; struct DNode *prev, *next; } DNode;
typedef struct { DNode head; size_t len; } DList;

static void dl_init(DList *l) { l->head.prev = l->head.next = &l->head; l->len = 0; }

static DNode *dl_insert_after(DList *l, DNode *pos, int v) {          /* O(1) given the position */
    DNode *n = malloc(sizeof *n);
    n->value = v; n->prev = pos; n->next = pos->next;
    pos->next->prev = n; pos->next = n;
    l->len++;
    return n;
}
static DNode *dl_push_back(DList *l, int v)  { return dl_insert_after(l, l->head.prev, v); }
static DNode *dl_push_front(DList *l, int v) { return dl_insert_after(l, &l->head, v); }
static void dl_remove(DList *l, DNode *n) {                           /* O(1) given the node itself */
    assert(n != &l->head);
    n->prev->next = n->next; n->next->prev = n->prev;
    free(n); l->len--;
}
static void dl_clear(DList *l) { while (l->head.next != &l->head) dl_remove(l, l->head.next); }

static void dl_print(const DList *l, const char *label) {
    printf("%-26s", label);
    for (const DNode *p = l->head.next; p != &l->head; p = p->next) printf("%d ", p->value);
    printf(" (len %zu, backward:", l->len);
    for (const DNode *p = l->head.prev; p != &l->head; p = p->prev) printf(" %d", p->value);
    puts(")");
}

int main(void) {
    DList l; dl_init(&l);
    for (int i = 1; i <= 4; i++) dl_push_back(&l, i * 10);
    dl_push_front(&l, 5);
    dl_print(&l, "after pushes:");
    DNode *thirty = l.head.next->next->next->next;                    /* walk to the node holding 30 */
    dl_insert_after(&l, thirty, 35);
    dl_print(&l, "insert 35 after 30:");
    dl_remove(&l, thirty);
    dl_print(&l, "remove node 30 (O(1)):");
    dl_remove(&l, l.head.next);                                       /* remove first */
    dl_remove(&l, l.head.prev);                                       /* remove last  */
    dl_print(&l, "remove first and last:");
    dl_clear(&l);
    dl_print(&l, "after clear:");
    return 0;
}
