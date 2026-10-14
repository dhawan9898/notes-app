---
title: "Hash Tables for Beginners: Fast Lookup by Key"
track: dsa
module: DSA Essentials
moduleOrder: 0.5
order: 3
level: beginner
summary: "How a hash table finds a value by key in about one step: the hash function, buckets, collisions and chaining, built as a working word counter in C with the buckets printed so you can see collisions, plus load factor and when not to use a hash table."
prerequisites:
  - "Thinking in Data Structures"
  - "Structs, Enums and Typedef"
  - "Linked Lists, Stacks and Queues for Beginners"
sources:
  - label: Sedgewick & Wayne — Algorithms, 4th ed., §3.4 (Hash Tables)
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 11 (Hash Tables)
  - label: Bernstein — the djb2 string hash (as documented by Dan Bernstein and collected on the York University hash-functions page)
    url: http://www.cse.yorku.ca/~oz/hash.html
  - label: Kernighan & Pike — The Practice of Programming, Ch. 2 (Algorithms and Data Structures: hash tables)
---

Suppose you are counting how many times each word appears in a book. With an array of (word, count) pairs you would search the array for every word: with 50,000 distinct words, that is up to 50,000 comparisons *per word*. A **hash table** answers "have I seen this key, and what is its value?" in about **one step on average**, no matter how many keys it holds. It is behind dictionaries in Python, `std::unordered_map` in C++, objects in JavaScript, and the caches and symbol tables inside compilers and operating systems.

## The idea in three steps

1. Keep an array of **buckets** (here, 8 of them).
2. A **hash function** turns any key into a number. Compute `hash(key) % number_of_buckets` and that is the bucket where the key belongs. The same key always gives the same bucket, so you only ever need to look in that one bucket.
3. Different keys can land in the same bucket; this is a **collision**, and it is normal. The simplest fix is **chaining**: each bucket is a small linked list (see *Linked Lists, Stacks and Queues for Beginners*) holding every key that hashed there.

{{include dsa/essentials/wordcount.c}}

{{run dsa/essentials :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o wordcount wordcount.c && ./wordcount; rm -f wordcount}}

Look at the buckets. Twelve words, 8 of them distinct, were spread over 8 buckets: `to` has bucket 0 to itself, but `sat` and `cat` share bucket 5, and `a`, `ran` and `the` share bucket 6. A lookup hashes the word, jumps straight to that bucket and compares against only the 1–3 entries in it, instead of all 8. `lookup("dog")` returns 0 because the one bucket it hashes to contains no `dog`; absence is detected just as quickly.

The pieces to study in the code:

- **`hash`** is djb2: start at 5381 and for each character compute `h * 33 + c`. It is not magic; any function that mixes all the characters and spreads results evenly will do. A *bad* hash (for example, just the first letter) sends many keys to the same bucket and ruins the speed.
- **`% BUCKETS`** converts the large hash into a bucket number. With unsigned arithmetic, overflow in the hash is well-defined wrap-around.
- **`add`** first searches the chain with `strcmp` (keys are *compared*, not just hashed, because different words can have the same hash), then either bumps the count or pushes a new entry at the front.
- **`copy_string`** duplicates the word, since the table must own its own copy: the caller's array may change later. (`strdup` does this, but it is not part of standard C17, so the program writes it by hand.)
- The cleanup loop frees every word and every entry.

## Why it is fast, and when it stops being fast

If the hash spreads keys evenly, each of *b* buckets holds about *n / b* keys, the **load factor**. With the load factor kept near 1 (about as many buckets as keys), a chain has just a handful of entries, so lookup, insert and delete cost **O(1) on average**. This table has 8 buckets and 8 keys, which is fine for a demonstration. For a million words it would be hopeless, so real tables **resize**: when the load factor passes a limit (often 0.75 to 1), allocate a bigger bucket array and re-insert everything. That rehashing is occasionally expensive but rare, so the average cost stays O(1) (the same *amortized* reasoning as *Dynamic Arrays*).

The worst case is **O(n)**: if every key lands in one bucket, the table degenerates into one long linked list. That can happen by accident with a poor hash, or on purpose (an attacker who knows your hash function sends keys that all collide, called a hash-flooding attack; modern libraries use randomised hash functions). The later note *Hash Tables: Open Addressing, Deletion, Cuckoo and Hash Flooding* covers this and the alternative to chaining.

## What a hash table cannot do

- **No order.** Keys come out in bucket order, not sorted and not in insertion order. If you need them sorted, use a sorted array or a tree.
- **No "next larger key" or range queries** ("all words between *d* and *g*"). A tree answers those; a hash table does not.
- **Keys must be comparable for equality and hashable consistently**: two equal keys must always produce equal hashes.
- **Memory overhead and poor cache behaviour** compared with a plain array.

## When to use one

| You need | Choose |
|---|---|
| Count or group by key, remove duplicates, "have I seen this?" | hash table |
| Cache results by input | hash table |
| Keys in sorted order or range queries | sorted array or balanced tree |
| Few items (under about 20) | an array and a linear scan are simpler and often faster |

## Pitfalls

- **Hashing without comparing**: two different keys can share a hash and a bucket; always compare the actual keys.
- **Forgetting that the table owns its keys** (storing a pointer to a buffer that is later overwritten).
- **Using a table whose size never grows** so chains get long as the data grows.
- **Poor hash functions** (first letter, string length) that cluster keys.
- **Memory leaks**: free each key and each entry.
- **Mutating a key** after inserting it, so it no longer hashes to the bucket where it is stored.

## Key takeaways

1. A hash table maps keys to buckets with a hash function, so lookup, insert and delete take O(1) time on average.
2. Collisions are normal; chaining keeps colliding keys in a small list per bucket and lookups compare the real keys.
3. Keep the load factor near or below 1 by resizing; a bad or attacked hash degrades to O(n).
4. Hash tables give fast exact-match lookup but no ordering and no range queries.
