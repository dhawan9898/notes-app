---
title: "Caches: LRU, LFU, CLOCK and Belady's OPT"
track: dsa
module: Systems-Flavored Structures
moduleOrder: 9
order: 1
level: advanced
summary: "Replacement policies simulated on Zipf, scan-polluted and looping workloads against the offline optimum, plus an O(1) hash-plus-list LRU cache with a pooled allocator, invariant checks and a throughput measurement."
sources:
  - label: Belady — A Study of Replacement Algorithms for a Virtual-Storage Computer (IBM Systems Journal 5(2), 1966)
  - label: Corbató — A Paging Experiment with the Multics System (1968) (origin of the CLOCK algorithm)
  - label: Johnson & Shasha — 2Q, A Low Overhead High Performance Buffer Management Replacement Algorithm (VLDB 1994)
  - label: Megiddo & Modha — ARC, A Self-Tuning, Low Overhead Replacement Cache (FAST 2003)
  - label: Einziger, Friedman, Manes — TinyLFU, A Highly Efficient Cache Admission Policy (ACM Trans. Storage 13(4), 2017)
  - label: Sleator & Tarjan — Amortized Efficiency of List Update and Paging Rules (CACM 28(2), 1985)
  - label: Linux kernel documentation — Multi-Gen LRU
    url: https://www.kernel.org/doc/html/latest/admin-guide/mm/multigen_lru.html
  - label: Redis documentation — Key eviction
    url: https://redis.io/docs/latest/develop/reference/eviction/
---

A **cache** keeps a small, fast copy of a larger, slower store. When it is full, a **replacement policy** decides which entry to evict. The policy is a bet about the future, and the entire field is the question: *what does the past tell us about the next request?* Hardware caches, the Linux page cache, database buffer pools, CDNs, Redis and memoization tables all face the same decision.

## The policies

| Policy | Evicts | Idea | Cost per hit |
|---|---|---|---|
| FIFO | oldest insertion | trivial | O(1), no update |
| **LRU** | least recently used | recency predicts reuse (temporal locality) | move node to the front |
| **LFU** | least frequently used | popularity predicts reuse | update count / heap |
| **CLOCK** (second chance) | first frame whose reference bit is 0 | LRU approximation | set one bit — cheap and lock-friendly |
| Random | a random entry | surprisingly competitive; no metadata | none |
| 2Q / ARC / LIRS / TinyLFU | combinations of the above | recency *and* frequency, scan resistance, adaptive sizing | more metadata |
| **OPT** (Belady) | key used farthest in the future | needs the future — offline upper bound | n/a |

**Belady's OPT** is the optimal replacement policy and the yardstick: no online policy can beat it, and the gap to OPT is the room for improvement. **CLOCK**, from the Multics paging experiments, keeps frames in a ring with a reference bit; a hit merely sets the bit, and on a miss the "hand" sweeps forward, clearing bits and evicting the first frame whose bit is already clear. It approximates LRU with no list manipulation on the hit path, which matters when many threads hit concurrently — it is the basis of page-replacement in most operating systems.

## Simulating them

All the policies are implemented over a trace of integer keys and compared on three workloads, with OPT as the ceiling. The code is the simulator; each policy returns the per-request hit flags so a workload can report only the requests that matter.

{{include dsa/caches/policies.h}}

{{include dsa/caches/sim.c}}

{{run dsa/caches :: gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined sim.c -o sim_asan -lm && ./sim_asan}}

Reading the results:

**Sanity check — Belady's anomaly.** On the trace `1 2 3 4 1 2 5 1 2 3 4 5` FIFO with capacity 3 gets 3 hits, but with capacity **4 it gets only 2**: more memory, fewer hits. FIFO is not a *stack algorithm*; LRU and OPT are, and for them a bigger cache never hurts. (The simulator also asserts, on every row, that no policy beats OPT.)

**1. Zipf workload.** Real request streams are heavily skewed — a few keys account for most traffic — and a Zipf distribution models that. Here:
- All policies improve steadily with size; even a cache holding 1 % of the keys catches half or more of the requests.
- **LFU is best of the online policies** (up to 12 points above LRU at the smallest sizes, narrowing to about 3 when the cache holds 20 % of the keys), which matches theory: when requests are independent draws from a fixed distribution, keeping the most popular keys is optimal.
- **LRU and CLOCK are within about a point of each other** — CLOCK buys LRU-like behavior with a bit flip — and both beat FIFO clearly, because FIFO evicts popular keys on schedule even when they are being used.
- OPT is another 6–8 points above LFU: the cost of not knowing the future.

**2. Scan pollution.** A single sequential pass over a large data set (a backup, a full table scan, `cat bigfile`) touches many keys exactly once. LRU treats each as "recently used", so the scan **flushes the genuinely hot entries** out of the cache. The second table reports the hit ratio over the Zipf (useful) requests only, while 30 % of all requests are a never-reused sweep: LRU's hot-request hit ratio falls well below the 50.5 % it reached without the sweep (at 1000 entries it is about 45.6 %), FIFO and CLOCK degrade similarly, and **LFU is nearly unaffected** (59.2 % vs 59.4 %) because sweep keys have frequency one and are evicted first. This is why production systems do not use plain LRU:

