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
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., §16.1 (Aggregate analysis) and Ch. 10
  - label: Sedgewick & Wayne — Algorithms, 4th ed., §1.3 (exercises on stacks, queues and linked lists)
  - label: Floyd, R. W. — Nondeterministic Algorithms (JACM, 1967), cycle detection
---

These exercises turn the module's ideas into numbers and pointer code. Write your solution first, then compare, and run everything under `-fsanitize=address,undefined`.

## Exercises

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Why doubling?** A dynamic array starts with capacity 1. Count how many element **copies** `N` pushes cost if it grows (a) by doubling and (b) by a fixed +10. Predict the ratio for `N = 1,000,000` before running anything, then state the amortised cost per push for each strategy.

**2. Cycle detection.** Given the head of a singly linked list, decide whether it contains a cycle using O(1) extra memory. (Hint: two pointers moving at different speeds.) Explain why they must meet if a cycle exists.

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/lists/ex2-cycle.c}}

</details>

**3. Merge sorted lists.** Merge two sorted singly linked lists into one sorted list **without allocating new nodes**. Keep the merge stable (equal keys from the first list stay first). Hint: a dummy head node removes the special case for the first element.

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/lists/ex3-merge-lists.c}}

</details>

**4. Queue from two stacks.** Implement a FIFO queue using only two LIFO stacks. Show that every item is moved between the stacks at most once, so `enqueue` and `dequeue` are amortised O(1).

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/lists/ex4-queue-two-stacks.c}}

</details>

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

{{include dsa/practice/p2_growth.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -o p2_growth p2_growth.c && ./p2_growth; rm -f p2_growth}}

Doubling copies about **N** elements in total (1,048,575 for a million pushes: 1 + 2 + 4 + ... + 2^19), so the amortised cost is about 1 copy per push: **O(1)**. Growing by a constant copies about N²/20 elements in total (50 billion for a million pushes), so each push costs O(N) amortised, roughly 48,000 times worse at this size. The deeper reason is that doubling makes the *copies a geometric series* whose sum is bounded by twice the last term; a fixed increment makes it an arithmetic series that grows quadratically. Real libraries use factors between 1.5 and 2.

**2 and 3.** Both are in one program: `has_cycle` is Floyd's tortoise and hare, and `merge` relinks nodes through a stack-allocated dummy head.

{{include dsa/practice/p2_lists.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p2_lists p2_lists.c && ./p2_lists; rm -f p2_lists}}

*Cycle detection:* `slow` moves one step and `fast` two. Without a cycle, `fast` reaches `NULL`. With one, both enter the cycle, and inside it the gap between them shrinks by one each iteration (fast gains one node per step), so it must reach zero: they meet. Memory use is two pointers. (To find *where* the cycle starts, reset one pointer to the head after they meet and advance both one step at a time.) The test deliberately builds a cycle by linking the tail into the middle, then **undoes the link before freeing** so the cleanup loop terminates.

*Merge:* repeatedly take the smaller front node, append it to the tail, and advance that list. Using `<=` takes from `a` on ties, which keeps the merge **stable** (this is the merge step of merge sort). No node is created or copied: the work is O(n + m) and the extra space O(1). The dummy head means `tail->next = ...` works for the very first node too.

**4.**

{{include dsa/practice/p2_queue2stacks.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p2_queue2stacks p2_queue2stacks.c && ./p2_queue2stacks; rm -f p2_queue2stacks}}

Enqueue pushes onto the `in` stack. Dequeue pops from `out`; only when `out` is empty does it pour *all* of `in` into `out`, which reverses the order so the oldest item ends up on top. The crucial rule is to refill **only when `out` is empty**; refilling earlier would put new items ahead of old ones. Each item is pushed to `in` once, moved to `out` once and popped once (the counter shows 6 moves for 6 items), so any sequence of *n* operations costs O(n) in total: amortised O(1) each, though a single `dequeue` may take O(n).

</details>

## Key takeaways

1. Geometric growth makes total copying linear, so dynamic-array appends are amortised O(1); fixed-step growth is quadratic.
2. Fast and slow pointers detect a cycle in O(n) time and O(1) space; a dummy head node removes first-element special cases in list code.
3. Merging sorted lists is relinking, not copying; prefer `<=` on ties for stability.
4. Amortised analysis counts total work over a sequence, which is why the two-stack queue is O(1) per operation despite occasional O(n) steps.
