---
title: B-Trees, B+ Trees and LSM Trees
track: dsa
module: Trees
moduleOrder: 5
order: 5
level: advanced
summary: "Search trees built for block storage — why wide nodes win, a tested B+ tree measured across node sizes, page-size arithmetic for billions of keys, and a counting LSM tree that shows write, read and space amplification and what Bloom filters buy."
sources:
  - label: Bayer & McCreight — Organization and Maintenance of Large Ordered Indexes (Acta Informatica 1(3), 1972)
  - label: Comer — The Ubiquitous B-Tree (ACM Computing Surveys 11(2), 1979)
  - label: Lehman & Yao — Efficient Locking for Concurrent Operations on B-Trees (ACM TODS 6(4), 1981)
  - label: Graefe — Modern B-Tree Techniques (Foundations and Trends in Databases 3(4), 2011)
  - label: O'Neil, Cheng, Gawlick, O'Neil — The Log-Structured Merge-Tree (LSM-Tree) (Acta Informatica 33(4), 1996)
  - label: Athanassoulis et al. — Designing Access Methods, the RUM Conjecture (EDBT 2016)
  - label: Dayan, Athanassoulis, Idreos — Monkey, Optimal Navigable Key-Value Store (SIGMOD 2017)
  - label: Aggarwal & Vitter — The Input/Output Complexity of Sorting and Related Problems (CACM 31(9), 1988)
  - label: RocksDB Wiki — Leveled Compaction
    url: https://github.com/facebook/rocksdb/wiki/Leveled-Compaction
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 18 (B-Trees)
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

{{include dsa/btree/bplus.c}}

{{run dsa/btree :: gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined bplus.c -o bplus_asan && ./bplus_asan | head -2 && gcc -std=gnu11 -O2 -Wall -Wextra bplus.c -o bplus && ./bplus}}

Reading the table (the first two lines come from the sanitizer build; the timed table is the optimized build):

- **Height falls as node size grows**: from 12 levels at 4 keys per node to 3 levels at 256 or more — a binary tree on two million keys is at least 21 levels deep.
- **Leaf fill is about 70 %**, consistent with the classic result that random insertions into a B-tree leave nodes about `ln 2 ≈ 69 %` full on average. A B+ tree therefore uses ~1.4× the space of a perfectly packed array; sequentially loaded trees (bulk loading) can be packed to ~100 %.
- **Lookups and inserts get faster with wider nodes up to a point**, then flatten or get slightly worse: once the node is large, a binary search inside a node costs more cache lines than the levels saved. For disk-based indexes the node size is the page size; for in-memory ones a few cache lines to a few KB is typical.
- **Range scans improve dramatically** (roughly 50 ns/key at 4-key nodes down to 2–3 ns/key for wide leaves), because consecutive keys are contiguous; this is the B+ tree's defining advantage over a binary tree and over hash indexes.
- The **sorted array** is still the fastest for lookups — it has no pointers — but it cannot be updated cheaply: that is the trade a B+ tree makes.

### Page-size arithmetic

How many levels does a database index need? With 8-byte child pointers and a 32-byte page header, an internal node of `P` bytes holds about `P / (key + 8)` entries.

{{include dsa/btree/pagemath.py}}

{{run dsa/btree :: python3 pagemath.py}}

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

{{include dsa/btree/lsm.c}}

{{run dsa/btree :: gcc -std=gnu11 -O2 -Wall -Wextra lsm.c -o lsm && ./lsm}}

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
