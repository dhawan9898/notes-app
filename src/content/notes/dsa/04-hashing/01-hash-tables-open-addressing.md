---
title: Hash Tables with Open Addressing
track: dsa
module: Hashing
moduleOrder: 4
order: 1
level: intermediate
summary: Design a production-quality hash table in C — probing, tombstones, load factor, resizing and the real cost model.
prerequisites:
  - Dynamic Arrays
sources:
  - label: "Knuth, The Art of Computer Programming, Vol. 3 — §6.4 Hashing"
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, Ch. 11 Hash Tables"
  - label: "Celis, Larson, Munro — Robin Hood Hashing (FOCS 1985)"
  - label: "Fowler–Noll–Vo hash function"
    url: http://www.isthe.com/chongo/tech/comp/fnv/
---

A hash table maps keys to values in **expected O(1)** time by turning a key into an array index. Two decisions define a design: the **hash function** and the **collision strategy**. This note builds *open addressing*, where all entries live in one flat array and collisions are resolved by probing, which is the cache-friendly choice and the one used by most high-performance tables.

## Chaining vs. open addressing

| | Separate chaining | Open addressing |
|---|---|---|
| Storage | array of list heads + heap nodes | single array of slots |
| Cache behavior | pointer chasing, poor | sequential probes, good |
| Memory overhead | pointer per entry + allocator overhead | empty slots (keep load ≤ 0.5–0.8) |
| Load factor α | can exceed 1 | must stay < 1 |
| Delete | trivial | needs tombstones or backward shift |
| Worst case | long chain | long probe cluster |

## Cost model

With load factor `α = n / capacity` and a good hash, the expected number of probes under **linear probing** is (Knuth, §6.4):

- successful search: `½ · (1 + 1/(1−α))`
- unsuccessful search / insert: `½ · (1 + 1/(1−α)²)`

| α | hit | miss |
|---|---|---|
| 0.50 | 1.5 | 2.5 |
| 0.75 | 2.5 | 8.5 |
| 0.90 | 5.5 | 50.5 |

The miss cost explodes as α → 1, which is why implementations resize around **0.5–0.75**. Linear probing suffers *primary clustering* (runs of occupied slots grow and merge), but each probe is an adjacent memory read, so in practice it beats quadratic probing or double hashing until the table is fairly full.

## Choosing a hash function

Requirements: deterministic, fast, and **uniform in the low bits** if the capacity is a power of two (we mask rather than divide). Identity hashes on integers, or the pointer value alone, put aligned keys in the same few buckets. Use an avalanche step. For strings, FNV-1a is the simplest decent choice; for adversarial input (network-facing code) use a keyed hash such as SipHash to avoid hash-flooding denial of service.

```c
static inline uint64_t mix64(uint64_t x) {      // SplitMix64 finalizer
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31; return x;
}
```

## Implementation

Keys and values are `uint64_t`. Each slot has a state, because *empty* and *deleted* must be distinguished.

