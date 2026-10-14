---
title: "Linked Lists, Stacks and Queues for Beginners"
track: dsa
module: DSA Essentials
moduleOrder: 0.5
order: 2
level: beginner
summary: "The three first data structures built from scratch in C: a singly linked list (insert, find, remove, reverse), a stack applied to bracket matching, and a fixed-size ring-buffer queue, with the trade-offs against arrays and a sanitizer-clean test of each."
prerequisites:
  - "Thinking in Data Structures"
  - "Structs, Enums and Typedef"
sources:
  - label: "Sedgewick & Wayne — Algorithms, 4th ed., §1.3 (Bags, Queues, and Stacks)"
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., §10.1 (Simple array-based implementations of stacks and queues) and §10.2 (Linked lists)"
  - label: "Kernighan & Ritchie — The C Programming Language, 2nd ed., §6.5 (Self-referential structures)"
  - label: "AddressSanitizer — Clang/GCC documentation"
    url: https://github.com/google/sanitizers/wiki/AddressSanitizer
---

A **data structure** is a way of arranging data in memory so that the operations you need are fast. You already know the simplest one, the array. This note adds three more, each a few dozen lines of C: the **linked list** (cheap insertion and removal), the **stack** (last in, first out) and the **queue** (first in, first out). They are the building blocks of almost every later structure in this track. The in-depth versions are *Dynamic Arrays*, *Linked Lists* and *Stacks, Queues, Deques and Ring Buffers*.

## The linked list: a chain of small structs

An array keeps its items side by side, so inserting at the front means shifting everything one slot along, and its size is fixed or must be reallocated. A **linked list** stores each item in its own small `struct` (a **node**) allocated with `malloc`, and each node holds the address of the next one. You only ever keep the address of the first node, the **head**. The last node's `next` is `NULL`. If struct pointers feel unfamiliar, read *Structs, Enums and Typedef* first.

```c
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
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o list_basic list_basic.c && ./list_basic; rm -f list_basic
built       50 -> 40 -> 30 -> 20 -> 10 -> NULL
find 30: found, find 99: missing
remove 30   50 -> 40 -> 20 -> 10 -> NULL
remove 50   40 -> 20 -> 10 -> NULL
reversed    10 -> 20 -> 40 -> NULL
```

(The program is compiled with AddressSanitizer, which would report any leak, double free or use-after-free; it printed nothing extra, so the list code is clean.)

Walk through the operations:

- **`push_front` is O(1).** Make a node, point it at the old head, return it as the new head. No other item moves.
- **`find` is O(n).** There is no index; you must walk from the head. A list cannot do binary search.
- **`remove_value`** unlinks a node by making the *previous* link skip over it, then frees it. The pointer-to-pointer `link` always holds "the address of the pointer that leads to the current node", which is why removing the head needs no special case. If that line looks hard, trace it on paper with three nodes.
- **`reverse`** flips every `next` pointer in one pass using three variables (`prev`, `head`, `next`), with no new memory.
- **`free_all`** must save `next` *before* freeing a node; reading `head->next` after `free(head)` is a use-after-free.

| | Array | Linked list |
|---|---|---|
| Access item *i* | O(1) | O(n) |
| Insert/remove at the front | O(n) (shift) | O(1) |
| Insert/remove after a node you already hold | O(n) | O(1) |
| Memory per item | just the value | value + pointer, plus allocator overhead |
| Cache behaviour | excellent (contiguous) | poor (nodes scattered) |

In practice arrays win more often than beginners expect, because modern CPUs read contiguous memory very fast. Reach for a list when you insert and remove in the middle a lot *and* already hold a pointer to the spot.

## The stack: last in, first out

A **stack** has two operations: `push` (put on top) and `pop` (take from the top). The last item pushed is the first popped, like a pile of plates. A stack can sit on an array with a `top` counter, which is the simplest possible data structure. Stacks appear whenever something must be undone in reverse order: the **function call stack**, the undo button, backtracking in a maze, and checking that brackets match.

