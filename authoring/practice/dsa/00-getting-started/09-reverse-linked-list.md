---
kind: code
title: Reverse a linked list
module: Getting Started
difficulty: Easy
tags:
  - linked list
  - pointers
starter: dsa/starters/start-here/ex9-reverse-list.c
examples:
  - input: 1 -> 2 -> 3 -> 4
    output: 4 -> 3 -> 2 -> 1
  - input: single node 9
    output: 9 (next is NULL)
  - input: 'NULL'
    output: 'NULL'
constraints:
  - The list may be empty.
  - O(n) time, O(1) extra memory.
hints:
  - 'Walk the list with three pointers: previous, current and next.'
  - Save `current->next` before overwriting it.
---

Reverse a singly linked list **in place** by changing the `next` pointers and return the new head. Do not allocate any nodes.

## Explanation

**Cost.** O(n), O(1).

**Key idea.** keep `prev`, `current` and save `next` *before* overwriting it.
