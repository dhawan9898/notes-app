---
title: "Hash Tables: Open Addressing, Deletion, Cuckoo and Hash Flooding"
track: dsa
module: Hashing
moduleOrder: 4
order: 1
level: intermediate
summary: "A production-grade hash table in C: chaining vs open addressing, linear probing with backward-shift deletion verified against an oracle, measured probe counts against Knuth's formulas, resizing, cuckoo hashing, and a real hash-flooding attack defeated by SipHash."
prerequisites:
  - Dynamic Arrays
sources:
  - label: "Knuth, The Art of Computer Programming, Vol. 3 — §6.4 Hashing"
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, Ch. 11 Hash Tables"
  - label: "Celis, Larson, Munro — Robin Hood Hashing (FOCS 1985)"
  - label: "Pagh & Rodler — Cuckoo Hashing (Journal of Algorithms, 2004)"
    url: https://doi.org/10.1016/j.jalgor.2003.12.002
  - label: "Aumasson & Bernstein — SipHash: a fast short-input PRF (INDOCRYPT 2012)"
    url: https://www.aumasson.jp/siphash/siphash.pdf
  - label: "Crosby & Wallach — Denial of Service via Algorithmic Complexity Attacks (USENIX Security 2003)"
    url: https://www.usenix.org/legacy/events/sec03/tech/full_papers/crosby/crosby.pdf
  - label: "Fowler–Noll–Vo hash function"
    url: http://www.isthe.com/chongo/tech/comp/fnv/
---

A hash table maps keys to values in **expected O(1)** time by turning a key into an array index. Two decisions define a design: the **hash function** and the **collision strategy** (what happens when two keys land on the same index, which is certain by the pigeonhole principle once there are more keys than slots, and likely much earlier, by the birthday paradox: 23 random keys in 365 slots already collide with probability above 50%).

## Collision strategies

| | Separate chaining | Open addressing |
|---|---|---|
| Storage | array of list heads + heap nodes | single array of slots |
| Cache behavior | pointer chasing, poor | sequential probes, good |
| Memory overhead | pointer per entry + allocator overhead | empty slots (keep load ≤ 0.5–0.8) |
| Load factor α | can exceed 1 | must stay < 1 |
| Delete | trivial (unlink) | tombstones or backward shift |
| Worst case | long chain | long probe cluster |
| Pointer stability | nodes never move | entries move on resize and on delete |

