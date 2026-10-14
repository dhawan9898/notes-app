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
  - "Hash Tables for Beginners: Fast Lookup by Key"
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

```c
// Open-addressing hash map, linear probing, power-of-two capacity, backward-shift deletion (no tombstones).
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct { uint64_t key, val; bool full; } slot_t;
typedef struct { slot_t *s; size_t cap, n; unsigned long probes; } map_t;   // probes: statistics only

static inline uint64_t mix64(uint64_t x) {          // SplitMix64 finalizer: good avalanche in the LOW bits
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31; return x;
}

static bool map_init(map_t *m, size_t cap) {         // cap: power of two
    m->s = calloc(cap, sizeof *m->s); m->cap = cap; m->n = 0; m->probes = 0;
    return m->s != NULL;
}
static void map_free(map_t *m) { free(m->s); m->s = NULL; }

static bool map_get(map_t *m, uint64_t k, uint64_t *out) {
    size_t mask = m->cap - 1;
    for (size_t i = mix64(k) & mask;; i = (i + 1) & mask) {
        m->probes++;
        if (!m->s[i].full) return false;             // an empty slot ends the probe chain
        if (m->s[i].key == k) { if (out) *out = m->s[i].val; return true; }
    }
}

static bool map_grow(map_t *m);

static bool map_put_nogrow(map_t *m, uint64_t k, uint64_t v) {
    size_t mask = m->cap - 1;
    for (size_t i = mix64(k) & mask;; i = (i + 1) & mask) {
        if (!m->s[i].full) { m->s[i] = (slot_t){k, v, true}; m->n++; return true; }
        if (m->s[i].key == k) { m->s[i].val = v; return true; }
    }
}

static bool map_put(map_t *m, uint64_t k, uint64_t v) {
    if ((m->n + 1) * 4 > m->cap * 3 && !map_grow(m)) return false;    // keep load factor <= 0.75
    return map_put_nogrow(m, k, v);
}

static bool map_grow(map_t *m) {
    map_t n;
    if (!map_init(&n, m->cap * 2)) return false;
    for (size_t i = 0; i < m->cap; i++) if (m->s[i].full) map_put_nogrow(&n, m->s[i].key, m->s[i].val);
    free(m->s); n.probes = m->probes; *m = n; return true;
}

// Backward-shift deletion: after emptying slot i, pull later members of the same probe run back
// into the hole whenever that does not move them in front of their home slot.
static bool map_del(map_t *m, uint64_t k) {
    size_t mask = m->cap - 1, i = mix64(k) & mask;
    while (m->s[i].full && m->s[i].key != k) i = (i + 1) & mask;
    if (!m->s[i].full) return false;
    size_t hole = i;
    for (size_t j = (i + 1) & mask; m->s[j].full; j = (j + 1) & mask) {
        size_t home = mix64(m->s[j].key) & mask;
        // can slot j move into the hole? only if its home is NOT cyclically in (hole, j]
        bool in_between = hole <= j ? (hole < home && home <= j) : (hole < home || home <= j);
        if (!in_between) { m->s[hole] = m->s[j]; hole = j; }
    }
    m->s[hole].full = false; m->n--;
    return true;
}
```

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