```c
#include <stdio.h>
#include <string.h>

#define MAX 64

typedef struct { char data[MAX]; int top; } Stack;     /* top = number of items; the next free slot */

static void push(Stack *s, char c)  { s->data[s->top++] = c; }
static char pop(Stack *s)           { return s->data[--s->top]; }
static int  empty(const Stack *s)   { return s->top == 0; }

/* Are the brackets balanced? Last opened must be first closed: exactly what a stack gives you. */
static int balanced(const char *text) {
    Stack s = {.top = 0};
    for (const char *p = text; *p; p++) {
        if (strchr("([{", *p)) {
            if (s.top == MAX) return 0;                /* too deep for our fixed-size stack */
            push(&s, *p);
        } else if (strchr(")]}", *p)) {
            if (empty(&s)) return 0;                   /* a closer with nothing open */
            char open = pop(&s);
            if ((*p == ')' && open != '(') || (*p == ']' && open != '[') || (*p == '}' && open != '{')) return 0;
        }
    }
    return empty(&s);                                  /* anything still open is an error */
}

int main(void) {
    const char *tests[] = {"(a[b]{c})", "(]", "((())", "}{", ""};
    for (int i = 0; i < 5; i++) printf("%-12s -> %s\n", tests[i][0] ? tests[i] : "(empty)", balanced(tests[i]) ? "balanced" : "NOT balanced");
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o brackets brackets.c && ./brackets; rm -f brackets
(a[b]{c})    -> balanced
(]           -> NOT balanced
((())        -> NOT balanced
}{           -> NOT balanced
(empty)      -> balanced
```

Every opening bracket is pushed; every closing bracket must match the item on top, which is the most recently opened one, and that is exactly what "properly nested" means. Notice the three failure cases: a closer that does not match, a closer with nothing open, and openers left over at the end. Also notice the guard `if (s.top == MAX)`: a fixed-size stack must check for overflow before pushing, or it writes past the array.

## The queue: first in, first out

A **queue** serves items in arrival order: `enqueue` at the back, `dequeue` from the front, like a line at a ticket window. Printers, network packet buffers and breadth-first search all use queues. A naive array queue wastes space because the front keeps moving right; a **ring buffer** fixes that by wrapping the indexes around with `% CAP`.

```c
#include <stdio.h>

#define CAP 4

typedef struct { int data[CAP]; int head, count; } Queue;   /* a ring: indexes wrap around */

static int enqueue(Queue *q, int v) {
    if (q->count == CAP) return 0;                          /* full */
    q->data[(q->head + q->count) % CAP] = v;                 /* write after the last item, wrapping */
    q->count++;
    return 1;
}
static int dequeue(Queue *q, int *out) {
    if (q->count == 0) return 0;                             /* empty */
    *out = q->data[q->head];
    q->head = (q->head + 1) % CAP;
    q->count--;
    return 1;
}

int main(void) {
    Queue q = {.head = 0, .count = 0};
    for (int v = 1; v <= 5; v++) printf("enqueue %d: %s\n", v, enqueue(&q, v) ? "ok" : "full");
    int x;
    dequeue(&q, &x); printf("dequeued %d (first in, first out)\n", x);
    dequeue(&q, &x); printf("dequeued %d\n", x);
    enqueue(&q, 6); enqueue(&q, 7);                          /* reuses the freed slots by wrapping around */
    printf("remaining in order:");
    while (dequeue(&q, &x)) printf(" %d", x);
    printf("\n");
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o queue_ring queue_ring.c && ./queue_ring; rm -f queue_ring
enqueue 1: ok
enqueue 2: ok
enqueue 3: ok
enqueue 4: ok
enqueue 5: full
dequeued 1 (first in, first out)
dequeued 2
remaining in order: 3 4 6 7
```

The queue holds at most 4 items, so the fifth `enqueue` is refused (a real program must decide whether to wait, drop or grow). After two items are removed, the slots at the start are reused: the new items 6 and 7 are written *behind* the old ones by wrapping around, and the queue still delivers 3, 4, 6, 7 in order. `head` says where to read, and `(head + count) % CAP` says where to write.

## Which one do I need?

| Question | Use |
|---|---|
| "What was the most recent thing?" / undo / nested structure | stack |
| "What arrived first?" / fair processing order | queue |
| "Insert or delete at a known place without shifting" | linked list |
| "Fast access by position" | array |
| "Fast lookup by key" | hash table (next note) |

## Pitfalls

- **Using a node after `free`**, or freeing twice.
- **Forgetting to update the head** when the first node is added or removed, which loses the list.
- **Losing the rest of the list** by overwriting `next` before saving it.
- **No overflow or underflow checks** on fixed-size stacks and queues (`pop` on empty returns garbage).
- **Memory leaks** from never freeing nodes: build with `-fsanitize=address` to catch them.
- **Believing "linked lists are always faster for insertion"**: finding the position costs O(n) anyway.

## Key takeaways

1. A linked list trades O(1) insertion and removal at a known spot for O(n) lookup and scattered memory.
2. A stack is last-in-first-out (push/pop at one end); it is the natural fit for nested structure and undo.
3. A queue is first-in-first-out; a ring buffer reuses space by wrapping indexes with modulo.
4. Always guard against empty and full states, and free everything you allocate.
