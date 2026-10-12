---
title: Linked Lists
track: dsa
module: Linear Structures
moduleOrder: 2
order: 2
level: intermediate
summary: Singly, doubly and circular lists in C — pointer manipulation patterns (reversal, middle, merge, cycle detection), sentinel nodes that remove special cases, list merge sort, and an honest look at when lists are the right tool.
prerequisites:
  - Pointers for Beginners
  - Dynamic Arrays
sources:
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., §10.2 (Linked lists)"
  - label: "Knuth — The Art of Computer Programming, Vol. 1, §2.2 (Linear lists) and Vol. 3, §5.2.4 (Sorting by merging)"
  - label: "Floyd — Nondeterministic Algorithms (JACM 1967); Brent — An Improved Monte Carlo Factorization Algorithm (BIT 1980)"
  - label: "Linux kernel docs — Linked Lists (include/linux/list.h)"
    url: https://docs.kernel.org/core-api/list.html
  - label: "Stroustrup — Are lists evil?"
    url: https://isocpp.org/blog/2014/06/stroustrup-lists
  - label: "Binkley — Linked list problems (Stanford CS Education Library, Nick Parlante)"
    url: http://cslibrary.stanford.edu/105/LinkedListProblems.pdf
---

A **linked list** is a sequence of nodes where each node stores a value and a pointer to the next node. It trades the array's O(1) indexing for O(1) insertion and removal *given a pointer to the position*, and for stable element addresses (nodes never move). Linked lists dominate textbooks and interviews far more than production code, but their patterns — pointer rewiring, sentinels, two-pointer traversal — recur in kernels, allocators, LRU caches, hash chains and graph adjacency lists, so they are worth mastering.

## Variants

| Variant | Node | Pros | Cons |
|---|---|---|---|
| **Singly linked** | `value, next` | smallest; O(1) push front; simple | no backward traversal; delete needs the predecessor |
| **Doubly linked** | `value, prev, next` | O(1) delete given the node; bidirectional | +8 bytes per node; more pointers to maintain |
| **Circular** | last points to first | round-robin iteration; no NULL terminator | termination condition differs |
| **With sentinel (dummy head/tail)** | permanent extra node | eliminates empty/first/last special cases | one extra node |
| **Intrusive** | the link is embedded in the user's struct (`container_of`) | no extra allocation; one object on many lists | needs care with lifetimes (see the C function-pointer note) |
| **XOR list** | one field `prev ^ next` | halves link memory | breaks GC, debuggers, many tools; a curiosity |
| **Skip list** | multi-level forward pointers | O(log n) search (balanced-tree note) | randomization, memory |
| **Unrolled list** | each node holds an array of elements | cache-friendly, fewer allocations | complexity of splits/merges |

| Operation | Singly | Doubly (sentinel) | Array |
|---|---|---|---|
| Access by index | O(n) | O(n) | O(1) |
| Push/pop front | O(1) | O(1) | O(n) |
| Push back | O(1) with tail pointer | O(1) | O(1) amortized |
| Pop back | O(n) (need predecessor) | O(1) | O(1) |
| Insert/delete at a known node | O(1) after; delete O(n) w/o predecessor | O(1) | O(n) |
| Search | O(n) | O(n) | O(n) |
| Memory per element | value + 8 (+ malloc overhead 8–16) | value + 16 (+ overhead) | value |

## Singly linked list patterns

All of these run under AddressSanitizer in `slist.c`:

```c
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
```

```text
$ gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined slist.c -o slist && ./slist
list:     1 2 3 4 5 
reversed: 5 4 3 2 1 
middle of 5 nodes = 3
cycle detected: 1 after 10 slow-pointer steps, cycle length 5, entry node value 7
after breaking the cycle: has_cycle=0
sort_list: 2000 random lists sorted with length preserved: yes
```

### Reversal

`prev = NULL; cur = head; while (cur) { next = cur->next; cur->next = prev; prev = cur; cur = next; }` — the invariant: *`prev` points to the already-reversed prefix, `cur` to the untouched suffix*. O(n) time, O(1) space, one pass. The recursive version is O(n) stack: avoid it for long lists (see the recursion note). Reversal underlies palindrome checks, reversing sublists, and "add two numbers stored as lists".

### Two-pointer (slow/fast) techniques

