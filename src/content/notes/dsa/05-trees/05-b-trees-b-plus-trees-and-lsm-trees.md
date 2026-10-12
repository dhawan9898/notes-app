---
title: B-Trees, B+ Trees and LSM Trees
track: dsa
module: Trees
moduleOrder: 5
order: 5
level: advanced
summary: "Search trees built for block storage — why wide nodes win, a tested B+ tree measured across node sizes, page-size arithmetic for billions of keys, and a counting LSM tree that shows write, read and space amplification and what Bloom filters buy."
sources:
  - label: "Bayer & McCreight — Organization and Maintenance of Large Ordered Indexes (Acta Informatica 1(3), 1972)"
  - label: "Comer — The Ubiquitous B-Tree (ACM Computing Surveys 11(2), 1979)"
  - label: "Lehman & Yao — Efficient Locking for Concurrent Operations on B-Trees (ACM TODS 6(4), 1981)"
  - label: "Graefe — Modern B-Tree Techniques (Foundations and Trends in Databases 3(4), 2011)"
  - label: "O'Neil, Cheng, Gawlick, O'Neil — The Log-Structured Merge-Tree (LSM-Tree) (Acta Informatica 33(4), 1996)"
  - label: "Athanassoulis et al. — Designing Access Methods, the RUM Conjecture (EDBT 2016)"
  - label: "Dayan, Athanassoulis, Idreos — Monkey, Optimal Navigable Key-Value Store (SIGMOD 2017)"
  - label: "Aggarwal & Vitter — The Input/Output Complexity of Sorting and Related Problems (CACM 31(9), 1988)"
  - label: "RocksDB Wiki — Leveled Compaction"
    url: https://github.com/facebook/rocksdb/wiki/Leveled-Compaction
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 18 (B-Trees)"
---

A binary search tree is optimal when each comparison costs the same and memory is uniform. Storage is neither: a disk or SSD transfers whole **blocks** (typically 4–16 KB), reads cost microseconds to milliseconds against nanoseconds for RAM, and even in memory a cache miss costs ~100× a cache hit. In the **external-memory model** (Aggarwal & Vitter) the cost of an algorithm is the number of block transfers, and a search tree should therefore have a *fan-out* of about one block's worth of keys: height `log_B n` instead of `log_2 n`. That is the B-tree.

## B-trees and B+ trees

A **B-tree** of order `m` (Bayer & McCreight, 1972) is a balanced multiway search tree:

- every node holds up to `m` keys (and `m+1` children if internal) and, except the root, at least about `m/2`;
- all leaves are at the same depth, so the tree grows and shrinks at the **root**, not at the leaves — balance is automatic rather than repaired by rotations;
- keys inside a node are sorted and searched by binary search (or linear scan for small nodes).

**Insertion** descends to a leaf and inserts into it; if the node now holds more than `m` keys it **splits** in half and pushes the median up into the parent, which may split in turn — up to the root, which if split creates a new root (the only way height increases). **Deletion** removes the key; a node that falls below the minimum first **borrows** from a sibling (rotation through the parent) or **merges** with one. Both are `O(log_m n)` block accesses.

The variant almost every system uses is the **B+ tree**:

| | B-tree | B+ tree |
|---|---|---|
| Values stored in | every node | leaves only |
| Internal nodes hold | keys + values | only separator keys → higher fan-out |
| Leaves linked | no | yes — range scans follow `next` pointers |
| Lookup cost | may stop early | always root-to-leaf (uniform) |

Putting values only in leaves makes internal nodes smaller (so more keys per block, hence a shorter tree and a better chance the upper levels stay cached) and makes **range queries** a sequential walk along the leaf chain. When a leaf splits, the separator is *copied* up (it still lives in the leaf); when an internal node splits, the median *moves* up. The code below implements exactly this.

### Implementation

The tree is in memory, so what is measured is the effect of node size on pointer chases and cache behavior, not disk I/O. Nodes are allocated in one block sized from the order `M` (keys, then 8-byte slots that hold either a child pointer or a value). The test suite checks sorted keys, separator bounds, uniform leaf depth, the count of keys, every lookup against a reference table and the leaf chain after 300,000 random puts that include overwrites. Deletion is not implemented here — it is mechanically the reverse of split, with borrow-or-merge — so this tree supports insert, overwrite, point lookup and range scans only.