```c
// Randomized differential test of oamap.h against a trivial array oracle, with heavy deletion churn.
#include "oamap.h"
#include <stdio.h>
#include <string.h>

#define KEYS 512            // small key space => many collisions, re-insertions and deletions
static uint64_t rng = 88172645463325252ULL;
static uint64_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

int main(void) {
    bool present[KEYS] = {0}; uint64_t value[KEYS] = {0};
    map_t m; map_init(&m, 8);
    size_t ops = 2000000, count = 0;
    for (size_t t = 0; t < ops; t++) {
        uint64_t k = rnd() % KEYS * 977 + 3;     // spread keys; index back via k
        size_t idx = (k - 3) / 977;
        switch (rnd() % 3) {
        case 0: { uint64_t v = rnd(); map_put(&m, k, v); if (!present[idx]) count++; present[idx] = true; value[idx] = v; break; }
        case 1: { bool r = map_del(&m, k); if (r != present[idx]) { printf("delete mismatch\n"); return 1; } if (present[idx]) count--; present[idx] = false; break; }
        default: { uint64_t v = 0; bool r = map_get(&m, k, &v);
                   if (r != present[idx] || (r && v != value[idx])) { printf("get mismatch\n"); return 1; } }
        }
        if (m.n != count) { printf("size mismatch\n"); return 1; }
    }
    // final audit: every present key findable, no extras
    for (size_t i = 0; i < KEYS; i++) {
        uint64_t v; bool r = map_get(&m, i * 977 + 3, &v);
        if (r != present[i] || (r && v != value[i])) { printf("final mismatch\n"); return 1; }
    }
    printf("%zu random operations match the oracle; final size %zu, capacity %zu\n", ops, m.n, m.cap);
    map_free(&m);
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra -Wno-unused-function -fsanitize=address,undefined test.c -o t && ./t
2000000 random operations match the oracle; final size 270, capacity 512
```

### The cost model, measured

With load factor `α = n / capacity` and a good hash, Knuth (§6.4) gives the expected probes under linear probing:

- successful search: `½ · (1 + 1/(1−α))`
- unsuccessful search / insert: `½ · (1 + 1/(1−α)²)`

Measuring them with 1M slots and keys inserted at each load:

```c
// Measure average probes per lookup at fixed load factors and compare with Knuth's linear-probing formulas.
#include "oamap.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    const size_t cap = 1 << 20;
    printf("%6s %12s %12s %12s %12s\n", "load", "hit(meas)", "hit(Knuth)", "miss(meas)", "miss(Knuth)");
    for (double a = 0.30; a < 0.96; a += 0.15) {
        map_t m; map_init(&m, cap);
        size_t n = (size_t)(a * cap);
        for (size_t i = 0; i < n; i++) map_put_nogrow(&m, i * 2654435761u + 1, i);      // keys 1, ...
        m.probes = 0;
        for (size_t i = 0; i < n; i++) map_get(&m, i * 2654435761u + 1, NULL);
        double hit = (double)m.probes / n;
        m.probes = 0;
        for (size_t i = 0; i < n; i++) map_get(&m, (i + (1ull << 40)) * 2654435761u + 1, NULL);   // absent keys
        double miss = (double)m.probes / n;
        printf("%6.2f %12.2f %12.2f %12.2f %12.2f\n", a, hit, 0.5 * (1 + 1 / (1 - a)), miss, 0.5 * (1 + 1 / ((1 - a) * (1 - a))));
        map_free(&m);
    }
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra -Wno-unused-function probes.c -o p -lm && ./p
  load    hit(meas)   hit(Knuth)   miss(meas)  miss(Knuth)
  0.30         1.21         1.21         1.52         1.52
  0.45         1.41         1.41         2.15         2.15
  0.60         1.75         1.75         3.62         3.62
  0.75         2.50         2.50         8.52         8.50
  0.90         5.47         5.50        49.12        50.50
```

The measured and predicted columns agree to two digits. Read them as the design rule: lookups of present keys stay cheap until α ≈ 0.75, but **misses cost 49 probes at α = 0.9**, which is why implementations resize at 0.5–0.75. Linear probing suffers *primary clustering* (runs of occupied slots grow and merge), but each probe reads an adjacent address, so in practice it beats quadratic probing or double hashing until the table is fairly full.

## Variations worth knowing

