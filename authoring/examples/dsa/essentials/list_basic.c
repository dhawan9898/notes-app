#include <stdio.h>
#include <stdlib.h>

typedef struct node { int value; struct node *next; } Node;

static Node *push_front(Node *head, int v) {             /* O(1): no shifting, unlike an array */
    Node *n = malloc(sizeof *n);
    if (!n) exit(1);
    n->value = v; n->next = head;
    return n;                                             /* the new head */
}
static Node *find(Node *head, int v) {                    /* O(n): must walk from the front */
    for (Node *p = head; p; p = p->next) if (p->value == v) return p;
    return NULL;
}
static Node *remove_value(Node *head, int v) {
    Node **link = &head;                                  /* pointer to the pointer that leads to the current node */
    while (*link && (*link)->value != v) link = &(*link)->next;
    if (*link) { Node *dead = *link; *link = dead->next; free(dead); }
    return head;
}
static Node *reverse(Node *head) {
    Node *prev = NULL;
    while (head) { Node *next = head->next; head->next = prev; prev = head; head = next; }
    return prev;
}
static void print(const char *label, const Node *head) {
    printf("%-11s", label);
    for (const Node *p = head; p; p = p->next) printf(" %d ->", p->value);
    printf(" NULL\n");
}
static void free_all(Node *head) { while (head) { Node *n = head->next; free(head); head = n; } }

int main(void) {
    Node *list = NULL;
    for (int v = 1; v <= 5; v++) list = push_front(list, v * 10);
    print("built", list);
    printf("find 30: %s, find 99: %s\n", find(list, 30) ? "found" : "missing", find(list, 99) ? "found" : "missing");
    list = remove_value(list, 30);  print("remove 30", list);
    list = remove_value(list, 50);  print("remove 50", list);   /* removing the head works too */
    list = reverse(list);           print("reversed", list);
    free_all(list);
    return 0;
}
