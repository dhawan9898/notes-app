#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Node { int value; struct Node *next; } Node;

static Node *push_front(Node *head, int v) { Node *n = malloc(sizeof *n); n->value = v; n->next = head; return n; }

static void free_list(Node *h) { while (h) { Node *n = h->next; free(h); h = n; } }

/* Reverse in place: three pointers, O(n) time, O(1) space.  Invariant: `prev` is the reversed prefix, `cur` the untouched rest. */
static Node *reverse(Node *head) {
    Node *prev = NULL, *cur = head;
    while (cur) { Node *next = cur->next; cur->next = prev; prev = cur; cur = next; }
    return prev;
}

/* Middle node with slow/fast pointers: when fast reaches the end, slow is at the middle. */
static Node *middle(Node *head) {
    Node *slow = head, *fast = head;
    while (fast && fast->next) { slow = slow->next; fast = fast->next->next; }
    return slow;
}

/* Floyd's cycle detection: returns the length of the tail+cycle information via out parameters, or 0 if no cycle. */
static int has_cycle(Node *head, int *cycle_len, int *steps) {
    Node *slow = head, *fast = head; int s = 0;
    while (fast && fast->next) {
        slow = slow->next; fast = fast->next->next; s++;
        if (slow == fast) {                                  /* they met inside the cycle: measure its length */
            int len = 1; for (Node *p = slow->next; p != slow; p = p->next) len++;
            *cycle_len = len; *steps = s; return 1;
        }
    }
    *steps = s; return 0;
}

/* Where does the cycle start?  Reset one pointer to the head; advance both one step at a time: they meet at the entry. */
static Node *cycle_entry(Node *head) {
    Node *slow = head, *fast = head;
    while (fast && fast->next) { slow = slow->next; fast = fast->next->next; if (slow == fast) break; }
    if (!fast || !fast->next) return NULL;
    slow = head;
    while (slow != fast) { slow = slow->next; fast = fast->next; }
    return slow;
}

/* Merge two sorted lists (stable) using a dummy head to remove the empty-list special case. */
static Node *merge(Node *a, Node *b) {
    Node dummy = {0, NULL}, *tail = &dummy;
    while (a && b) {
        if (b->value < a->value) { tail->next = b; b = b->next; } else { tail->next = a; a = a->next; }
        tail = tail->next;
    }
    tail->next = a ? a : b;
    return dummy.next;
}

/* Merge sort for lists: O(n log n) time, O(log n) stack, no extra node allocation, and stable. */
static Node *sort_list(Node *head) {
    if (!head || !head->next) return head;
    Node *mid = middle(head), *prev = head;
    while (prev->next != mid) prev = prev->next;
    prev->next = NULL;                                       /* cut the list into two halves */
    return merge(sort_list(head), sort_list(mid));
}

static int is_sorted(const Node *h) { for (; h && h->next; h = h->next) if (h->value > h->next->value) return 0; return 1; }
static int length(const Node *h) { int n = 0; for (; h; h = h->next) n++; return n; }

int main(void) {
    /* reverse */
    Node *l = NULL; for (int i = 5; i >= 1; i--) l = push_front(l, i);
    printf("list:     "); for (Node *p = l; p; p = p->next) printf("%d ", p->value);
    l = reverse(l);
    printf("\nreversed: "); for (Node *p = l; p; p = p->next) printf("%d ", p->value);
    printf("\nmiddle of 5 nodes = %d\n", middle(l)->value);
    free_list(l);

    /* cycle detection: build a tail of 7 nodes leading into a cycle of 5 nodes */
    Node *nodes[12];
    for (int i = 0; i < 12; i++) { nodes[i] = malloc(sizeof(Node)); nodes[i]->value = i; nodes[i]->next = i + 1 < 12 ? NULL : NULL; }
    for (int i = 0; i < 11; i++) nodes[i]->next = nodes[i + 1];
    nodes[11]->next = nodes[7];                                   /* cycle entry = node 7, cycle length = 5 */
    int clen = 0, steps = 0;
    int c = has_cycle(nodes[0], &clen, &steps);
    printf("cycle detected: %d after %d slow-pointer steps, cycle length %d, entry node value %d\n", c, steps, clen, cycle_entry(nodes[0])->value);
    nodes[11]->next = NULL;
    printf("after breaking the cycle: has_cycle=%d\n", has_cycle(nodes[0], &clen, &steps));
    for (int i = 0; i < 12; i++) free(nodes[i]);

    /* random testing of sort_list against invariants */
    srand(5);
    int ok = 1;
    for (int t = 0; t < 2000 && ok; t++) {
        int n = rand() % 60; Node *h = NULL;
        for (int i = 0; i < n; i++) h = push_front(h, rand() % 30);
        int before = length(h);
        h = sort_list(h);
        ok = is_sorted(h) && length(h) == before;
        free_list(h);
    }
    printf("sort_list: 2000 random lists sorted with length preserved: %s\n", ok ? "yes" : "NO");
    return 0;
}
