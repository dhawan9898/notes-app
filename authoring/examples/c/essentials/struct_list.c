#include <stdio.h>
#include <stdlib.h>

typedef struct node {                 /* a struct may point to another struct of its own kind */
    int value;
    struct node *next;                /* "struct node" is needed here: the typedef name is not ready yet */
} Node;

int main(void) {
    Node *head = NULL;
    for (int v = 3; v >= 1; v--) {    /* build 1 -> 2 -> 3 by inserting at the front */
        Node *n = malloc(sizeof *n);
        if (!n) return 1;
        n->value = v;
        n->next = head;
        head = n;
    }
    for (Node *p = head; p != NULL; p = p->next) printf("%d ", p->value);
    printf("\n");
    while (head) { Node *next = head->next; free(head); head = next; }   /* free every node */
    return 0;
}