- **Robin Hood hashing**: on insert, an entry that has probed farther than the resident *steals* its slot and the displaced one continues. This equalizes probe lengths (low variance), lets a miss stop early (once the probe distance exceeds the resident's), and pairs with backward-shift deletion.
- **Swiss tables / SIMD probing** (Abseil, Rust's `hashbrown`): one byte of metadata per slot, 16 compared at once with SSE2, so a probe group is a single load and compare.
- **Quadratic probing / double hashing**: avoid primary clustering but jump around memory; usually slower than linear probing with a good hash.
- **Storing the hash** in each slot speeds resizing and rejects mismatches without comparing long keys.

## Cuckoo hashing: worst-case O(1) lookups

Open addressing has no worst-case bound on lookup. **Cuckoo hashing** uses two tables and two hash functions; every key lives in exactly one of two possible slots, so a lookup is **at most two probes, always**. Insert puts the key in its slot in table 0; if occupied, the resident is *kicked out* to its slot in table 1, which may kick another back, and so on. A long chain means a cycle, so the table is rebuilt with new hash seeds. Theory (Pagh & Rodler) says two tables work up to about 50% load with expected O(1) insert; more tables or buckets of several slots raise the usable load to 90% and above.

```c
// Cuckoo hashing: two tables, two hash functions; every key lives in one of exactly two slots, so a lookup is at most 2 probes.
// Insert kicks the resident out of its slot into its alternate slot, possibly repeatedly; a chain that is too long means a
// cycle, and the whole table is rebuilt with fresh hash seeds. Two-table cuckoo works up to about 50% load.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct { uint64_t *t[2]; bool *used[2]; size_t half; uint64_t seed[2]; size_t kicks; } cuckoo_t;

static uint64_t mix64(uint64_t x) { x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL; x ^= x >> 27; x *= 0x94d049bb133111ebULL; x ^= x >> 31; return x; }
static size_t slot(const cuckoo_t *c, int w, uint64_t k) { return mix64(k ^ c->seed[w]) % c->half; }

static uint64_t rng = 0x9e3779b97f4a7c15ULL;
static uint64_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

static void reset(cuckoo_t *c) {                         // empty tables, fresh seeds
    for (int w = 0; w < 2; w++) { free(c->t[w]); free(c->used[w]); c->t[w] = calloc(c->half, 8); c->used[w] = calloc(c->half, 1); c->seed[w] = rnd(); }
}
static bool lookup(const cuckoo_t *c, uint64_t k) {
    for (int w = 0; w < 2; w++) { size_t i = slot(c, w, k); if (c->used[w][i] && c->t[w][i] == k) return true; }
    return false;
}
static bool insert(cuckoo_t *c, uint64_t k) {            // false: kick chain too long (cycle); a key is left homeless
    int w = 0;
    for (int kick = 0; kick < 64; kick++) {
        size_t i = slot(c, w, k);
        if (!c->used[w][i]) { c->used[w][i] = true; c->t[w][i] = k; return true; }
        uint64_t evicted = c->t[w][i]; c->t[w][i] = k; k = evicted; w ^= 1; c->kicks++;
    }
    return false;
}

int main(void) {
    printf("%9s %9s %6s %10s %14s\n", "slots", "keys", "load", "rebuilds", "kicks/insert");
    for (int pct = 20; pct <= 48; pct += 7) {
        cuckoo_t c = { .half = 1 << 16 };
        size_t want = (size_t)(pct / 100.0 * 2 * c.half), rebuilds = 0, inserts = 0;
        uint64_t *keys = malloc(want * 8);
        for (size_t i = 0; i < want; i++) keys[i] = i * 11400714819323198485ULL + 1;
        reset(&c);
        for (size_t i = 0; i < want; i++) {
            inserts++;
            if (!insert(&c, keys[i])) { rebuilds++; reset(&c); i = (size_t)-1; }   // restart with new seeds
        }
        bool all = true;
        for (size_t i = 0; i < want; i++) if (!lookup(&c, keys[i])) all = false;
        printf("%9zu %9zu %5d%% %10zu %14.2f %s\n", 2 * c.half, want, pct, rebuilds, (double)c.kicks / inserts, all ? "" : "LOOKUP FAILED");
        free(keys);
    }
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra cuckoo.c -o cuckoo && ./cuckoo
    slots      keys   load   rebuilds   kicks/insert
   131072     26214    20%          0           0.19 
   131072     35389    27%          0           0.26 
   131072     44564    34%          0           0.35 
   131072     53739    41%          0           0.46 
   131072     62914    48%          0           0.65 
```

Kicks per insert grow as the load approaches 50% (the last row), and past that the rebuild loop would never finish. Cuckoo shines where lookups dominate and the worst case matters (packet classification, hardware tables, read-mostly caches); the cost is two cache misses per lookup and lower density.

## Hash flooding: when the input is hostile

Everything above assumes keys hash uniformly. If an attacker chooses the keys and knows the hash function, they can make **every key collide**, turning O(1) into O(n) per operation and the whole build into O(n²). This is a real, practical attack (Crosby & Wallach 2003; the 2011 "hash collision" disclosures hit PHP, Java, Ruby, Python and ASP.NET; one small POST body with a few thousand crafted parameter names stalled a server).

The demonstration uses the polynomial hash `h = 31·h + c` (Java's `String.hashCode`). Because `"Aa"` and `"BB"` both hash to 2112, *any* concatenation of those two blocks collides with any other of the same length, so *k* blocks give 2^k colliding strings. The defence is a **keyed** hash with a secret, per-process random key, e.g. **SipHash-2-4**, which is designed so that without the key an attacker cannot predict collisions.

```c
// SipHash-2-4 (Aumasson & Bernstein): a keyed 64-bit hash for untrusted keys.
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))
#define SIPROUND do { \
    v0 += v1; v1 = ROTL(v1, 13); v1 ^= v0; v0 = ROTL(v0, 32); \
    v2 += v3; v3 = ROTL(v3, 16); v3 ^= v2; \
    v0 += v3; v3 = ROTL(v3, 21); v3 ^= v0; \
    v2 += v1; v1 = ROTL(v1, 17); v1 ^= v2; v2 = ROTL(v2, 32); } while (0)

static uint64_t siphash24(const void *data, size_t len, const uint8_t key[16]) {
    uint64_t k0, k1; memcpy(&k0, key, 8); memcpy(&k1, key + 8, 8);        // little-endian host assumed
    uint64_t v0 = 0x736f6d6570736575ULL ^ k0, v1 = 0x646f72616e646f6dULL ^ k1;
    uint64_t v2 = 0x6c7967656e657261ULL ^ k0, v3 = 0x7465646279746573ULL ^ k1;
    const uint8_t *p = data, *end = p + (len & ~(size_t)7);
    for (; p != end; p += 8) { uint64_t m; memcpy(&m, p, 8); v3 ^= m; SIPROUND; SIPROUND; v0 ^= m; }
    uint64_t b = (uint64_t)len << 56;
    for (size_t i = 0; i < (len & 7); i++) b |= (uint64_t)p[i] << (8 * i);
    v3 ^= b; SIPROUND; SIPROUND; v0 ^= b;
    v2 ^= 0xff; SIPROUND; SIPROUND; SIPROUND; SIPROUND;
    return v0 ^ v1 ^ v2 ^ v3;
}
```

```c
// Hash flooding: a chained table keyed by strings, with (a) a weak polynomial hash and (b) SipHash-2-4 with a secret key.
// The attacker's keys all collide under h = 31*h + c (as Java's String.hashCode did), so one bucket gets everything.
#include "siphash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct node { struct node *next; char *key; } node;
typedef struct { node **b; size_t nb, n; } table_t;
static uint8_t secret[16] = {0x13,0x37,0xc0,0xde,0x5e,0xed,0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99};   // random per process in real code

static uint64_t h_poly(const char *s, size_t len) { uint32_t h = 0; for (size_t i = 0; i < len; i++) h = 31 * h + (unsigned char)s[i]; return h; }
static uint64_t h_sip(const char *s, size_t len)  { return siphash24(s, len, secret); }

static void insert(table_t *t, uint64_t (*hash)(const char *, size_t), const char *key, size_t len, size_t *steps) {
    size_t i = hash(key, len) % t->nb;
    for (node *p = t->b[i]; p; p = p->next) { (*steps)++; if (strcmp(p->key, key) == 0) return; }   // search before insert
    node *x = malloc(sizeof *x); x->key = strdup(key); x->next = t->b[i]; t->b[i] = x; t->n++;
}

static void run(const char *name, uint64_t (*hash)(const char *, size_t), char **keys, size_t n) {
    table_t t = { calloc(1 << 16, sizeof(node *)), 1 << 16, 0 };
    size_t steps = 0, longest = 0;
    for (size_t i = 0; i < n; i++) insert(&t, hash, keys[i], strlen(keys[i]), &steps);
    for (size_t i = 0; i < t.nb; i++) { size_t c = 0; for (node *p = t.b[i]; p; p = p->next) c++; if (c > longest) longest = c; }
    printf("  %-22s %7zu keys: %12zu chain steps, longest chain %6zu\n", name, n, steps, longest);
    for (size_t i = 0; i < t.nb; i++) for (node *p = t.b[i], *q; p; p = q) { q = p->next; free(p->key); free(p); }
    free(t.b);
}

// 2^k keys: every key is k blocks, each block "Aa" or "BB" (both hash to 2112 under 31*h + c, and so every concatenation collides)
static char **make_attack(int k, size_t *n) {
    *n = (size_t)1 << k; char **keys = malloc(*n * sizeof *keys);
    for (size_t i = 0; i < *n; i++) {
        char *s = malloc(2 * k + 1);
        for (int b = 0; b < k; b++) { const char *blk = (i >> b) & 1 ? "BB" : "Aa"; s[2*b] = blk[0]; s[2*b+1] = blk[1]; }
        s[2*k] = 0; keys[i] = s;
    }
    return keys;
}

int main(void) {
    printf("SipHash-2-4 self-test (reference vector, key 00..0f, message 00..0e): ");
    uint8_t k[16], msg[15]; for (int i = 0; i < 16; i++) k[i] = i; for (int i = 0; i < 15; i++) msg[i] = i;
    printf("%s\n", siphash24(msg, 15, k) == 0xa129ca6149be45e5ULL ? "ok" : "FAIL");

    for (int bits = 12; bits <= 16; bits += 2) {
        size_t n; char **keys = make_attack(bits, &n);
        printf("attack keys: %zu strings of length %d, all with the same polynomial hash\n", n, 2 * bits);
        run("polynomial 31*h+c", h_poly, keys, n);
        run("SipHash-2-4 (keyed)", h_sip, keys, n);
        for (size_t i = 0; i < n; i++) free(keys[i]);
        free(keys);
    }
    return 0;
}
```

```text
$ gcc -O2 -Wall -Wextra flood.c -o flood && ./flood
SipHash-2-4 self-test (reference vector, key 00..0f, message 00..0e): ok
attack keys: 4096 strings of length 24, all with the same polynomial hash
  polynomial 31*h+c         4096 keys:      8386560 chain steps, longest chain   4096
  SipHash-2-4 (keyed)       4096 keys:          108 chain steps, longest chain      3
attack keys: 16384 strings of length 28, all with the same polynomial hash
  polynomial 31*h+c        16384 keys:    134209536 chain steps, longest chain  16384
  SipHash-2-4 (keyed)      16384 keys:         2045 chain steps, longest chain      5
attack keys: 65536 strings of length 32, all with the same polynomial hash
  polynomial 31*h+c        65536 keys:   2147450880 chain steps, longest chain  65536
  SipHash-2-4 (keyed)      65536 keys:        32549 chain steps, longest chain      7
```

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
