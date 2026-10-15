---
title: "Probabilistic Structures: Bloom, Count-Min, HyperLogLog and Consistent Hashing"
track: dsa
module: Hashing
moduleOrder: 4
order: 2
level: advanced
summary: "Trading exactness for orders of magnitude less memory — hash quality and avalanche, Bloom filters sized from the formulas, Count-Min sketches, HyperLogLog and consistent hashing, each measured against its theory."
prerequisites:
  - "Hash Tables: Open Addressing, Deletion, Cuckoo and Hash Flooding"
sources:
  - label: Bloom — Space/Time Trade-offs in Hash Coding with Allowable Errors (CACM 13(7), 1970)
  - label: Cormode & Muthukrishnan — An Improved Data Stream Summary, the Count-Min Sketch and its Applications (J. Algorithms 55(1), 2005)
  - label: Flajolet, Fusy, Gandouet, Meunier — HyperLogLog, the analysis of a near-optimal cardinality estimation algorithm (AofA 2007)
  - label: Karger et al. — Consistent Hashing and Random Trees (STOC 1997)
  - label: Kirsch & Mitzenmacher — Less Hashing, Same Performance, Building a Better Bloom Filter (2006)
    url: https://www.eecs.harvard.edu/~michaelm/postscripts/rsa2008.pdf
  - label: Appleby — MurmurHash3 and SMHasher (finalizer mix constants)
  - label: Aumasson & Bernstein — SipHash, a fast short-input PRF (INDOCRYPT 2012)
    url: https://www.aumasson.jp/siphash/siphash.pdf
  - label: Mitzenmacher & Upfal — Probability and Computing, Ch. 5 and 14
---

A hash table answers "is x in the set?" exactly, at the price of storing every key. Many systems do not need exactness: a database wants to skip a disk read when a key is *definitely* absent, a network monitor wants the top talkers among billions of packets, an analytics pipeline wants the number of distinct visitors. All of these can be answered from a few kilobytes if you accept a small, **quantified** error. The structures here share one idea — hash the item, and keep only a tiny, lossy summary of the hash — and one discipline: the error is not "approximately right"; it has a proven bound, and we can measure that it holds.

## 1. Everything starts with hash quality

A sketch is only as good as the independence of its hash functions. The standard test is **avalanche**: flipping one input bit should flip each output bit with probability ½, so on average 32 of 64 output bits change. Weak hashes fail this in ways that silently break the guarantees below.

{{include dsa/probabilistic/hash.h}}

`mix64` is the SplitMix64 / Murmur3-style finalizer: two multiply-xorshift rounds. `hash64(key, seed)` derives an independent-looking family from one function by seeding — the usual trick for "k hash functions" (Kirsch & Mitzenmacher show two suffice for Bloom filters, but with a strong mixer, seeding is simpler to reason about).

{{include dsa/probabilistic/avalanche.c}}

{{run dsa/probabilistic :: gcc -std=gnu11 -O2 -Wall -Wextra avalanche.c -o avalanche && ./avalanche}}

Read the table carefully:

- **Identity** changes exactly 1 output bit per input bit flip: perfect locality, zero mixing. Fine for a table indexed by small dense integers; fatal for sketches.
- **Multiplying by the golden ratio** (Fibonacci hashing) spreads information *upward*: high output bits depend on many input bits, but the low bits depend only on the low input bits. Used correctly — take the **high** bits — it is a good, very cheap table index function. Used with the low bits it is useless, as the bucket test shows: every multiple of 1024 lands in one bucket.
- **FNV-1a** is byte-at-a-time and passes the bucket test, but its avalanche is incomplete on short inputs (some input bits move only ~9 output bits).
- **`mix64`** is within noise of ideal on every input bit.

Two more points. First, for *adversarial* inputs (network-facing hash tables), no fixed function is safe: an attacker who knows it can craft colliding keys and degrade a table to O(n) per operation (hash flooding). The defence is a keyed pseudo-random function — **SipHash** is what Python, Rust and the Linux kernel use for this — with a random per-process key. Second, the guarantees of the structures below are proven for *random* hash functions; a good mixer approximates that for non-adversarial data, and the experiments here check that it does.