```c
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

/* In-memory B+ tree: internal nodes route, leaves hold all keys/values and are chained for range scans.
 * `M` (max keys per node) is a runtime parameter so we can compare node sizes; arrays are sized for the largest M. */
#define MAXM 512
typedef struct Node {
    int leaf, n;                                   /* n = number of keys */
    struct Node *next;                             /* leaf chain */
    uint32_t k[];                                  /* keys[kcap] followed by slots[kcap] (uint64: a child pointer or a value) */
} Node;
typedef struct { Node *root; int M, kcap; size_t nodes, leaves; int height; } BPT;

static inline uint64_t *slots(const BPT *t, const Node *x) { return (uint64_t *)(x->k + t->kcap); }
static Node *new_node(BPT *t, int leaf) {                      /* sized for this tree's M: header + (M+2) keys + (M+2) slots */
    Node *n = calloc(1, sizeof *n + (size_t)t->kcap * (sizeof(uint32_t) + sizeof(uint64_t))); n->leaf = leaf; t->nodes++; t->leaves += leaf; return n;
}
static void bpt_init(BPT *t, int M) { t->M = M; t->kcap = (M + 3) & ~1; t->nodes = t->leaves = 0; t->root = new_node(t, 1); t->height = 1; }

static int lower(const uint32_t *k, int n, uint32_t key) { int lo = 0, hi = n; while (lo < hi) { int m = (lo + hi) / 2; if (k[m] < key) lo = m + 1; else hi = m; } return lo; }
static int upper(const uint32_t *k, int n, uint32_t key) { int lo = 0, hi = n; while (lo < hi) { int m = (lo + hi) / 2; if (k[m] <= key) lo = m + 1; else hi = m; } return lo; }

static const Node *find_leaf(const BPT *t, uint32_t key) {
    const Node *x = t->root;
    while (!x->leaf) x = (const Node *)slots(t, x)[upper(x->k, x->n, key)];       /* separator k[i] is the smallest key of child i+1 */
    return x;
}
static int bpt_get(const BPT *t, uint32_t key, uint32_t *val) {
    const Node *l = find_leaf(t, key); int i = lower(l->k, l->n, key);
    if (i < l->n && l->k[i] == key) { *val = (uint32_t)slots(t, l)[i]; return 1; }
    return 0;
}
/* insert into subtree; if the node splits, returns the new right sibling and its separator key */
static Node *insert_rec(BPT *t, Node *x, uint32_t key, uint32_t val, uint32_t *sep, int *added) {
    int M = t->M; uint64_t *sl = slots(t, x);
    if (x->leaf) {
        int i = lower(x->k, x->n, key);
        if (i < x->n && x->k[i] == key) { sl[i] = val; return NULL; }
        memmove(x->k + i + 1, x->k + i, (size_t)(x->n - i) * sizeof(uint32_t));
        memmove(sl + i + 1, sl + i, (size_t)(x->n - i) * sizeof(uint64_t));
        x->k[i] = key; sl[i] = val; x->n++; *added = 1;
        if (x->n <= M) return NULL;
        Node *r = new_node(t, 1); int h = x->n / 2;                    /* split leaf: right gets the upper half; separator is COPIED up */
        r->n = x->n - h; memcpy(r->k, x->k + h, (size_t)r->n * sizeof(uint32_t)); memcpy(slots(t, r), sl + h, (size_t)r->n * sizeof(uint64_t));
        x->n = h; r->next = x->next; x->next = r; *sep = r->k[0]; return r;
    }
    int i = upper(x->k, x->n, key);
    uint32_t csep; Node *r = insert_rec(t, (Node *)sl[i], key, val, &csep, added);
    if (!r) return NULL;
    memmove(x->k + i + 1, x->k + i, (size_t)(x->n - i) * sizeof(uint32_t));
    memmove(sl + i + 2, sl + i + 1, (size_t)(x->n - i) * sizeof(uint64_t));
    x->k[i] = csep; sl[i + 1] = (uint64_t)(uintptr_t)r; x->n++;
    if (x->n <= M) return NULL;
    Node *s = new_node(t, 0); int h = x->n / 2;                        /* split internal: middle key MOVES up */
    *sep = x->k[h]; s->n = x->n - h - 1;
    memcpy(s->k, x->k + h + 1, (size_t)s->n * sizeof(uint32_t)); memcpy(slots(t, s), sl + h + 1, (size_t)(s->n + 1) * sizeof(uint64_t));
    x->n = h; return s;
}
static void bpt_put(BPT *t, uint32_t key, uint32_t val, size_t *count) {
    uint32_t sep; int added = 0; Node *r = insert_rec(t, t->root, key, val, &sep, &added);
    if (added) (*count)++;
    if (r) { Node *nr = new_node(t, 0); nr->n = 1; nr->k[0] = sep; slots(t, nr)[0] = (uint64_t)(uintptr_t)t->root; slots(t, nr)[1] = (uint64_t)(uintptr_t)r; t->root = nr; t->height++; }
}
static void bpt_free_rec(const BPT *t, Node *x) { if (!x->leaf) for (int i = 0; i <= x->n; i++) bpt_free_rec(t, (Node *)slots(t, x)[i]); free(x); }
static void bpt_free(BPT *t) { bpt_free_rec(t, t->root); }

/* structural check: sorted keys, separators bound the children, equal depth */
static int check(const BPT *t, const Node *x, uint32_t lo, uint32_t hi, int depth, int *leaf_depth, size_t *keys) {
    for (int i = 1; i < x->n; i++) if (x->k[i - 1] >= x->k[i]) return 0;
    for (int i = 0; i < x->n; i++) if (x->k[i] < lo || x->k[i] >= hi) return 0;
    if (x->leaf) { if (*leaf_depth < 0) *leaf_depth = depth; *keys += (size_t)x->n; return *leaf_depth == depth; }
    for (int i = 0; i <= x->n; i++)
        if (!check(t, (const Node *)slots(t, x)[i], i ? x->k[i - 1] : lo, i < x->n ? x->k[i] : hi, depth + 1, leaf_depth, keys)) return 0;
    return 1;
}

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }
static int cmp_u32(const void *a, const void *b) { return (*(const uint32_t *)a > *(const uint32_t *)b) - (*(const uint32_t *)a < *(const uint32_t *)b); }

int main(void) {
    /* correctness: 300k random puts (with overwrites) vs a hash-free reference: sorted array rebuilt at the end */
    { BPT t; bpt_init(&t, 8); size_t cnt = 0; enum { N = 300000, RANGE = 100000 }; static uint32_t ref[RANGE]; static char has[RANGE];
      for (int i = 0; i < N; i++) { uint32_t k = rnd() % RANGE, v = rnd(); bpt_put(&t, k, v, &cnt); ref[k] = v; has[k] = 1; }
      size_t distinct = 0; for (int i = 0; i < RANGE; i++) distinct += has[i];
      int ld = -1; size_t keys = 0; assert(check(&t, t.root, 0, UINT32_MAX, 1, &ld, &keys)); assert(keys == distinct && cnt == distinct && ld == t.height);
      for (int k = 0; k < RANGE; k++) { uint32_t v; assert(bpt_get(&t, (uint32_t)k, &v) == has[k]); if (has[k]) assert(v == ref[k]); }
      size_t scanned = 0; uint32_t prev = 0; int first = 1;
      for (const Node *l = find_leaf(&t, 0); l; l = l->next) for (int i = 0; i < l->n; i++) { assert(first || l->k[i] > prev); prev = l->k[i]; first = 0; scanned++; }
      assert(scanned == distinct);
      printf("M = 8 B+ tree: %d puts, %zu distinct keys, structure/lookup/leaf-chain checks passed (height %d)\n\n", N, distinct, t.height);
      bpt_free(&t); }

    /* shape and speed vs node size, n = 2,000,000 random distinct-ish keys */
    size_t n = 2000000; uint32_t *keys = malloc(n * sizeof *keys), *probe = malloc(n * sizeof *probe);
    for (size_t i = 0; i < n; i++) keys[i] = rnd();
    for (size_t i = 0; i < n; i++) probe[i] = keys[rnd() % n];
    printf("%-12s %6s %8s %12s %12s %10s %10s %10s\n", "structure", "M", "height", "nodes", "leaf fill", "insert ns", "lookup ns", "scan ns/key");
    int Ms[] = { 4, 16, 64, 256, 510 };
    for (int mi = 0; mi < 5; mi++) {
        BPT t; bpt_init(&t, Ms[mi]); size_t cnt = 0; double t0 = now();
        for (size_t i = 0; i < n; i++) bpt_put(&t, keys[i], (uint32_t)i, &cnt);
        double ti = (now() - t0) / (double)n * 1e9; uint32_t v; long sink = 0;
        t0 = now(); for (size_t i = 0; i < n; i++) sink += bpt_get(&t, probe[i], &v) ? v : 0; double tl = (now() - t0) / (double)n * 1e9;
        t0 = now(); size_t sc = 0; for (const Node *l = find_leaf(&t, 0); l; l = l->next) for (int i = 0; i < l->n; i++) sink += (long)slots(&t, l)[i], sc++; double tsc = (now() - t0) / (double)sc * 1e9;
        printf("%-12s %6d %8d %12zu %11.0f%% %10.0f %10.0f %10.1f\n", "B+ tree", Ms[mi], t.height, t.nodes, 100.0 * (double)cnt / ((double)t.leaves * Ms[mi]), ti, tl, tsc);
        if (sink == 42) puts("");
        bpt_free(&t);
    }
    qsort(keys, n, sizeof *keys, cmp_u32);
    long sink = 0; double t0 = now();
    for (size_t i = 0; i < n; i++) { size_t lo = 0, hi = n; while (lo < hi) { size_t m = (lo + hi) / 2; if (keys[m] < probe[i]) lo = m + 1; else hi = m; } sink += keys[lo % n]; }
    printf("%-12s %6s %8s %12s %12s %10s %10.0f %10s\n", "sorted array", "-", "-", "-", "100%", "(static)", (now() - t0) / (double)n * 1e9, "-");
    if (sink == 42) puts("");
    free(keys); free(probe);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined bplus.c -o bplus_asan && ./bplus_asan | head -2 && gcc -std=gnu11 -O2 -Wall -Wextra bplus.c -o bplus && ./bplus
M = 8 B+ tree: 300000 puts, 95075 distinct keys, structure/lookup/leaf-chain checks passed (height 6)

M = 8 B+ tree: 300000 puts, 95075 distinct keys, structure/lookup/leaf-chain checks passed (height 6)

structure         M   height        nodes    leaf fill  insert ns  lookup ns scan ns/key
B+ tree           4       12       936599          73%        939        681       60.2
B+ tree          16        6       193478          70%        381        356       13.5
B+ tree          64        4        45852          70%        251        279        4.6
B+ tree         256        3        11196          70%        265        251        2.4
B+ tree         510        3         5527          71%        311        246        1.8
sorted array      -        -            -         100%   (static)        136          -
```