- The Linux page cache keeps **active** and **inactive** lists so a page must be touched twice to be promoted (a 2Q-like design), and since kernel 6.1 offers the multi-generational LRU (MGLRU), which tracks several generations of pages to estimate working sets (see *Virtual Memory, Paging and the Page Cache*).
- 2Q, ARC, LIRS and TinyLFU add admission control or ghost lists to resist scans while still adapting to recency.
- Databases tag sequential scans so they use a small ring of buffers instead of the whole pool (PostgreSQL does this for large sequential scans).

**3. The loop.** A cyclic access pattern over 1,100 keys with a 1,000-entry cache is LRU's, FIFO's and CLOCK's worst case: each key is evicted just before it is needed again, so the hit ratio is **0 %** — and LFU also fails here. OPT achieves 90.9 % by keeping most of the loop and sacrificing a few keys. With a cache of 1,100 entries every policy reaches 99.9 % (only the first lap misses). A cache that is slightly too small can be worth *nothing* — a cliff, not a slope — which is why capacity planning on working-set size matters more than policy choice.

## An O(1) LRU cache

LRU's textbook implementation combines two structures: a **hash table** for O(1) lookup and a **doubly linked list** ordered by recency for O(1) "move to front" and "evict the tail". This implementation allocates all entries once from a fixed pool (no `malloc`/`free` on the hot path), recycles evicted entries through a free list, and keeps the list and hash table consistent — an invariant checker verifies after tens of thousands of operations that they hold exactly the same set of entries and that the list's back-pointers are right. A differential test requires its hit counts to equal the array-based simulator's on the same trace.

{{include dsa/caches/lru_cache.c}}

{{run dsa/caches :: gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined lru_cache.c -o lru_asan && ./lru_asan | head -3 && gcc -std=gnu11 -O2 -Wall -Wextra lru_cache.c -o lru_cache && ./lru_cache | tail -1}}

The hit counts match the simulator exactly at three capacities; the optimized build handles on the order of 10–20 million requests per second on one thread at about 60 bytes of metadata per entry. Concurrency is the practical problem: a single LRU list is a global lock. Production caches **shard** the keys over many independent LRUs, or move the recency update off the hit path (CLOCK bits, or buffered/batched promotions as in Caffeine and Linux's per-CPU batches).

## Production considerations

- **TTL / expiry**: store a deadline per entry and check on access (lazy), plus a background sweep or timing wheel for proactive removal; expiry and eviction are separate mechanisms.
- **Approximate LRU**: Redis samples a few random keys and evicts the best candidate among them, avoiding a global list entirely (`allkeys-lru`; `allkeys-lfu` uses a probabilistic frequency counter with decay).
- **Admission vs eviction**: TinyLFU compares the candidate's estimated frequency (a Count-Min sketch, see *Probabilistic Structures*) with the victim's and refuses to admit rarer keys — scan resistance without ghost lists.
- **Write policies**: write-through, write-back and write-around decide when the backing store sees updates; **cache stampede** (many misses for one hot key at expiry) is handled by request coalescing or probabilistic early refresh.
- **Size by working set**: measure the hit-ratio-versus-size curve (the "miss ratio curve") on a real trace; the cliff in workload 3 is why. Simulation with sampled traces (SHARDS) makes it cheap.
- **CPU caches** use the same ideas in hardware: set-associative placement with pseudo-LRU (tree bits) because exact LRU for 8 or 16 ways is too costly; the cost of a miss is what makes cache-aware data structures (B-trees, d-ary heaps, CSR graphs earlier in this track) win.

## Pitfalls

- **Measuring on the wrong trace.** Policies rank differently on different workloads; synthetic Zipf is a starting point, not a verdict. Replay real traces.
- **Unbounded metadata**: LFU with a lazy-deletion heap (used here) must be compacted periodically, and ARC/LIRS ghost lists consume memory not counted in the cache size.
- **Counting hits on all requests when part of the stream is one-time** (scans) hides the pollution damage; report the hit ratio of the requests that could have hit.
- **Cache stampede and thundering herd** at expiry or on a cold start.
- **Memory accounting**: an entry's real cost includes key, value, hash-table slots and allocator overhead; "capacity 100,000" is not "100,000 × payload size".
- **Concurrency hidden behind "O(1)"**: a global lock around the list makes a thread-safe LRU slower than a single thread.
- **Stale data**: caching correctness lives in invalidation, not in eviction.

## Key takeaways

1. Replacement is a prediction problem; Belady's OPT, which sees the future, bounds what any policy can achieve.
2. For skewed, independent requests LFU wins; LRU and CLOCK are close to each other and clearly better than FIFO; CLOCK is nearly free on the hit path.
3. Plain LRU is vulnerable to scans and to loops slightly larger than the cache (hit ratio collapses to zero); real systems add admission control or multiple lists (2Q, ARC, MGLRU, TinyLFU).
4. An O(1) LRU is a hash table plus an intrusive doubly linked list over a pre-allocated pool; test it with an invariant checker and a differential simulator.
5. Sizing the cache to the working set matters more than the policy — hit ratio versus size can be a cliff.
6. Evaluate on realistic traces and report hits on requests that could hit.