```c
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

enum { EMPTY = 0, FULL = 1, TOMB = 2 };

typedef struct { uint64_t key, val; uint8_t state; } slot_t;
typedef struct { slot_t *s; size_t cap, used, tombs; } map_t;   // cap: power of two

static inline uint64_t mix64(uint64_t x) {
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31; return x;
}

bool map_init(map_t *m, size_t cap) {          // cap must be a power of two
    m->s = calloc(cap, sizeof *m->s);
    m->cap = cap; m->used = m->tombs = 0;
    return m->s != NULL;
}

static bool grow(map_t *m, size_t newcap);

bool map_put(map_t *m, uint64_t k, uint64_t v) {
    if ((m->used + m->tombs + 1) * 4 > m->cap * 3 &&           // load incl. tombstones > 0.75
        !grow(m, m->used * 2 >= m->cap ? m->cap * 2 : m->cap)) // rehash in place if mostly tombstones
        return false;
    size_t mask = m->cap - 1, i = mix64(k) & mask, first_tomb = (size_t)-1;
    for (;; i = (i + 1) & mask) {
        slot_t *p = &m->s[i];
        if (p->state == FULL && p->key == k) { p->val = v; return true; }   // update
        if (p->state == TOMB && first_tomb == (size_t)-1) first_tomb = i;
        if (p->state == EMPTY) {                                            // not present
            if (first_tomb != (size_t)-1) { p = &m->s[first_tomb]; m->tombs--; }
            *p = (slot_t){ k, v, FULL }; m->used++;
            return true;
        }
    }
}

bool map_get(const map_t *m, uint64_t k, uint64_t *out) {
    size_t mask = m->cap - 1;
    for (size_t i = mix64(k) & mask; m->s[i].state != EMPTY; i = (i + 1) & mask)
        if (m->s[i].state == FULL && m->s[i].key == k) { *out = m->s[i].val; return true; }
    return false;
}

bool map_del(map_t *m, uint64_t k) {
    size_t mask = m->cap - 1;
    for (size_t i = mix64(k) & mask; m->s[i].state != EMPTY; i = (i + 1) & mask)
        if (m->s[i].state == FULL && m->s[i].key == k) {
            m->s[i].state = TOMB; m->used--; m->tombs++; return true;
        }
    return false;
}

static bool grow(map_t *m, size_t newcap) {
    map_t n;
    if (!map_init(&n, newcap)) return false;
    for (size_t i = 0; i < m->cap; i++)
        if (m->s[i].state == FULL) map_put(&n, m->s[i].key, m->s[i].val);
    free(m->s); *m = n; return true;
}
```

### Why each detail matters

- **Power-of-two capacity** lets `& mask` replace `% cap` (a division costs tens of cycles). It requires the good mixing step above.
- **The probe loop must terminate.** It does only because at least one slot is `EMPTY`. The load check counts *tombstones too*, otherwise a table full of tombstones makes lookups of absent keys loop forever.
- **Deletion cannot just clear the slot**: that would cut the probe chain for keys inserted *after* a colliding entry. Hence tombstones. `map_get` skips them, `map_put` reuses the first one it saw once the key is proven absent.
- **Resizing** costs O(n) but happens at doubling intervals, so insertion is **amortized O(1)**. The rehash also discards tombstones. When most of the load is tombstones, rehashing at the *same* capacity reclaims space (the `m->used * 2 >= m->cap` check).
- **Pointer invalidation**: any `put` may reallocate, so never hold a pointer into the table across inserts.

## Tombstone-free alternatives

- **Backward-shift deletion** (linear probing): after removing slot *i*, scan forward and shift entries whose home bucket lies at or before *i* back by one, until an empty slot. No tombstones, and lookups stay short.
- **Robin Hood hashing**: on insert, an entry that has probed farther than the resident entry *steals* its slot and the displaced one continues. This equalizes probe lengths (low variance), allows early termination of misses, and pairs naturally with backward-shift deletion.
- **Cuckoo hashing**: two (or more) hash functions, worst-case O(1) lookup with at most two probes; inserts may trigger relocation chains.
- **Swiss tables / SIMD probing** (Abseil, Rust's `hashbrown`): store a byte of metadata per slot, compare 16 at a time with SSE2.

## Practical guidance

- Pre-size when you know `n`: `cap = next_pow2(n * 4 / 3 + 1)` avoids repeated rehashing.
- Choose string-key tables so that the hash is computed once; store it in the slot to avoid recomputation during resize and to speed up mismatches.
- Iteration order is arbitrary and changes on resize; never depend on it.
- Test with an adversarial and a degenerate workload (sequential keys, keys that are multiples of 1024, delete-heavy churn) — not just random data.

## Key takeaways

- Open addressing = one flat array + probing: simple and fast, but α must stay well below 1.
- Expected probe counts depend on α; keep it ≤ 0.75 (≤ 0.5 for linear probing under heavy misses).
- Deletion needs tombstones or backward-shift, and tombstones count toward load.
- Power-of-two sizing needs a hash with good low-bit mixing.
- Amortized O(1) insertion comes from geometric resizing.