Reading the table (the first two lines come from the sanitizer build; the timed table is the optimized build):

- **Height falls as node size grows**: from 12 levels at 4 keys per node to 3 levels at 256 or more — a binary tree on two million keys is at least 21 levels deep.
- **Leaf fill is about 70 %**, consistent with the classic result that random insertions into a B-tree leave nodes about `ln 2 ≈ 69 %` full on average. A B+ tree therefore uses ~1.4× the space of a perfectly packed array; sequentially loaded trees (bulk loading) can be packed to ~100 %.
- **Lookups and inserts get faster with wider nodes up to a point**, then flatten or get slightly worse: once the node is large, a binary search inside a node costs more cache lines than the levels saved. For disk-based indexes the node size is the page size; for in-memory ones a few cache lines to a few KB is typical.
- **Range scans improve dramatically** (roughly 50 ns/key at 4-key nodes down to 2–3 ns/key for wide leaves), because consecutive keys are contiguous; this is the B+ tree's defining advantage over a binary tree and over hash indexes.
- The **sorted array** is still the fastest for lookups — it has no pointers — but it cannot be updated cheaply: that is the trade a B+ tree makes.

### Page-size arithmetic

How many levels does a database index need? With 8-byte child pointers and a 32-byte page header, an internal node of `P` bytes holds about `P / (key + 8)` entries.

