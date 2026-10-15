---
kind: code
title: Merge two sorted linked lists
module: Linear Structures
difficulty: Medium
tags:
  - linked list
  - stable merge
starter: dsa/starters/lists/ex3-merge-lists.c
examples:
  - input: a = 1 5 9, b = 2 5 8
    output: 1a 2b 5a 5b 8b 9a
    why: The 5 from a precedes the 5 from b.
  - input: a = NULL, b = 4
    output: '4'
constraints:
  - Either list may be empty.
  - O(n + m) time, O(1) extra memory; every node in the result comes from the inputs.
hints:
  - A dummy head node removes the special case for the first element.
  - Take from `a` when `a->key <= b->key`, not `<`, to keep it stable.
  - Append the non-empty remainder at the end.
---

Merge two sorted lists by relinking their nodes (no allocation) and return the head. The merge must be **stable**: for equal keys the node from `a` comes before the node from `b`.

## Explanation

Both are in one program: `has_cycle` is Floyd's tortoise and hare, and `merge` relinks nodes through a stack-allocated dummy head.

*Cycle detection:* `slow` moves one step and `fast` two. Without a cycle, `fast` reaches `NULL`. With one, both enter the cycle, and inside it the gap between them shrinks by one each iteration (fast gains one node per step), so it must reach zero: they meet. Memory use is two pointers. (To find *where* the cycle starts, reset one pointer to the head after they meet and advance both one step at a time.) The test deliberately builds a cycle by linking the tail into the middle, then **undoes the link before freeing** so the cleanup loop terminates.

*Merge:* repeatedly take the smaller front node, append it to the tail, and advance that list. Using `<=` takes from `a` on ties, which keeps the merge **stable** (this is the merge step of merge sort). No node is created or copied: the work is O(n + m) and the extra space O(1). The dummy head means `tail->next = ...` works for the very first node too.