## 2. Bloom filter: set membership in ~10 bits per key

A Bloom filter is an array of `m` bits and `k` hash functions. **Insert** sets the `k` bits `h_i(x) mod m`. **Query** reports "possibly present" only if all `k` bits are set. There are no false negatives (an inserted key's bits are never cleared), but false positives occur when other keys happened to set all `k` bits.

**The math.** After inserting `n` keys, a given bit is still 0 with probability `(1 − 1/m)^{kn} ≈ e^{−kn/m}`. A false positive needs `k` set bits, so

`p ≈ (1 − e^{−kn/m})^k`.

For fixed `m/n` this is minimized at `k = (m/n)·ln 2`, which makes exactly half of the bits set, giving `p = 2^{−k}`. Inverting for a target rate `p`:

- bits per key `m/n = −ln p / (ln 2)²  ≈ 1.44·log2(1/p)`
- hash count `k = log2(1/p)`

Each factor of 10 in accuracy costs 4.79 more bits per key. The information-theoretic minimum for a lossy set at rate `p` is `log2(1/p)` bits per key, so a Bloom filter is a factor 1.44 above optimal — the price of being simple and mergeable (union of two filters is a bitwise OR).

{{include dsa/probabilistic/bloom.c}}

{{run dsa/probabilistic :: gcc -std=gnu11 -O2 -Wall -Wextra bloom.c -o bloom && ./bloom}}

The measured false-positive rates match the formula to within the sampling noise of 2 million probes, and with `k` chosen optimally about half the bits are set, as predicted. The sizing table shows why people use them: a filter with a 1 % false-positive rate needs about 9.6 bits per key *regardless of key size* — whereas a hash set of 64-bit keys needs at least 64 bits, and a set of URLs or database rows far more.

**Where it earns its keep.** LSM-tree databases (LevelDB, RocksDB, Cassandra, HBase) keep a Bloom filter per on-disk table: a lookup for an absent key consults the in-memory filters and skips almost every disk read. CDNs use them to avoid caching "one-hit wonders" (cache an object only on its second request). Browsers and mail systems have used them as a fast pre-filter for blocklists, falling back to an exact check on a hit.

**Limits and variants.**

- No deletion: clearing a bit may remove other keys. A **counting Bloom filter** uses 4-bit counters instead of bits (4× the space) to support delete. **Cuckoo filters** store short fingerprints in a cuckoo hash table, support deletion, and beat Bloom filters below roughly 3 % false-positive rates.
- The filter must be **sized in advance**; if `n` exceeds the design capacity, `p` degrades quickly (the formula above with the real `n`). Scalable variants chain filters.
- Access is `k` random memory reads — `k` cache misses for a large filter. *Blocked* Bloom filters confine all `k` bits to one 64-byte cache line at the cost of a slightly higher false-positive rate.

## 3. Count-Min sketch: frequencies in sublinear space

A Count-Min sketch (CMS) is a `d × w` array of counters with one hash function per row. **Update(x)** increments `cell[r][h_r(x)]` in every row. **Estimate(x)** returns the **minimum** over the rows. Collisions only ever *add* to a counter, so every cell is ≥ the true count and the minimum is the least-polluted one: the estimate **never underestimates**.

**The guarantee.** With `w = ⌈e/ε⌉` and `d = ⌈ln(1/δ)⌉`, for a stream of total weight `N` the estimate satisfies

`true ≤ estimate ≤ true + ε·N` with probability at least `1 − δ`.

(Proof sketch: in one row the expected pollution of a cell is at most `N/w`; Markov's inequality gives pollution `> e·N/w` with probability `< 1/e`; the `d` rows are independent so all fail with probability `< e^{−d}`.)

The key subtlety is that the bound is **additive in N**, not relative to the item's own count. It is a heavy-hitter tool: items whose frequency is well above `εN` are estimated with small relative error, while rare items can be wildly overestimated. The experiment makes that concrete with a Zipf-distributed stream (the realistic shape of word frequencies, flow sizes, and page views).

{{include dsa/probabilistic/sketches.c 1-35}}

{{run dsa/probabilistic :: gcc -std=gnu11 -O2 -Wall -Wextra sketches.c -o sketches -lm && ./sketches}}

The first block is the Count-Min result. Observe:

1. **Zero underestimates** out of 100,000 items — the one-sided guarantee holds exactly, not just probabilistically.
2. The top items (the heavy hitters) are accurate to well under 1 %, from 40 KB of counters for a 5-million-event stream.
3. For the rare items the *relative* error is enormous (the item seen 6 times is reported as hundreds). The *absolute* error stays within the `e·N/w` bound printed on the last line — which is why the bound on its own tells you nothing about rare items. This is not a bug; it is the contract. Use CMS to find and rank heavy hitters (with a small heap alongside to track the top-k), never to count the long tail.

Two practical refinements: **conservative update** (increment only the cells equal to the current minimum) greatly reduces overestimation with the same memory; and sketches are **mergeable** — cell-wise addition of two sketches built with the same hash functions is the sketch of the combined stream, which is what makes them suitable for distributed aggregation.

## 4. HyperLogLog: counting distinct items in 16 KB

Counting distinct elements exactly needs memory proportional to the answer. HyperLogLog (HLL) estimates it with a relative standard error of `1.04/√m` using `m` tiny registers.

**The idea.** Hash each item to a uniform bit string. The probability that a hash starts with `j−1` zeros followed by a 1 is `2^{−j}`; so if the longest run of leading zeros seen is `ρ`, there are likely about `2^ρ` distinct items. A single such estimate has huge variance, so HLL splits the stream into `m = 2^p` sub-streams using the first `p` hash bits, keeps the maximum `ρ` per sub-stream in a register (one byte is plenty — `ρ ≤ 64`), and combines them with a **harmonic mean**, which damps the outliers from lucky large `ρ` values:

`E = α_m · m² / Σ 2^{−M[j]}`, with `α_m ≈ 0.7213/(1 + 1.079/m)`.

For small cardinalities (`E ≤ 2.5m`) with some empty registers, HLL switches to *linear counting*, `m·ln(m/V)` where `V` is the number of empty registers; this is the correction in the code above. Duplicates never change the state (the max is idempotent), and **union is the register-wise max**, so sketches from many machines merge exactly.

The second block of the output above uses `p = 14`: 16,384 registers, 16 KB, theoretical standard error `1.04/√16384 = 0.81 %`. The errors measured at cardinalities from 100 to 10 million are all within about one standard deviation of that, while the same structure would need ~80 MB as an exact set of 64-bit hashes at 10 million elements. The merge line shows two overlapping sets combining into an estimate of their union without any access to the originals. Production systems (Redis `PFADD`/`PFCOUNT`, BigQuery `APPROX_COUNT_DISTINCT`, Presto, Druid) use exactly this structure, usually with the bias corrections of HLL++ for mid-range cardinalities.

A caution on trust: the guarantee is on the *standard error* — an individual estimate can be about two standard errors (~1.6 % here) off. Since the single-run table above is one sample, the honest reading is "consistent with 0.8 %", not "always under 1 %".

## 5. Consistent hashing: sharding without a global reshuffle

You have `N` cache or storage servers and must decide which one holds a key. The obvious rule `server = hash(key) mod N` is perfectly balanced — and catastrophic when `N` changes, because almost every key maps somewhere new (a cache stampede on the database, or a mass data migration).

**Consistent hashing** (Karger et al., 1997) places both servers and keys on a circular hash space; a key belongs to the first server point clockwise from it. Adding or removing a server only moves the keys in the arc it takes over: an expected `1/N` of them. A lookup is a binary search over the sorted ring points.

One point per server gives terrible balance (arc lengths of random points vary by a factor of ~log N). The fix is **virtual nodes**: each server owns many points, which averages the arc lengths. Load imbalance falls like `1/√(points per server)`.

{{include dsa/probabilistic/consistent.c}}

{{run dsa/probabilistic :: gcc -std=gnu11 -O2 -Wall -Wextra consistent.c -o consistent -lm && ./consistent}}

Two results to take away:

- **Balance**: with a single point per node, the busiest node carries over twice its fair share and the lightest only a quarter. Every ×10 in virtual nodes improves the spread roughly by √10; at 1000 points per node the busiest node is within a few percent of ideal. The price is ring size (`nodes × points` entries, 16 bytes each here) and a slower rebuild.
- **Movement**: adding the 11th node with `hash mod N` relocates about 91 % of the keys (a key stays only if `h mod 10 = h mod 11`, which happens about 1 time in 11). The ring moves about 9 %, close to the ideal `1/11`, and *only to the new node*.

Real systems: Dynamo-style stores (Cassandra, Riak), memcached client libraries (ketama), and CDN request routing use it. Variants address its weaknesses — **rendezvous (highest-random-weight) hashing** needs no ring and gives perfect balance for small `N` at O(N) cost per lookup; **jump consistent hash** (Lamping & Veach) is a 5-line, memory-free function when buckets are numbered 0..N−1 and only grow or shrink at the end; **bounded-load** consistent hashing caps any node at `(1+ε)×` the average.

## 6. Choosing among them

| Question | Structure | Memory | Error |
|---|---|---|---|
| Have I seen x? (set membership) | Bloom / Cuckoo filter | ~1.44·log2(1/p) bits per key | false positives at rate p, never false negatives |
| How often did x occur? | Count-Min sketch | `w·d` counters | overestimate ≤ ε·N with prob. 1−δ |
| How many distinct items? | HyperLogLog | `m` bytes | std. error 1.04/√m |
| Which node owns x, resilient to resizing? | Consistent / rendezvous hashing | ring of `nodes × vnodes` | none (balance only) |
| Top-k heavy hitters? | Count-Min + heap, or Misra–Gries / Space-Saving | O(k) to O(1/ε) | bounded undercount/overcount |

## Pitfalls

- **Weak hash, broken guarantees.** Low-bit use of a multiplicative hash, correlated seeds (e.g. `h(x) + i`), or a hash that ignores part of the key will make measured error far worse than the formulas. Always run an experiment like the ones above on your real data.
- **Sizing from the wrong `n`.** Bloom filters and CMS accuracy depend on the number of items actually inserted; plan for peak, not average.
- **Treating "possibly present" as "present".** A Bloom hit must lead to the authoritative lookup; the filter only removes work on the negative path.
- **Serializing sketches.** To merge or persist sketches across processes, the hash function, seed and layout must be identical and versioned — otherwise the merge silently produces garbage.
- **Adversarial inputs.** Everything here assumes keys are not chosen by an attacker who knows your hash; use a keyed hash (SipHash) if they might be.

## Key takeaways

1. These structures keep a lossy summary of hashes and prove an error bound; the value is memory (bits per item instead of bytes) and mergeability.
2. A Bloom filter needs `1.44·log2(1/p)` bits per key and `k = log2(1/p)` hashes — about 10 bits and 7 hashes for 1 %. Measured rates match the formula; there are no false negatives and no deletes.
3. Count-Min never underestimates, and its error is additive in the *stream size*, so it is for heavy hitters, not the long tail.
4. HyperLogLog estimates distinct counts with 1.04/√m relative error from `m` bytes and merges by register-wise max.
5. `hash mod N` moves almost all keys when `N` changes; consistent hashing moves ~1/N, and virtual nodes buy balance at the cost of ring size.
6. Hash quality is the foundation: test avalanche, use high bits of multiplicative hashes, and use a keyed hash against adversaries.