```python
import math
# Fan-out and height of a B+ tree for a given page size, key size and pointer size.
# An internal node holds m keys and m+1 child pointers: m*key + (m+1)*ptr <= page - header.
HEADER = 32
def fanout(page, key, ptr):
    return (page - HEADER - ptr) // (key + ptr) + 1
print("B+ tree height (root to leaf, counting the leaf level) at 70% fill; 8-byte child pointers, 8-byte values, 32-byte page header")
print(f"{'page':>8} {'key':>5} {'fan-out':>8} | {'n=1e6':>6} {'n=1e9':>6} {'n=1e12':>7}   keys per leaf")
for page in (4096, 8192, 16384, 65536):
    for key in (8, 16, 64):
        f = fanout(page, key, 8)
        leaf_keys = int((page - HEADER) // (key + 8) * 0.7)
        def h(n):
            leaves = math.ceil(n / leaf_keys); height = 1
            while leaves > 1: leaves = math.ceil(leaves / (f * 0.7)); height += 1
            return height
        print(f"{page:>8} {key:>5} {f:>8} | {h(1e6):>6} {h(1e9):>6} {h(1e12):>7}   {leaf_keys}")
print("\nbinary tree for comparison: log2(1e9) =", round(math.log2(1e9), 1), "levels")
```

```text
$ python3 pagemath.py
B+ tree height (root to leaf, counting the leaf level) at 70% fill; 8-byte child pointers, 8-byte values, 32-byte page header
    page   key  fan-out |  n=1e6  n=1e9  n=1e12   keys per leaf
    4096     8      254 |      3      5       6   177
    4096    16      170 |      3      5       6   118
    4096    64       57 |      4      6       8   39
    8192     8      510 |      3      4       5   357
    8192    16      340 |      3      4       6   237
    8192    64      114 |      4      5       7   79
   16384     8     1022 |      3      4       5   715
   16384    16      682 |      3      4       5   476
   16384    64      228 |      3      5       6   158
   65536     8     4094 |      2      3       4   2865
   65536    16     2730 |      2      3       4   1910
   65536    64      910 |      3      4       5   636

binary tree for comparison: log2(1e9) = 29.9 levels
```

A billion 8-byte keys fit in a tree 4–5 levels high with 4–8 KB pages; a trillion in 5–6. The top two levels are only a few hundred KB to a few MB and live permanently in the buffer pool, so a point lookup in a billion-row table costs about **2–3 actual disk reads** — compared with ~30 for a binary tree. This is why a primary-key lookup in InnoDB, PostgreSQL or SQLite stays fast as tables grow. Longer keys (64-byte strings) cut the fan-out and add a level; databases use **prefix and suffix truncation** in separators to restore it.

