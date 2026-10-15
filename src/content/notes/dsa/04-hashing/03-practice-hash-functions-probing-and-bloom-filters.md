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
  - label: "Knuth — The Art of Computer Programming, Vol. 3, §6.4 (Hashing)"
  - label: "Bloom, B. H. — Space/Time Trade-offs in Hash Coding with Allowable Errors (CACM, 1970)"
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 11"
---

Hash tables are simple to describe and easy to get subtly wrong. Each exercise is a small experiment: predict the outcome first, then run it.

## Exercises

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Judge a hash function.** Hash 2,000 pseudo-random lowercase words into 256 buckets using (a) the word's length, (b) its first letter, (c) djb2 (`h = h*33 + c`, starting at 5381). For each, count the buckets used and the longest chain. Predict the ranking and say what the *ideal* chain length is.

**2. The deletion bug.** A linear-probing table with 8 slots stores keys 8, 16 and 24, which all hash to slot 0 (`key % 8`). Delete 16 by simply marking its slot empty. What does a lookup of 24 return, and why? Fix it with a **tombstone** marker and show that lookups work and that a later insert can reuse the tombstone.

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 2: deleting from a linear-probing table without breaking later searches.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 8
enum { EMPTY = -1, TOMBSTONE = -2 };
static int slot[N];
static void init(void) { for (int i = 0; i < N; i++) slot[i] = EMPTY; }
static int home(int key) { return key % N; }

/* insert: the first EMPTY or TOMBSTONE slot along the probe sequence */
static void insert(int key) {
    for (int i = 0, p = home(key); i < N; i++, p = (p + 1) % N)
        if (slot[p] == EMPTY || slot[p] == TOMBSTONE) { slot[p] = key; return; }
}

/* find: the slot holding key, or -1. An EMPTY slot ends the search; a TOMBSTONE does not. */
static int find(int key) {
    for (int i = 0, p = home(key); i < N; i++, p = (p + 1) % N) {
        if (slot[p] == EMPTY) return -1;
        if (slot[p] == key) return p;
    }
    return -1;
}

/* ===== Your code: edit below this line ===== */
void remove_key(int key) {
    int p = find(key);
    if (p >= 0) slot[p] = EMPTY;        /* breaks the probe chain of keys stored after p */
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    init();
    insert(8); insert(16); insert(24);          /* all hash to slot 0, so they sit in slots 0, 1 and 2 */
    remove_key(16);
    check_int("find(16) after removing it", find(16), -1);
    check_int("find(24) after removing 16", find(24), 2);
    check_int("find(8) after removing 16", find(8), 0);
    insert(32);
    check_int("insert(32) reuses the freed slot", find(32), 1);
    remove_key(99);
    check_int("removing a missing key changes nothing", find(24), 2);
    return summary();
}
```

</details>

**3. Bloom filter arithmetic.** A Bloom filter with `m = 8192` bits and `k = 4` hash functions holds `n = 1000` keys. Use the formula `(1 - e^(-kn/m))^k` to predict the false-positive rate, then build it and measure with 100,000 keys that were never inserted. Can a Bloom filter ever report a stored key as missing?

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

```c
#include <stdio.h>
#include <string.h>

#define BUCKETS 256
#define WORDS 2000

static unsigned h_length(const char *s) { return (unsigned)strlen(s); }                 /* bad: only ~10 distinct values */
static unsigned h_first(const char *s) { return (unsigned char)s[0]; }                    /* bad: 26 distinct values at most */
static unsigned h_djb2(const char *s) { unsigned h = 5381; while (*s) h = h * 33 + (unsigned char)*s++; return h; }

