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
  - label: Sedgewick & Wayne — Algorithms, 4th ed., §1.3 (Bags, Queues, and Stacks)
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., §10.1 (Simple array-based implementations of stacks and queues) and §10.2 (Linked lists)
  - label: Kernighan & Ritchie — The C Programming Language, 2nd ed., §6.5 (Self-referential structures)
  - label: AddressSanitizer — Clang/GCC documentation
    url: https://github.com/google/sanitizers/wiki/AddressSanitizer
---

A **data structure** is a way of arranging data in memory so that the operations you need are fast. You already know the simplest one, the array. This note adds three more, each a few dozen lines of C: the **linked list** (cheap insertion and removal), the **stack** (last in, first out) and the **queue** (first in, first out). They are the building blocks of almost every later structure in this track. The in-depth versions are *Dynamic Arrays*, *Linked Lists* and *Stacks, Queues, Deques and Ring Buffers*.

## The linked list: a chain of small structs

An array keeps its items side by side, so inserting at the front means shifting everything one slot along, and its size is fixed or must be reallocated. A **linked list** stores each item in its own small `struct` (a **node**) allocated with `malloc`, and each node holds the address of the next one. You only ever keep the address of the first node, the **head**. The last node's `next` is `NULL`. If struct pointers feel unfamiliar, read *Structs, Enums and Typedef* first.

{{include dsa/essentials/list_basic.c}}

{{run dsa/essentials :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o list_basic list_basic.c && ./list_basic; rm -f list_basic}}

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

{{include dsa/essentials/brackets.c}}

{{run dsa/essentials :: gcc -std=c17 -Wall -Wextra -o brackets brackets.c && ./brackets; rm -f brackets}}

Every opening bracket is pushed; every closing bracket must match the item on top, which is the most recently opened one, and that is exactly what "properly nested" means. Notice the three failure cases: a closer that does not match, a closer with nothing open, and openers left over at the end. Also notice the guard `if (s.top == MAX)`: a fixed-size stack must check for overflow before pushing, or it writes past the array.

## The queue: first in, first out

A **queue** serves items in arrival order: `enqueue` at the back, `dequeue` from the front, like a line at a ticket window. Printers, network packet buffers and breadth-first search all use queues. A naive array queue wastes space because the front keeps moving right; a **ring buffer** fixes that by wrapping the indexes around with `% CAP`.

{{include dsa/essentials/queue_ring.c}}

{{run dsa/essentials :: gcc -std=c17 -Wall -Wextra -o queue_ring queue_ring.c && ./queue_ring; rm -f queue_ring}}

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
