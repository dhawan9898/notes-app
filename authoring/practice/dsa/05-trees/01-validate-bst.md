---
kind: code
title: Validate a binary search tree
module: Trees
difficulty: Medium
tags:
  - binary search tree
  - recursion
starter: dsa/starters/trees/ex1-valid-bst.c
examples:
  - input: 50 (30 (20, 40), 70 (60, 80))
    output: '1'
  - input: 50 (30 (20, 55), 70)
    output: '0'
    why: 55 is the right child of 30, but it sits in the left subtree of 50.
  - input: empty tree
    output: '1'
constraints:
  - Keys are distinct; the tree may be empty.
  - O(n) time.
hints:
  - Pass down the range `(low, high)` of keys allowed at each node.
  - Going left tightens `high` to the node’s key; going right tightens `low`.
  - 'An in-order walk must be strictly increasing: another valid approach.'
---

Return 1 if the binary tree is a valid binary search tree: every key in a node’s **left subtree** is smaller than the node’s key and every key in its **right subtree** is larger. Checking only the direct children is not enough.

## Explanation

*Validation.* The invariant is about **ranges**, not neighbours: every key in a node's left subtree must be smaller than the node *and* larger than everything above that bound. The trap tree has `55` as the right child of `30` (55 > 30, so the parent-only check passes) but it sits in the left subtree of `50`, so it is too large. The correct check passes an open interval `(lo, hi)` down the recursion: going left sets `hi = node->key`, going right sets `lo = node->key`. (Use a wider type than the keys for the initial bounds, as the code does with `long`, or the extreme key `INT_MIN` would be wrongly rejected.) An alternative proof: an in-order traversal of a valid BST is strictly increasing.

*k-th smallest.* An in-order walk visits keys in sorted order, so count down `k` and stop when it reaches zero: O(height + k) time, O(height) stack. Augmenting each node with its subtree size gives O(height) lookup by rank.

*Balance.* The function returns the height, or `-1` as an "unbalanced" signal that propagates up, so each node is visited once (O(n)). Computing heights separately at every node would be O(n²) on a chain. The chain `1-2-3` is correctly reported unbalanced.
