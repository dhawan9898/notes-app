---
title: "Practice: Hash Functions, Probing and Bloom Filters"
track: dsa
module: Hashing
moduleOrder: 4
order: 3
level: intermediate
summary: "Three exercises on the hashing module: measuring how a poor hash ruins bucket distribution, reproducing the classic deletion bug in linear probing and fixing it with tombstones, and checking a Bloom filter's false-positive rate against its formula, with verified C answers."
prerequisites:
  - "Hash Tables: Open Addressing, Deletion, Cuckoo and Hash Flooding"
  - "Probabilistic Structures: Bloom, Count-Min, HyperLogLog and Consistent Hashing"
sources:
  - label: Knuth — The Art of Computer Programming, Vol. 3, §6.4 (Hashing)
  - label: Bloom, B. H. — Space/Time Trade-offs in Hash Coding with Allowable Errors (CACM, 1970)
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 11
---

Hash tables are simple to describe and easy to get subtly wrong. Each exercise is a small experiment: predict the outcome first, then run it.

## Exercises

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Judge a hash function.** Hash 2,000 pseudo-random lowercase words into 256 buckets using (a) the word's length, (b) its first letter, (c) djb2 (`h = h*33 + c`, starting at 5381). For each, count the buckets used and the longest chain. Predict the ranking and say what the *ideal* chain length is.

**2. The deletion bug.** A linear-probing table with 8 slots stores keys 8, 16 and 24, which all hash to slot 0 (`key % 8`). Delete 16 by simply marking its slot empty. What does a lookup of 24 return, and why? Fix it with a **tombstone** marker and show that lookups work and that a later insert can reuse the tombstone.

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/hashing/ex2-tombstone.c}}

</details>

**3. Bloom filter arithmetic.** A Bloom filter with `m = 8192` bits and `k = 4` hash functions holds `n = 1000` keys. Use the formula `(1 - e^(-kn/m))^k` to predict the false-positive rate, then build it and measure with 100,000 keys that were never inserted. Can a Bloom filter ever report a stored key as missing?

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

{{include dsa/practice/p4_hash.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -o p4_hash p4_hash.c && ./p4_hash; rm -f p4_hash}}

With 2,000 keys in 256 buckets a perfect spread gives about 8 per bucket (the table prints the integer 7). Hashing by **length** can produce only about 8 distinct values (words here are 3 to 10 letters), so 8 buckets hold everything and the longest chain is 260: lookups degrade toward a linear scan. **First letter** allows at most 26 values: 26 buckets, chains up to 90. **djb2** uses every bucket and the longest chain is 15, close to what random placement gives (the maximum of 256 random bucket loads exceeds the mean by a few standard deviations). A good hash function must use *all* the input and spread the output *evenly*; "cheap to compute" is not enough.

**2.**

{{include dsa/practice/p4_probe.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -o p4_probe p4_probe.c && ./p4_probe; rm -f p4_probe}}

Keys 8, 16 and 24 collide at slot 0 and probe into slots 1 and 2. The naive deletion empties slot 1; a lookup for 24 starts at slot 0, moves to slot 1, sees an **empty** slot and concludes "if 24 were here, probing would not have passed an empty slot", so it reports not found although 24 sits in slot 2. Probing relies on the invariant *no empty slot between a key's home and its position*, and a naive delete breaks it. A **tombstone** (`X`) means "occupied once; keep probing": `find` continues through it, and `insert` may reuse it (the last line). Tombstones accumulate, so real tables count them toward the load factor and rebuild when there are too many; the alternative is **backward-shift deletion**, which moves later keys of the cluster back and leaves no tombstone.

**3.**

{{include dsa/practice/p4_bloom.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -o p4_bloom p4_bloom.c -lm && ./p4_bloom; rm -f p4_bloom}}

The formula predicts about 2.23%; the measurement lands within a few hundredths of a percent of it. A false positive happens when all `k` bits of a never-inserted key were set by *other* keys. Bits are only ever set, never cleared, so a stored key always finds its `k` bits set: **no false negatives**, the first line prints 0 missing. The trade-off is the table's size: 8 bits per key here gives a 2% error rate, and about 10 bits per key with the optimal `k = (m/n) ln 2` gives 1%. You cannot delete from a plain Bloom filter (clearing a bit would also forget other keys); counting variants exist.

</details>

## Key takeaways

1. A hash function must use all of the key and spread results evenly; measure chain lengths instead of trusting intuition.
2. In open addressing, deleting by emptying a slot breaks probe chains; use tombstones (and rebuild) or backward-shift deletion.
3. A Bloom filter has false positives but never false negatives, and its error rate follows `(1 - e^(-kn/m))^k`.
