---
title: Linked Lists
track: dsa
module: Linear Structures
moduleOrder: 2
order: 2
level: intermediate
summary: Singly, doubly and circular lists in C — pointer manipulation patterns (reversal, middle, merge, cycle detection), sentinel nodes that remove special cases, list merge sort, and an honest look at when lists are the right tool.
sources:
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., §10.2 (Linked lists)
  - label: Knuth — The Art of Computer Programming, Vol. 1, §2.2 (Linear lists) and Vol. 3, §5.2.4 (Sorting by merging)
  - label: Floyd — Nondeterministic Algorithms (JACM 1967); Brent — An Improved Monte Carlo Factorization Algorithm (BIT 1980)
  - label: Linux kernel docs — Linked Lists (include/linux/list.h)
    url: https://docs.kernel.org/core-api/list.html
  - label: Stroustrup — Are lists evil?
    url: https://isocpp.org/blog/2014/06/stroustrup-lists
  - label: Binkley — Linked list problems (Stanford CS Education Library, Nick Parlante)
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

{{include dsa/lists/slist.c}}

{{run dsa/lists :: gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined slist.c -o slist && ./slist}}

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

{{include dsa/lists/dlist.c}}

{{run dsa/lists :: gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined dlist.c -o dlist && ./dlist}}

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
