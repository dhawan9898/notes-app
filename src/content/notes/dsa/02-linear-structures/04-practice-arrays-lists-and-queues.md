---
title: "Practice: Arrays, Lists and Queues"
track: dsa
module: Linear Structures
moduleOrder: 2
order: 4
level: intermediate
summary: "Four exercises on the linear-structures module: measuring why doubling beats fixed-size growth, detecting a cycle in O(1) space, merging two sorted lists without allocating, and building a queue from two stacks with amortised O(1) operations, each with a verified C answer."
prerequisites:
  - "Dynamic Arrays"
  - "Linked Lists"
  - "Stacks, Queues, Deques and Ring Buffers"
sources:
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., §16.1 (Aggregate analysis) and Ch. 10"
  - label: "Sedgewick & Wayne — Algorithms, 4th ed., §1.3 (exercises on stacks, queues and linked lists)"
  - label: "Floyd, R. W. — Nondeterministic Algorithms (JACM, 1967), cycle detection"
---

These exercises turn the module's ideas into numbers and pointer code. Write your solution first, then compare, and run everything under `-fsanitize=address,undefined`.

## Exercises

**1. Why doubling?** A dynamic array starts with capacity 1. Count how many element **copies** `N` pushes cost if it grows (a) by doubling and (b) by a fixed +10. Predict the ratio for `N = 1,000,000` before running anything, then state the amortised cost per push for each strategy.

**2. Cycle detection.** Given the head of a singly linked list, decide whether it contains a cycle using O(1) extra memory. (Hint: two pointers moving at different speeds.) Explain why they must meet if a cycle exists.

**3. Merge sorted lists.** Merge two sorted singly linked lists into one sorted list **without allocating new nodes**. Keep the merge stable (equal keys from the first list stay first). Hint: a dummy head node removes the special case for the first element.

**4. Queue from two stacks.** Implement a FIFO queue using only two LIFO stacks. Show that every item is moved between the stacks at most once, so `enqueue` and `dequeue` are amortised O(1).

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

```c
#include <stdio.h>

/* Count how many element copies N pushes cost when the array grows by doubling versus by a fixed +10. */
static long copies_for(long pushes, int doubling) {
    long cap = 1, size = 0, copies = 0;
    for (long i = 0; i < pushes; i++) {
        if (size == cap) {                        /* full: allocate a bigger array and copy everything over */
            copies += size;
            cap = doubling ? cap * 2 : cap + 10;
        }
        size++;
    }
    return copies;
}

int main(void) {
    printf("%10s %16s %16s\n", "pushes", "copies (double)", "copies (+10)");
    for (long n = 1000; n <= 1000000; n *= 10)
        printf("%10ld %16ld %16ld\n", n, copies_for(n, 1), copies_for(n, 0));
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o p2_growth p2_growth.c && ./p2_growth; rm -f p2_growth
    pushes  copies (double)     copies (+10)
      1000             1023            49600
     10000            16383          4996000
    100000           131071        499960000
   1000000          1048575      49999600000
```

Doubling copies about **N** elements in total (1,048,575 for a million pushes: 1 + 2 + 4 + ... + 2^19), so the amortised cost is about 1 copy per push: **O(1)**. Growing by a constant copies about N²/20 elements in total (50 billion for a million pushes), so each push costs O(N) amortised, roughly 48,000 times worse at this size. The deeper reason is that doubling makes the *copies a geometric series* whose sum is bounded by twice the last term; a fixed increment makes it an arithmetic series that grows quadratically. Real libraries use factors between 1.5 and 2.

**2 and 3.** Both are in one program: `has_cycle` is Floyd's tortoise and hare, and `merge` relinks nodes through a stack-allocated dummy head.

```c
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
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p2_lists p2_lists.c && ./p2_lists; rm -f p2_lists
a        1 5 9
b        2 5 8
merged   1 2 5 5 8 9
cycle in merged list? 0
after linking tail to the 3rd node: 1
```

*Cycle detection:* `slow` moves one step and `fast` two. Without a cycle, `fast` reaches `NULL`. With one, both enter the cycle, and inside it the gap between them shrinks by one each iteration (fast gains one node per step), so it must reach zero: they meet. Memory use is two pointers. (To find *where* the cycle starts, reset one pointer to the head after they meet and advance both one step at a time.) The test deliberately builds a cycle by linking the tail into the middle, then **undoes the link before freeing** so the cleanup loop terminates.

*Merge:* repeatedly take the smaller front node, append it to the tail, and advance that list. Using `<=` takes from `a` on ties, which keeps the merge **stable** (this is the merge step of merge sort). No node is created or copied: the work is O(n + m) and the extra space O(1). The dummy head means `tail->next = ...` works for the very first node too.

**4.**

```c
#include <stdio.h>

#define CAP 16

typedef struct { int a[CAP]; int n; } Stack;
static void spush(Stack *s, int v) { s->a[s->n++] = v; }
static int  spop(Stack *s)         { return s->a[--s->n]; }

/* A FIFO queue built from two LIFO stacks. Moving items between them reverses their order, which is exactly what is needed. */
typedef struct { Stack in, out; long moves; } Queue;

static void enqueue(Queue *q, int v) { spush(&q->in, v); }
static int dequeue(Queue *q) {
    if (q->out.n == 0)
        while (q->in.n > 0) { spush(&q->out, spop(&q->in)); q->moves++; }   /* refill only when the output stack is empty */
    return spop(&q->out);
}

int main(void) {
    Queue q = {0};
    for (int i = 1; i <= 4; i++) enqueue(&q, i * 10);
    int first = dequeue(&q);                        /* separate statements: argument evaluation order is unspecified */
    int second = dequeue(&q);
    printf("dequeue: %d %d\n", first, second);
    enqueue(&q, 50); enqueue(&q, 60);
    printf("then:   ");
    for (int i = 0; i < 4; i++) printf(" %d", dequeue(&q));
    printf("\nitems moved between stacks: %ld for 6 items (each item moves at most once)\n", q.moves);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p2_queue2stacks p2_queue2stacks.c && ./p2_queue2stacks; rm -f p2_queue2stacks
dequeue: 10 20
then:    30 40 50 60
items moved between stacks: 6 for 6 items (each item moves at most once)
```

Enqueue pushes onto the `in` stack. Dequeue pops from `out`; only when `out` is empty does it pour *all* of `in` into `out`, which reverses the order so the oldest item ends up on top. The crucial rule is to refill **only when `out` is empty**; refilling earlier would put new items ahead of old ones. Each item is pushed to `in` once, moved to `out` once and popped once (the counter shows 6 moves for 6 items), so any sequence of *n* operations costs O(n) in total: amortised O(1) each, though a single `dequeue` may take O(n).

</details>

## Key takeaways

1. Geometric growth makes total copying linear, so dynamic-array appends are amortised O(1); fixed-step growth is quadratic.
2. Fast and slow pointers detect a cycle in O(n) time and O(1) space; a dummy head node removes first-element special cases in list code.
3. Merging sorted lists is relinking, not copying; prefer `<=` on ties for stability.
4. Amortised analysis counts total work over a sequence, which is why the two-stack queue is O(1) per operation despite occasional O(n) steps.
