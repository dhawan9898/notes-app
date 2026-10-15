---
kind: code
title: Deleting from a probing table
module: Hashing
difficulty: Medium
tags:
  - hash table
  - open addressing
starter: dsa/starters/hashing/ex2-tombstone.c
examples:
  - input: insert 8, 16, 24 (all hash to slot 0); remove_key(16)
    output: find(24) == 2, find(16) == -1
  - input: then insert(32)
    output: find(32) == 1 (the freed slot is reused)
constraints:
  - Removing a key that is not present changes nothing.
  - The table has 8 slots; `EMPTY` and `TOMBSTONE` are the two special values.
hints:
  - Mark the slot `TOMBSTONE` instead of `EMPTY`.
  - '`find` keeps probing past a tombstone, and `insert` may reuse one.'
---

In a linear-probing table, setting a deleted slot to `EMPTY` breaks the probe chain of keys stored after it: `find` stops at the hole and misses them. Fix `remove_key` so later searches still work and the freed slot can be reused by `insert`.

## Explanation

Keys 8, 16 and 24 collide at slot 0 and probe into slots 1 and 2. The naive deletion empties slot 1; a lookup for 24 starts at slot 0, moves to slot 1, sees an **empty** slot and concludes "if 24 were here, probing would not have passed an empty slot", so it reports not found although 24 sits in slot 2. Probing relies on the invariant *no empty slot between a key's home and its position*, and a naive delete breaks it. A **tombstone** (`X`) means "occupied once; keep probing": `find` continues through it, and `insert` may reuse it (the last line). Tombstones accumulate, so real tables count them toward the load factor and rebuild when there are too many; the alternative is **backward-shift deletion**, which moves later keys of the cluster back and leaves no tombstone.