### What production B-trees add

- **Concurrency.** Lock coupling (latch crabbing) holds a parent latch only until the child is known to be "safe" (not going to split); Lehman & Yao's **B-link tree** adds a right-sibling pointer and a high key per node so that readers can proceed without holding parent latches during a split — the design behind PostgreSQL's `nbtree`.
- **Durability.** Changes go through a write-ahead log; a page split must be atomic across several pages, which is why recovery logic dominates real implementations. A **copy-on-write** B+ tree (LMDB, btrfs) never overwrites pages: it writes a new path to the root, gaining crash safety and cheap snapshots at the cost of write amplification and fragmentation.
- **Bulk loading** sorted data directly into packed leaves is far faster than repeated insertion, and gives 90–100 % fill.
- **Variable-length keys and values**, overflow pages, and slotted-page layouts.
- **Fill factor**: leave space (e.g. 70–90 %) on build to delay splits for update-heavy workloads.

Real systems: InnoDB (clustered B+ tree on the primary key; 16 KB pages by default), PostgreSQL (8 KB pages), SQLite (4 KB by default), ext4 (HTree for large directories), btrfs and XFS (B-trees for extents and metadata), NTFS and APFS.

## LSM trees: turning random writes into sequential ones

A B+ tree updates pages **in place**: a single-row write dirties a leaf page of several KB, causing a random page write (plus log). On a spinning disk or a busy SSD, random writes are the expensive resource. The **log-structured merge tree** (O'Neil et al., 1996) takes the opposite approach: *never* update in place, only append and merge.

```text
 writes ──► WAL (sequential) ──► memtable (sorted, in RAM)
                                     │ full: flush as an immutable sorted file
                                     ▼
   L0:  [run][run][run][run]         newest first, key ranges overlap
   L1:  [        one sorted run        ]  ◄── compaction merges L0 → L1
   L2:  [               larger, sorted run                       ]  ◄── L1 → L2
```

- **Write path:** append to a write-ahead log (for recovery), insert into an in-memory sorted structure (the *memtable*, commonly a skip list), and acknowledge. When the memtable fills, flush it as an immutable **SSTable** — a sorted file with an index and a Bloom filter. All disk writes are large and sequential.
- **Read path:** check the memtable, then each run from newest to oldest; the first match wins. Per-run **Bloom filters** (see *Probabilistic Structures*) let a lookup skip nearly all runs that cannot contain the key.
- **Deletes** write a **tombstone** — a marker that shadows older versions. It can be discarded only when it has been compacted into the bottom level, otherwise the older value would reappear.
- **Compaction** merges runs into larger ones in the background, dropping overwritten versions and eligible tombstones. *Leveled* compaction (LevelDB, RocksDB default) keeps one sorted run per level with each level `T` times (default `T = 10`) larger than the previous, bounding space waste and read cost; *tiered* (size-tiered) compaction (Cassandra's default, HBase) collects several similar-sized runs before merging, cutting write cost but leaving more runs to search.

### The amplification triangle

Every storage engine pays in three currencies (the **RUM conjecture**, Athanassoulis et al.): **R**ead, **U**pdate (write) and **M**emory/space overhead; improving two worsens the third.

- **Write amplification** — bytes physically written per byte the user writes. For a B+ tree, one small update rewrites a whole page (plus the log); for leveled LSM, each entry is rewritten roughly `T` times per level it passes through (many tens of times in a large leveled tree).
- **Read amplification** — blocks consulted per lookup: about `height` for a B+ tree; for an LSM, up to one per run (hence Bloom filters).
- **Space amplification** — bytes stored per live byte: ~1.4 for B+ trees (fill factor); LSM holds obsolete versions until compaction removes them.

The program counts these for a toy LSM: a sorted-array memtable of 16,384 entries, up to 4 overlapping L0 runs, one run per level beyond, size ratio `T`, per-run Bloom filters at 10 bits per key, 10 % of operations are deletes (tombstones), and 4,000,000 random operations on one million keys. Every lookup is verified against a reference table. "Entries written" counts every entry written by a flush or a compaction (a stand-in for bytes written); a "probe" is one run that a lookup actually has to search (a stand-in for a disk read).

```c
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A toy LSM tree that counts what a real one would do on disk.
 *   memtable : sorted in-memory array (MEM entries), flushed as an immutable sorted run ("SSTable")
 *   L0       : up to L0_MAX runs, newest first (runs may overlap)
 *   L1..     : ONE sorted run per level, capacity grows by the size ratio T; when a level overflows it is merged into the next
 * Cost model: every entry written to a run counts as 1 write unit; every run consulted by a lookup (binary search) counts as 1 read probe. */
typedef struct { uint32_t key, val; } Entry;
#define TOMB 0xFFFFFFFFu
#define MEM 16384
#define L0_MAX 4
#define MAXLVL 8

typedef struct { Entry *e; size_t n; uint64_t *bloom; size_t bits; } Run;
typedef struct {
    Entry mem[MEM + 1]; size_t mem_n;
    Run l0[L0_MAX]; int l0_n;                           /* l0[0] is the NEWEST */
    Run lv[MAXLVL];                                     /* lv[i] is level i+1 */
    int T, use_bloom;
    unsigned long long user_writes, entries_written, probes, lookups, bloom_skips;
} LSM;

static inline uint64_t mix64(uint64_t x) { x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL; x ^= x >> 27; x *= 0x94d049bb133111ebULL; x ^= x >> 31; return x; }
static void bloom_add(Run *r, uint32_t key) { for (int i = 0; i < 7; i++) { size_t b = mix64(key + 0x9e3779b97f4a7c15ULL * (uint64_t)(i + 1)) % r->bits; r->bloom[b >> 6] |= 1ull << (b & 63); } }
static int bloom_has(const Run *r, uint32_t key) { for (int i = 0; i < 7; i++) { size_t b = mix64(key + 0x9e3779b97f4a7c15ULL * (uint64_t)(i + 1)) % r->bits; if (!(r->bloom[b >> 6] >> (b & 63) & 1)) return 0; } return 1; }
static void run_finish(Run *r) {                        /* 10 bits per key bloom filter, k = 7 */
    r->bits = r->n * 10 + 64; r->bloom = calloc((r->bits + 63) / 64, 8);
    for (size_t i = 0; i < r->n; i++) bloom_add(r, r->e[i].key);
}
static void run_free(Run *r) { free(r->e); free(r->bloom); *r = (Run){0}; }

/* merge runs (index 0 = newest wins on equal keys); tombstones are dropped only if `bottom` */
static Run merge_runs(LSM *m, Run **in, int cnt, int bottom) {
    size_t total = 0; for (int i = 0; i < cnt; i++) total += in[i]->n;
    Run out = { malloc((total ? total : 1) * sizeof(Entry)), 0, NULL, 0 }; size_t *pos = calloc((size_t)cnt, sizeof *pos);
    for (;;) {
        int best = -1; uint32_t bk = 0;
        for (int i = 0; i < cnt; i++) if (pos[i] < in[i]->n && (best < 0 || in[i]->e[pos[i]].key < bk)) { best = i; bk = in[i]->e[pos[i]].key; }
        if (best < 0) break;
        Entry win = in[best]->e[pos[best]];
        for (int i = 0; i < cnt; i++) if (pos[i] < in[i]->n && in[i]->e[pos[i]].key == bk) pos[i]++;   /* skip older versions */
        if (!(bottom && win.val == TOMB)) out.e[out.n++] = win;
    }
    free(pos); m->entries_written += out.n; run_finish(&out); return out;
}
static int deepest(const LSM *m) { int d = -1; for (int i = 0; i < MAXLVL; i++) if (m->lv[i].n) d = i; return d; }
static size_t level_cap(const LSM *m, int i) { size_t c = (size_t)MEM * L0_MAX; for (int k = 0; k < i; k++) c *= (size_t)m->T; return c; }   /* lv[i] */

static void compact_level(LSM *m, int i);
static void flush(LSM *m) {
    Run r = { malloc(m->mem_n * sizeof(Entry)), m->mem_n, NULL, 0 }; memcpy(r.e, m->mem, m->mem_n * sizeof(Entry)); m->entries_written += r.n; run_finish(&r);
    m->mem_n = 0;
    if (m->l0_n == L0_MAX) {                            /* L0 full: merge all L0 runs (+ level 1) into level 1 */
        Run *in[L0_MAX + 2]; int c = 0; for (int i = 0; i < m->l0_n; i++) in[c++] = &m->l0[i];
        if (m->lv[0].n) in[c++] = &m->lv[0];
        int bottom = deepest(m) <= 0;
        Run out = merge_runs(m, in, c, bottom);
        for (int i = 0; i < m->l0_n; i++) run_free(&m->l0[i]);
        run_free(&m->lv[0]); m->lv[0] = out; m->l0_n = 0;
        compact_level(m, 0);
    }
    memmove(&m->l0[1], &m->l0[0], (size_t)m->l0_n * sizeof(Run)); m->l0[0] = r; m->l0_n++;
}
static void compact_level(LSM *m, int i) {              /* if lv[i] is over capacity, merge it into lv[i+1] */
    while (i + 1 < MAXLVL && m->lv[i].n > level_cap(m, i)) {
        Run *in[2] = { &m->lv[i], &m->lv[i + 1] }; int c = m->lv[i + 1].n ? 2 : 1;
        int bottom = deepest(m) <= i + 1;
        Run out = merge_runs(m, in, c, bottom);
        run_free(&m->lv[i]); run_free(&m->lv[i + 1]); m->lv[i + 1] = out; i++;
    }
}
static size_t mem_lower(const LSM *m, uint32_t key) { size_t lo = 0, hi = m->mem_n; while (lo < hi) { size_t k = (lo + hi) / 2; if (m->mem[k].key < key) lo = k + 1; else hi = k; } return lo; }
static void lsm_put(LSM *m, uint32_t key, uint32_t val) {
    m->user_writes++;
    size_t i = mem_lower(m, key);
    if (i < m->mem_n && m->mem[i].key == key) { m->mem[i].val = val; return; }
    memmove(&m->mem[i + 1], &m->mem[i], (m->mem_n - i) * sizeof(Entry)); m->mem[i] = (Entry){ key, val }; m->mem_n++;
    if (m->mem_n == MEM) flush(m);
}
static int run_get(LSM *m, const Run *r, uint32_t key, uint32_t *val) {
    if (!r->n) return 0;
    if (m->use_bloom && !bloom_has(r, key)) { m->bloom_skips++; return 0; }
    m->probes++;                                        /* we would read a block from this run on disk */
    size_t lo = 0, hi = r->n; while (lo < hi) { size_t k = (lo + hi) / 2; if (r->e[k].key < key) lo = k + 1; else hi = k; }
    if (lo < r->n && r->e[lo].key == key) { *val = r->e[lo].val; return 1; }
    return 0;
}
/* returns 1 if found (value in *val, TOMB means deleted) */
static int lsm_get(LSM *m, uint32_t key, uint32_t *val) {
    m->lookups++;
    size_t i = mem_lower(m, key); if (i < m->mem_n && m->mem[i].key == key) { *val = m->mem[i].val; return 1; }
    for (int r = 0; r < m->l0_n; r++) if (run_get(m, &m->l0[r], key, val)) return 1;
    for (int l = 0; l < MAXLVL; l++) if (run_get(m, &m->lv[l], key, val)) return 1;
    return 0;
}
static size_t stored(const LSM *m) { size_t n = m->mem_n; for (int i = 0; i < m->l0_n; i++) n += m->l0[i].n; for (int i = 0; i < MAXLVL; i++) n += m->lv[i].n; return n; }
static void lsm_free(LSM *m) { for (int i = 0; i < m->l0_n; i++) run_free(&m->l0[i]); for (int i = 0; i < MAXLVL; i++) run_free(&m->lv[i]); }

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    enum { KEYS = 1000000, OPS = 4000000 };
    static uint32_t ref[KEYS];                           /* reference: last value per key, TOMB = absent/deleted, 0xFFFFFFFE = never written */
    printf("%2s %6s | %10s %12s %10s | %10s %10s | %10s %10s\n", "T", "levels", "write amp", "space amp", "stored", "hit probes", "miss probes", "(bloom) hit", "(bloom) miss");
    int Ts[] = { 4, 10 };
    for (int ti = 0; ti < 2; ti++) {
        LSM *m = calloc(1, sizeof *m); m->T = Ts[ti];
        for (int k = 0; k < KEYS; k++) ref[k] = 0xFFFFFFFEu;
        for (long i = 0; i < OPS; i++) {
            uint32_t k = rnd() % KEYS;
            if (rnd() % 10 == 0) { lsm_put(m, k, TOMB); ref[k] = TOMB; } else { uint32_t v = rnd() & 0x7FFFFFFF; lsm_put(m, k, v); ref[k] = v; }
        }
        size_t live = 0; for (int k = 0; k < KEYS; k++) live += ref[k] != 0xFFFFFFFEu && ref[k] != TOMB;
        int levels = deepest(m) + 2;
        double wamp = (double)m->entries_written / (double)m->user_writes, samp = (double)stored(m) / (double)live;
        /* correctness + read cost, with and without bloom filters; hits = keys ever written & alive, misses = keys outside the range */
        double hp[2], mp[2];
        for (int bl = 0; bl < 2; bl++) {
            m->use_bloom = bl; unsigned long long p0, l0; int hits = 0, misses = 0;
            p0 = m->probes; l0 = m->lookups;
            for (int q = 0; q < 200000; q++) {
                uint32_t k = rnd() % KEYS, v; int f = lsm_get(m, k, &v);
                int exp_live = ref[k] != 0xFFFFFFFEu && ref[k] != TOMB;
                assert((f && v != TOMB) == exp_live); if (exp_live) { assert(v == ref[k]); hits++; }
            }
            hp[bl] = (double)(m->probes - p0) / (double)(m->lookups - l0);
            p0 = m->probes; l0 = m->lookups;
            for (int q = 0; q < 200000; q++) { uint32_t v; assert(!lsm_get(m, KEYS + (rnd() % KEYS), &v)); misses++; }
            mp[bl] = (double)(m->probes - p0) / (double)(m->lookups - l0);
            (void)hits; (void)misses;
        }
        printf("%2d %6d | %10.1f %12.2f %10zu | %10.2f %10.2f | %10.2f %10.2f\n", m->T, levels, wamp, samp, stored(m), hp[0], mp[0], hp[1], mp[1]);
        lsm_free(m); free(m);
    }
    printf("\nwrite amp = entries written by flushes and compactions / user writes; space amp = entries stored / live keys;\n");
    printf("probes = runs actually searched per lookup (each would be at least one disk read); %d keys, %d operations (10%% deletes)\n", KEYS, OPS);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra lsm.c -o lsm && ./lsm
 T levels |  write amp    space amp     stored | hit probes miss probes | (bloom) hit (bloom) miss
 4      4 |        5.9         1.04     917413 |       2.95       3.00 |       0.90       0.02
10      4 |        6.2         1.30    1144621 |       3.69       4.00 |       0.93       0.03

write amp = entries written by flushes and compactions / user writes; space amp = entries stored / live keys;
probes = runs actually searched per lookup (each would be at least one disk read); 1000000 keys, 4000000 operations (10% deletes)
```

How to read it:

- **Write amplification is about 6** for this tiny dataset (just a few levels): each user write is physically written about six times over its life. It grows with dataset size — roughly with the number of levels times `T` — and in a multi-terabyte leveled RocksDB instance values of 10–30 are common. This is the price of sequential-only writes and is why LSMs wear SSDs faster for update-heavy workloads.
- **Without Bloom filters** a lookup probes about 3–4 runs, even for keys that do not exist at all (misses must check every level).
- **With Bloom filters** lookups for existing keys drop to about 1 probe and **lookups for absent keys to ~0.02–0.03 probes** — the filter's false-positive rate multiplied by the number of runs. This is the most valuable trick in the whole design.
- **Space amplification** exceeds 1 because overwritten values and tombstones persist until compacted; in this run the larger ratio `T = 10` gave 1.3 against 1.04 for `T = 4` — a bigger ratio lets the upper levels hold more obsolete versions between compactions.
- The toy omits things production engines do: a WAL, block-structured SSTables with sparse indexes, compaction by key range instead of whole levels (so each compaction is bounded), rate limiting of compaction, and snapshots/versions.

## B+ tree vs LSM

| | B+ tree | LSM tree |
|---|---|---|
| Write path | in-place page update (random I/O), WAL | append + background merge (sequential I/O) |
| Point read | `height` page reads, ~2–3 for huge tables | up to one read per run, mitigated by Bloom filters |
| Range scan | follow leaf chain — excellent | merge across runs — good, more work |
| Write amplification | moderate (page + log) | high (rewritten through levels) |
| Space amplification | ~1.4 (fill factor) | 1.1–1.3 leveled; higher for tiered |
| Latency predictability | stable | compaction stalls can cause spikes |
| Typical systems | InnoDB, PostgreSQL, SQLite, LMDB, btrfs | LevelDB, RocksDB, Cassandra, HBase, ScyllaDB |

The choice follows the workload: read-heavy, point-and-range queries on data that fits the page cache → B+ tree; write-heavy ingestion, time series, logs, key-value stores on SSD → LSM. Hybrids exist (B-epsilon trees buffer updates inside internal nodes; Bw-trees; fractal trees; "Monkey"-style Bloom filter allocation across levels).

## Pitfalls

- **Wrong node size.** Matching the node to the device block is the whole point; a B-tree with 100-byte nodes is just a slow binary tree.
- **Variable-length keys without truncation** shrink fan-out; use prefix compression.
- **Treating the leaf chain as free under concurrency.** Scans that read pages while splits happen need B-link-style protocols or snapshot isolation.
- **Unbounded compaction debt** in LSMs: if writes outpace compaction, L0 run count grows and read latency explodes; engines apply write stalls for this reason.
- **Tombstone accumulation**: deleting many keys and then range-scanning the region reads all the tombstones until they compact away.
- **Benchmarking in memory.** A B+ tree that fits entirely in RAM says nothing about I/O behavior; measure with realistic data sizes and `O_DIRECT` or cold caches.

## Key takeaways

1. External-memory data structures minimize block transfers: fan-out ≈ block size / entry size gives `log_B n` height — 4–5 levels for a billion keys.
2. B+ trees keep values in leaves and chain them, giving high fan-out and fast range scans; random insertion leaves nodes ~70 % full.
3. Balance in a B-tree comes from splitting and merging at the root, not from rotations, and all leaves share one depth.
4. LSM trees convert random writes into sequential ones via a memtable, immutable sorted runs and background compaction; they pay in write amplification and read complexity.
5. Bloom filters make LSM point lookups cheap — a miss costs a few hundredths of a disk read rather than one read per level.
6. Read, write and space amplification trade against each other; pick the structure from the workload and measure at realistic scale.