static void measure(const char *name, unsigned (*h)(const char *), char (*words)[16]) {
    int count[BUCKETS] = {0}, used = 0, longest = 0;
    for (int i = 0; i < WORDS; i++) count[h(words[i]) % BUCKETS]++;
    for (int b = 0; b < BUCKETS; b++) { if (count[b]) used++; if (count[b] > longest) longest = count[b]; }
    printf("%-14s buckets used %3d of %d, longest chain %4d (ideal about %d)\n", name, used, BUCKETS, longest, WORDS / BUCKETS);
}

int main(void) {
    static char words[WORDS][16];
    unsigned seed = 7;                                           /* fixed seed: same words every run */
    for (int i = 0; i < WORDS; i++) {
        seed = seed * 1103515245u + 12345u;
        int len = 3 + (int)((seed >> 16) % 8);
        for (int j = 0; j < len; j++) { seed = seed * 1103515245u + 12345u; words[i][j] = (char)('a' + (seed >> 16) % 26); }
        words[i][len] = '\0';
    }
    measure("length", h_length, words);
    measure("first letter", h_first, words);
    measure("djb2", h_djb2, words);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o p4_hash p4_hash.c && ./p4_hash; rm -f p4_hash
length         buckets used   8 of 256, longest chain  260 (ideal about 7)
first letter   buckets used  26 of 256, longest chain   90 (ideal about 7)
djb2           buckets used 256 of 256, longest chain   15 (ideal about 7)
```

With 2,000 keys in 256 buckets a perfect spread gives about 8 per bucket (the table prints the integer 7). Hashing by **length** can produce only about 8 distinct values (words here are 3 to 10 letters), so 8 buckets hold everything and the longest chain is 260: lookups degrade toward a linear scan. **First letter** allows at most 26 values: 26 buckets, chains up to 90. **djb2** uses every bucket and the longest chain is 15, close to what random placement gives (the maximum of 256 random bucket loads exceeds the mean by a few standard deviations). A good hash function must use *all* the input and spread the output *evenly*; "cheap to compute" is not enough.

**2.**

```c
#include <stdio.h>

#define N 8
enum { EMPTY = -1, TOMBSTONE = -2 };

static int slot[N];

static void init(void) { for (int i = 0; i < N; i++) slot[i] = EMPTY; }
static int home(int key) { return key % N; }

static void insert(int key) {
    for (int i = 0, p = home(key); i < N; i++, p = (p + 1) % N)
        if (slot[p] == EMPTY || slot[p] == TOMBSTONE) { slot[p] = key; return; }     /* linear probing */
}
static int find(int key) {
    for (int i = 0, p = home(key); i < N; i++, p = (p + 1) % N) {
        if (slot[p] == EMPTY) return -1;                       /* an empty slot ends the search: the key cannot be further on */
        if (slot[p] == key) return p;
    }
    return -1;
}
static void remove_naive(int key) { int p = find(key); if (p >= 0) slot[p] = EMPTY; }
static void remove_tombstone(int key) { int p = find(key); if (p >= 0) slot[p] = TOMBSTONE; }

static void show(const char *label) {
    printf("%-26s", label);
    for (int i = 0; i < N; i++) { if (slot[i] == EMPTY) printf("  . "); else if (slot[i] == TOMBSTONE) printf("  X "); else printf("%3d ", slot[i]); }
    printf("\n");
}

int main(void) {
    init(); insert(8); insert(16); insert(24);               /* 8, 16 and 24 all hash to slot 0, so they probe into 1 and 2 */
    show("after inserting 8,16,24:");
    remove_naive(16);
    show("naive delete of 16:");
    printf("find(24) after naive delete -> %d   (24 is still stored, but the search stopped at the hole)\n", find(24));

    init(); insert(8); insert(16); insert(24);
    remove_tombstone(16);
    show("tombstone delete of 16:");
    printf("find(24) after tombstone    -> %d\n", find(24));
    insert(32);
    show("insert 32 reuses the X:");
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o p4_probe p4_probe.c && ./p4_probe; rm -f p4_probe
after inserting 8,16,24:    8  16  24   .   .   .   .   . 
naive delete of 16:         8   .  24   .   .   .   .   . 
find(24) after naive delete -> -1   (24 is still stored, but the search stopped at the hole)
tombstone delete of 16:     8   X  24   .   .   .   .   . 
find(24) after tombstone    -> 2
insert 32 reuses the X:     8  32  24   .   .   .   .   . 
```

Keys 8, 16 and 24 collide at slot 0 and probe into slots 1 and 2. The naive deletion empties slot 1; a lookup for 24 starts at slot 0, moves to slot 1, sees an **empty** slot and concludes "if 24 were here, probing would not have passed an empty slot", so it reports not found although 24 sits in slot 2. Probing relies on the invariant *no empty slot between a key's home and its position*, and a naive delete breaks it. A **tombstone** (`X`) means "occupied once; keep probing": `find` continues through it, and `insert` may reuse it (the last line). Tombstones accumulate, so real tables count them toward the load factor and rebuild when there are too many; the alternative is **backward-shift deletion**, which moves later keys of the cluster back and leaves no tombstone.

**3.**

```c
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define M 8192            /* bits */
#define K 4               /* hash functions */

static uint8_t bits[M / 8];

static uint32_t mix(uint32_t x, uint32_t salt) {            /* a small integer hash; salt gives K different functions */
    x ^= salt * 0x9E3779B9u; x ^= x >> 16; x *= 0x85EBCA6Bu; x ^= x >> 13; x *= 0xC2B2AE35u; x ^= x >> 16;
    return x;
}
static void add(uint32_t key) { for (uint32_t k = 0; k < K; k++) { uint32_t b = mix(key, k + 1) % M; bits[b / 8] |= (uint8_t)(1u << (b % 8)); } }
static int maybe_contains(uint32_t key) {
    for (uint32_t k = 0; k < K; k++) { uint32_t b = mix(key, k + 1) % M; if (!(bits[b / 8] & (1u << (b % 8)))) return 0; }
    return 1;                                                /* all K bits set: probably present */
}

int main(void) {
    const int n = 1000;
    memset(bits, 0, sizeof bits);
    for (int i = 0; i < n; i++) add((uint32_t)i);
    int missed = 0, false_pos = 0, probes = 100000;
    for (int i = 0; i < n; i++) if (!maybe_contains((uint32_t)i)) missed++;
    for (int i = 0; i < probes; i++) if (maybe_contains((uint32_t)(1000000 + i))) false_pos++;
    double predicted = pow(1.0 - exp(-(double)K * n / M), K);
    printf("members reported missing (must be 0): %d\n", missed);
    printf("false positives: %d of %d = %.2f%% (formula predicts %.2f%%)\n", false_pos, probes, 100.0 * false_pos / probes, 100.0 * predicted);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o p4_bloom p4_bloom.c -lm && ./p4_bloom; rm -f p4_bloom
members reported missing (must be 0): 0
false positives: 2259 of 100000 = 2.26% (formula predicts 2.23%)
```

The formula predicts about 2.23%; the measurement lands within a few hundredths of a percent of it. A false positive happens when all `k` bits of a never-inserted key were set by *other* keys. Bits are only ever set, never cleared, so a stored key always finds its `k` bits set: **no false negatives**, the first line prints 0 missing. The trade-off is the table's size: 8 bits per key here gives a 2% error rate, and about 10 bits per key with the optimal `k = (m/n) ln 2` gives 1%. You cannot delete from a plain Bloom filter (clearing a bit would also forget other keys); counting variants exist.

</details>

## Key takeaways

1. A hash function must use all of the key and spread results evenly; measure chain lengths instead of trusting intuition.
2. In open addressing, deleting by emptying a slot breaks probe chains; use tombstones (and rebuild) or backward-shift deletion.
3. A Bloom filter has false positives but never false negatives, and its error rate follows `(1 - e^(-kn/m))^k`.