- **Middle**: advance `slow` by 1 and `fast` by 2; when `fast` reaches the end, `slow` is at the middle (the program prints 3 for 1…5).
- **Nth from the end**: advance `fast` N nodes ahead, then move both until `fast` hits the end.
- **Cycle detection (Floyd's tortoise and hare)**: if there is a cycle, the fast pointer eventually laps the slow one inside it. Space O(1) versus O(n) for a hash set of visited nodes. The run built a list with a 7-node tail feeding a 5-node cycle: the pointers met after 10 steps, the cycle length (5) was measured by walking once around from the meeting point, and the **entry node** (value 7) was found by resetting one pointer to the head and advancing both by one — correct because `tail_len ≡ (cycle_len − meeting_offset) mod cycle_len`. Brent's variant uses powers of two and is typically faster in practice. Cycles in *memory structures* (corrupted lists, circular references) are also found this way.

### Merge and merge sort

Merging two sorted lists splices nodes; using a **dummy head node** (`Node dummy; tail = &dummy`) removes the "is the result empty?" branch. List **merge sort** is O(n log n) time with no auxiliary array and O(log n) stack, and is *stable*; it is the standard way to sort a linked list (quick sort and heap sort need random access). The test sorted 2,000 random lists and verified order and length. Linux's `list_sort()` uses a bottom-up merge sort tuned for cache behavior.

### The pointer-to-pointer idiom (delete without a special case)

Deleting node `x` from a singly linked list normally needs a special case when `x` is the head. Keep a pointer to the *link* that points at the current node:

```c
void list_remove(Node **head, int v) {
    for (Node **link = head; *link; link = &(*link)->next)
        if ((*link)->value == v) { Node *dead = *link; *link = dead->next; free(dead); return; }
}
```

`link` initially points at `head` itself, later at some node's `next` field; the same assignment `*link = dead->next` works in both cases. This is the "good taste" example popularized by Linus Torvalds.

## Doubly linked list with a sentinel

A circular list whose head is a permanent **sentinel** node (never holds data) makes the empty list `head.next == head.prev == &head`, so insertion and deletion are four pointer assignments with no `NULL` checks:

```c
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
```

```text
$ gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined dlist.c -o dlist && ./dlist
after pushes:             5 10 20 30 40  (len 5, backward: 40 30 20 10 5)
insert 35 after 30:       5 10 20 30 35 40  (len 6, backward: 40 35 30 20 10 5)
remove node 30 (O(1)):    5 10 20 35 40  (len 5, backward: 40 35 20 10 5)
remove first and last:    10 20 35  (len 3, backward: 35 20 10)
after clear:               (len 0, backward:)
```

Both directions stay consistent after every operation (the `backward:` list in each line is the exact reverse), `remove` of any node costs O(1) given only that node (this is why LRU caches pair a hash table with a doubly linked list), and the same code handles first, last and only element. The kernel's `list_head` is exactly this structure, made intrusive.

Invariants to assert in debug builds: `n->next->prev == n` and `n->prev->next == n` for every node; `len` equals the traversal count; the sentinel is never freed or returned to users.

## Common bugs

1. **Losing the rest of the list**: overwriting `cur->next` before saving it (reverse, delete in loops). Save `next` first.
2. **Off-by-one at the head/tail**: use sentinels or the pointer-to-pointer idiom.
3. **Use after free** while iterating and freeing: `p = p->next` after `free(p)` — read `next` before freeing (`free_list` above).
4. **Memory leaks**: every `malloc`ed node needs a path to `free`; destroy functions must traverse.
5. **Aliasing/double insertion**: inserting a node that is already in a list corrupts it (intrusive lists especially).
6. **Cycles** introduced by bugs make every traversal infinite; add a debug check (Floyd) or a length counter.
7. **Concurrent modification**: lists are not thread-safe; lock-free lists need hazard pointers or RCU (see the concurrency note).
8. **Pointer comparison/sorting** assumptions: list order is unrelated to memory order.

## When to use lists (honestly)

Use a linked list when most of these hold: elements are **large or immovable** (pointers to them must stay valid), you **insert/remove in the middle through handles** you already have (LRU caches, schedulers' run queues, timer lists, free lists, hash chains), the sequence is **short or traversed rarely**, you cannot afford the occasional large reallocation (hard real-time), or you need an object on **several lists at once** (intrusive). Elsewhere a dynamic array or deque wins: the measurements in the dynamic-array note show a 7–200× traversal advantage and 8× less memory for arrays, because lists pay for `malloc` per node, 8–16 bytes of allocator overhead, a pointer per element, and cache misses on every hop.

Real systems use lists constantly *around* arrays: the Linux kernel uses intrusive lists for wait queues, task lists and device lists; allocators thread free blocks through themselves; hash tables chain colliding entries; LRU caches combine hash + doubly linked list; adjacency lists in graphs (often compressed to arrays — CSR — once built).

## Key takeaways

- Linked lists give O(1) insert/delete at a known node and stable addresses, but O(n) indexing and poor cache behavior.
- Master the patterns: in-place reversal (prefix/suffix invariant), slow/fast pointers (middle, Nth from end, Floyd cycle detection with entry point), dummy-head merge, merge sort for lists, and the pointer-to-pointer deletion idiom.
- A circular doubly linked list with a sentinel removes all special cases; assert `n->next->prev == n`.
- Save `next` before freeing or relinking; test with ASan and randomized checks (sortedness + length).
- Prefer arrays/deques unless you need handle-based removal, immovable elements, or multi-list membership (intrusive lists).