Chaining is the right answer when entries are large, when stable pointers matter, or for simplicity (the Linux kernel's `hlist` tables are chained). Open addressing is the right answer when keys and values are small and speed matters. The rest of this note builds the latter and then looks at two situations where plain tables fail: guaranteed worst-case lookup (cuckoo) and hostile input (flooding).

## Choosing a hash function

Requirements: deterministic, fast, and **uniform in the low bits** when the capacity is a power of two (we mask rather than divide). Identity hashes on integers or the raw pointer value put aligned keys in the same few buckets. Use an avalanche step: flipping one input bit should flip about half of the output bits.

```c
static inline uint64_t mix64(uint64_t x) {      // SplitMix64 finalizer
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31; return x;
}
```

For strings, FNV-1a is the simplest decent non-keyed hash. For anything an outsider can feed you, use a **keyed** hash (see the flooding section).

## Open addressing with linear probing

Slots live in one array; a lookup starts at `hash & mask` and walks forward until it finds the key or an empty slot. The empty slot is what ends a failed search, so the table must never be completely full.

{{include dsa/hashing/oamap.h}}

### Why each detail matters

- **Power-of-two capacity** lets `& mask` replace `% cap` (a 64-bit division costs tens of cycles). It requires the mixing step above.
- **The probe loop terminates only because an empty slot exists.** The load check (`≤ 0.75` here) guarantees that.
- **Resizing** costs O(n) but happens at doubling intervals, so insertion is **amortized O(1)**. A single insert may be slow, which matters in latency-sensitive code; incremental rehashing (move a few buckets per operation) trades that for complexity.
- **Pointer invalidation**: any `put` may reallocate, so never hold a pointer into the table across inserts.

### Deletion: why you cannot just clear the slot

If `A` and `B` hash to the same home slot and `B` was placed after `A`, clearing `A`'s slot cuts the probe chain: a later search for `B` stops at the new hole and wrongly reports "absent". There are two fixes:

1. **Tombstones**: mark the slot "deleted". Lookups skip it, inserts may reuse it. The cost: tombstones count toward the load factor and degrade probe lengths, so the table must be rehashed periodically, and delete-heavy workloads (queues, caches) fill the table with tombstones.
2. **Backward-shift deletion** (used above): after emptying slot *i*, scan the rest of the run and move each later entry back into the hole if its home slot is not cyclically between the hole and its current position. This keeps the table exactly as if the deleted key had never been inserted: no tombstones, no periodic cleanup.

The cyclic condition is the subtle part and a classic source of bugs, so the implementation is tested against a trivial oracle with a small key space (many collisions, constant re-insertion and deletion), 2,000,000 random operations, under AddressSanitizer and UndefinedBehaviorSanitizer:

{{include dsa/hashing/test.c}}

{{run dsa/hashing :: gcc -O2 -Wall -Wextra -Wno-unused-function -fsanitize=address,undefined test.c -o t && ./t}}

### The cost model, measured

With load factor `α = n / capacity` and a good hash, Knuth (§6.4) gives the expected probes under linear probing:

- successful search: `½ · (1 + 1/(1−α))`
- unsuccessful search / insert: `½ · (1 + 1/(1−α)²)`

Measuring them with 1M slots and keys inserted at each load:

{{include dsa/hashing/probes.c}}

{{run dsa/hashing :: gcc -O2 -Wall -Wextra -Wno-unused-function probes.c -o p -lm && ./p}}

The measured and predicted columns agree to two digits. Read them as the design rule: lookups of present keys stay cheap until α ≈ 0.75, but **misses cost 49 probes at α = 0.9**, which is why implementations resize at 0.5–0.75. Linear probing suffers *primary clustering* (runs of occupied slots grow and merge), but each probe reads an adjacent address, so in practice it beats quadratic probing or double hashing until the table is fairly full.

## Variations worth knowing

- **Robin Hood hashing**: on insert, an entry that has probed farther than the resident *steals* its slot and the displaced one continues. This equalizes probe lengths (low variance), lets a miss stop early (once the probe distance exceeds the resident's), and pairs with backward-shift deletion.
- **Swiss tables / SIMD probing** (Abseil, Rust's `hashbrown`): one byte of metadata per slot, 16 compared at once with SSE2, so a probe group is a single load and compare.
- **Quadratic probing / double hashing**: avoid primary clustering but jump around memory; usually slower than linear probing with a good hash.
- **Storing the hash** in each slot speeds resizing and rejects mismatches without comparing long keys.

## Cuckoo hashing: worst-case O(1) lookups

Open addressing has no worst-case bound on lookup. **Cuckoo hashing** uses two tables and two hash functions; every key lives in exactly one of two possible slots, so a lookup is **at most two probes, always**. Insert puts the key in its slot in table 0; if occupied, the resident is *kicked out* to its slot in table 1, which may kick another back, and so on. A long chain means a cycle, so the table is rebuilt with new hash seeds. Theory (Pagh & Rodler) says two tables work up to about 50% load with expected O(1) insert; more tables or buckets of several slots raise the usable load to 90% and above.

{{include dsa/hashing/cuckoo.c}}

{{run dsa/hashing :: gcc -O2 -Wall -Wextra cuckoo.c -o cuckoo && ./cuckoo}}

Kicks per insert grow as the load approaches 50% (the last row), and past that the rebuild loop would never finish. Cuckoo shines where lookups dominate and the worst case matters (packet classification, hardware tables, read-mostly caches); the cost is two cache misses per lookup and lower density.

## Hash flooding: when the input is hostile

Everything above assumes keys hash uniformly. If an attacker chooses the keys and knows the hash function, they can make **every key collide**, turning O(1) into O(n) per operation and the whole build into O(n²). This is a real, practical attack (Crosby & Wallach 2003; the 2011 "hash collision" disclosures hit PHP, Java, Ruby, Python and ASP.NET; one small POST body with a few thousand crafted parameter names stalled a server).

The demonstration uses the polynomial hash `h = 31·h + c` (Java's `String.hashCode`). Because `"Aa"` and `"BB"` both hash to 2112, *any* concatenation of those two blocks collides with any other of the same length, so *k* blocks give 2^k colliding strings. The defence is a **keyed** hash with a secret, per-process random key, e.g. **SipHash-2-4**, which is designed so that without the key an attacker cannot predict collisions.

{{include dsa/hashing/siphash.h}}

{{include dsa/hashing/flood.c}}

{{run dsa/hashing :: gcc -O2 -Wall -Wextra flood.c -o flood && ./flood}}

The self-test line checks the implementation against the SipHash paper's reference vector. With the weak hash, 65,536 crafted keys cost over two *billion* chain steps (≈ n²/2: each insert walks the whole chain) and every key is in one chain. With SipHash on the same keys the longest chain is a handful. In practice:

- Use a randomly seeded keyed hash for any table keyed by external data (Python, Ruby, Rust's `HashMap`, Perl and Linux's `siphash` for network structures all do).
- Seeding a fast non-cryptographic hash (e.g. a random seed in murmur-style hashes) raises the bar but is weaker than a PRF like SipHash against a determined attacker.
- Cap input sizes and parameter counts; use balanced trees (O(log n) worst case) as the bucket structure, as Java 8's `HashMap` does when a chain exceeds 8.

## Practical guidance

- Pre-size when you know `n`: `cap = next_pow2(n * 4 / 3 + 1)` avoids repeated rehashing.
- Compute the hash once per operation and store it with the entry when keys are long.
- Iteration order is arbitrary and changes on resize; never depend on it, and do not mutate while iterating.
- Test with a degenerate workload (sequential keys, keys that are multiples of 1024, delete-heavy churn) and an oracle comparison, not only random data.
- If you need ordered traversal or range queries, a hash table is the wrong structure; use a balanced tree or a skip list.

## Key takeaways

- Open addressing = one flat array + probing: simple and fast, but α must stay well below 1; misses degrade much faster than hits.
- Measured probe counts match Knuth's formulas: keep α ≤ 0.75 (≤ 0.5 for miss-heavy workloads).
- Backward-shift deletion avoids tombstones, but its cyclic-distance condition needs an oracle-based test.
- Power-of-two sizing needs a hash with good low-bit mixing. Amortized O(1) insertion comes from geometric resizing.
- Cuckoo hashing gives two-probe worst-case lookups at lower load; use it where tail latency matters.
- For attacker-controlled keys, use a keyed hash such as SipHash; a weak hash makes the table an O(n²) denial-of-service vector.
