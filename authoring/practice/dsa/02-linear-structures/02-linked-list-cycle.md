---
kind: code
title: Detect a cycle in a linked list
module: Linear Structures
difficulty: Medium
tags:
  - linked list
  - two pointers
starter: dsa/starters/lists/ex2-cycle.c
examples:
  - input: 0 -> 1 -> 2 -> 3 -> 4 -> 5 -> NULL
    output: '0'
  - input: 0 -> ... -> 5 -> back to 2
    output: '1'
  - input: a node pointing to itself
    output: '1'
constraints:
  - '`head` may be NULL.'
  - Do not modify the list and do not use a hash set.
hints:
  - 'Two pointers: one moves one step, the other two steps.'
  - If they ever meet there is a cycle; if the fast one reaches NULL there is none.
---

Does a singly linked list contain a cycle? Use **O(1) extra memory**. The list may be empty, a single node, or a node that points to itself.

## Explanation

Both are in one program: `has_cycle` is Floyd's tortoise and hare, and `merge` relinks nodes through a stack-allocated dummy head.

*Cycle detection:* `slow` moves one step and `fast` two. Without a cycle, `fast` reaches `NULL`. With one, both enter the cycle, and inside it the gap between them shrinks by one each iteration (fast gains one node per step), so it must reach zero: they meet. Memory use is two pointers. (To find *where* the cycle starts, reset one pointer to the head after they meet and advance both one step at a time.) The test deliberately builds a cycle by linking the tail into the middle, then **undoes the link before freeing** so the cleanup loop terminates.

*Merge:* repeatedly take the smaller front node, append it to the tail, and advance that list. Using `<=` takes from `a` on ties, which keeps the merge **stable** (this is the merge step of merge sort). No node is created or copied: the work is O(n + m) and the extra space O(1). The dummy head means `tail->next = ...` works for the very first node too.
