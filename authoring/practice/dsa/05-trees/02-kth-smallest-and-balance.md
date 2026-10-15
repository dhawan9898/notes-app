---
kind: code
title: k-th smallest and balance check
module: Trees
difficulty: Medium
tags:
  - binary search tree
  - in-order
  - height
starter: dsa/starters/trees/ex2-kth-balanced.c
examples:
  - input: kth_smallest(good tree of 7 keys, 3)
    output: 1, key 40
  - input: kth_smallest(good tree, 8)
    output: '0'
  - input: is_balanced(chain 1 - 2 - 3)
    output: '0'
constraints:
  - Empty tree is balanced.
  - 'Equal heights at the root are not enough: check every node.'
hints:
  - 'In-order traversal visits keys in ascending order: count as you go and stop at k.'
  - Compute height and balance in one pass; return a sentinel (-1) when a subtree is unbalanced.
---

Implement two functions on a binary search tree:

1. `kth_smallest(root, k, &out)` stores the k-th smallest key (`k = 1` is the smallest) and returns 1, or returns 0 if the tree has fewer than `k` keys. Do not copy the tree into an array.
2. `is_balanced(root)` returns 1 if at **every** node the heights of the two subtrees differ by at most 1.

## Explanation

*Validation.* The invariant is about **ranges**, not neighbours: every key in a node's left subtree must be smaller than the node *and* larger than everything above that bound. The trap tree has `55` as the right child of `30` (55 > 30, so the parent-only check passes) but it sits in the left subtree of `50`, so it is too large. The correct check passes an open interval `(lo, hi)` down the recursion: going left sets `hi = node->key`, going right sets `lo = node->key`. (Use a wider type than the keys for the initial bounds, as the code does with `long`, or the extreme key `INT_MIN` would be wrongly rejected.) An alternative proof: an in-order traversal of a valid BST is strictly increasing.

*k-th smallest.* An in-order walk visits keys in sorted order, so count down `k` and stop when it reaches zero: O(height + k) time, O(height) stack. Augmenting each node with its subtree size gives O(height) lookup by rank.

*Balance.* The function returns the height, or `-1` as an "unbalanced" signal that propagates up, so each node is visited once (O(n)). Computing heights separately at every node would be O(n²) on a chain. The chain `1-2-3` is correctly reported unbalanced.
