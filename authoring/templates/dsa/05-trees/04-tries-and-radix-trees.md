---
title: Tries, Radix Trees and Longest-Prefix Match
track: dsa
module: Trees
moduleOrder: 5
order: 4
level: advanced
summary: "Prefix trees from the 26-way trie to compressed radix trees, then the structure that justifies them in networking — IPv4 longest-prefix match with binary, multibit and DIR-24-8 tables, checked against a linear scan and measured."
sources:
  - label: Fredkin — Trie Memory (CACM 3(9), 1960)
  - label: Morrison — PATRICIA, Practical Algorithm To Retrieve Information Coded in Alphanumeric (J. ACM 15(4), 1968)
  - label: Gupta, Lin, McKeown — Routing Lookups in Hardware at Memory Access Speeds (IEEE INFOCOM 1998)
  - label: Nilsson & Karlsson — IP-Address Lookup Using LC-Tries (IEEE JSAC 17(6), 1999)
  - label: Eatherton, Varghese, Dittia — Tree Bitmap, Hardware/Software IP Lookups with Incremental Updates (ACM SIGCOMM CCR 34(2), 2004)
  - label: Leis, Kemper, Neumann — The Adaptive Radix Tree, ARTful Indexing for Main-Memory Databases (ICDE 2013)
  - label: RFC 4632 — Classless Inter-domain Routing (CIDR), the Internet Address Assignment and Aggregation Plan
    url: https://www.rfc-editor.org/rfc/rfc4632
  - label: Linux kernel documentation — XArray
    url: https://www.kernel.org/doc/html/latest/core-api/xarray.html
  - label: DPDK Programmer's Guide — LPM Library
    url: https://doc.dpdk.org/guides/prog_guide/lpm_lib.html
---

A **trie** (from re*trie*val, usually pronounced "try") indexes keys by their *symbols* instead of by comparing whole keys: the path from the root spells the key, so lookup cost is `O(k)` in the key length `k`, independent of how many keys are stored. That makes tries the natural structure whenever the question is about **prefixes** — autocomplete, dictionary matching, routing tables, URL routers, IP access lists — and whenever the keys are bit strings, as in IP addresses.

## The design space

Every trie answers one question: how do you store the children of a node?

| Node representation | Child lookup | Memory per node | Notes |
|---|---|---|---|
| Array of `σ` pointers (σ = alphabet size) | O(1) | `8σ` bytes | Fast; wasteful for sparse nodes (26 letters → 208 B, bytes → 2 KB) |
| Linked list of children (first-child / next-sibling) | O(σ) | ~24 B | Compact; slow for wide nodes |
| Sorted array + binary search | O(log σ) | proportional to children | Good for medium fan-out |
| Bitmap + packed array (HAMT, Tree Bitmap, poptrie) | O(1) with popcount | 8σ bits + 8 per child | Dense-fast and sparse-compact |
| Hash table per node | O(1) expected | high constant | Rarely worthwhile |
| Adaptive node types (ART: 4, 16, 48, 256 children) | O(1) | adapts to fan-out | Best general in-memory design |

And one orthogonal choice: **path compression**. A chain of nodes with a single child wastes a node per symbol; a **radix tree** (Patricia trie) stores a whole substring on each edge, so a tree with `n` keys has at most `2n−1` nodes no matter how long the keys are. Compression complicates insertion (an edge may need to be *split*) but is what makes tries practical for long, sparse keys.

## Words: four ways to answer "complete this prefix"

The program builds three trie variants and the obvious alternative, a sorted array searched with binary search, on a deterministic synthetic vocabulary (syllable combinations, so these are *not* English words), inserts the words in scrambled order, and checks that for 5000 random prefixes all four structures return the same first ten completions in alphabetical order.

{{include dsa/tries/words.c}}

{{run dsa/tries :: gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined words.c -o words_asan && ./words_asan | head -3 && gcc -std=gnu11 -O2 -Wall -Wextra words.c -o words && ./words}}

(The first run is under AddressSanitizer/UBSan to validate memory handling; the second is the timed build.) How to read the table:

- The **26-way array trie** needs a 208-byte node for every character position: tens of megabytes for a vocabulary that fits in about a megabyte. This is the standard textbook trie and the standard warning: it is the *least* memory-efficient option.
- The **first-child/next-sibling** version stores each node in 24 bytes and is about an order of magnitude smaller, at the cost of scanning siblings.
- **Path compression** roughly halves the node count here (the vocabulary was built from shared syllables, so the saving is moderate — for sparse keys like URLs or UUID strings it is far larger), though each node now also owns a small label allocation.
- The **sorted array with binary search** is the honest competitor, and in this experiment it is the smallest *and* about as fast or faster. A prefix is a contiguous range in sorted order, so `lower_bound` plus a linear scan gives completions with no pointer chasing. This is a recurring theme: **a trie wins when keys are updated dynamically, when you need O(k) independent of `n`, or when the key is a bit string with a fixed structure — not automatically for text lookup.** Production autocomplete systems mostly use sorted arrays, FSTs (finite-state transducers, which are tries with shared suffixes too) or compressed tries, never the 26-array trie.

## IP routing: longest-prefix match

IP forwarding needs, for a destination address, the route with the **longest matching prefix** (CIDR, RFC 4632): a table can hold both `10.0.0.0/8` and `10.1.2.0/24`, and the more specific one wins. See the *Forwarding: Routing Table, Longest-Prefix Match, Policy Routing and ECMP* note in the IPv4 module for how the kernel and routers use it. At line rate the lookup has a hard budget: a 100 Gb/s link carries up to ~150 million minimum-size packets per second, about 7 ns per packet per port. The full IPv4 Internet table is on the order of a million prefixes. This is the textbook case for a trie, with a bit string for a key.

Four implementations, all verified against each other:

1. **Linear scan** — the specification: check every route, keep the longest match. Used as the oracle in the test.
2. **Binary trie** — one bit per level, up to 32 *dependent* memory loads per lookup. Updates are trivial.
3. **Multibit trie, stride 8** — consume a byte per level, so at most 4 loads. A prefix whose length is not a multiple of 8 is **expanded**: `10.0.0.0/10` becomes 64 entries at the second level. Insert in ascending length so a longer prefix overwrites the expansion of a shorter one.
4. **DIR-24-8** (Gupta, Lin & McKeown 1998) — a flat table of 2²⁴ entries indexed directly by the top 24 bits of the address, plus 256-entry second-level groups for the rare prefixes longer than /24. One memory access for the lookups that match a /24 or shorter prefix (the large majority), a second for the rest. It is the structure behind DPDK's `rte_lpm`, which software routers and virtual switches use.

{{include dsa/tries/lpm.c}}

{{run dsa/tries :: gcc -std=gnu11 -O2 -Wall -Wextra lpm.c -o lpm && ./lpm}}

The synthetic table has about 200,000 unique prefixes with a length distribution resembling the real Internet (more than half are /24; a few are longer than /24; one default route). It is *not* real BGP data. The key results:

- **All three structures agree with the linear scan** on 20,000 destinations, 80 % of which fall inside a route and 20 % of which are uniformly random.
- The **linear scan costs about 130 µs per lookup** — it examines every prefix. This is also what a naive routing table costs, and why tables were tries from the start.
- The **binary trie is a few hundred nanoseconds**, bounded by up to 32 dependent loads.
- The **stride-8 trie is several times faster** (4 loads) but its 256-pointer nodes make it by far the **largest** structure — a 256× fan-out for nodes that usually have a handful of children.
- **DIR-24-8 is the fastest, around 20 ns per lookup, using a flat 32 MB table** (16-bit entries) plus the second-level groups. The cost is memory and update time: a /8 route rewrites 65,536 entries. That is acceptable for a table that changes a few times per second, and why such tables are rebuilt or patched in the control plane while the data plane reads them lock-free.

### What real implementations do

- **Linux IPv4 FIB** uses a **LC-trie** (level- and path-compressed trie, Nilsson & Karlsson) in `net/ipv4/fib_trie.c`: path compression plus *level compression* that replaces a dense subtree of height `h` with one node of `2^h` children, resizing dynamically. Lookups are protected by RCU, so forwarding does not take locks. (The routing cache that preceded it was removed in 3.6.)
- **DPDK `rte_lpm`** implements DIR-24-8 as above. For IPv6 the address is 128 bits, a direct table is impossible, and `rte_lpm6` uses a chain of 8-bit-stride tables.
- **Hardware routers** store compressed multibit tries (Tree Bitmap) or use TCAM, which matches all prefixes in parallel. **Poptrie** is a modern software design that use bitmaps and popcount to get both compactness and a few memory accesses.
- The **Linux `radix_tree` / XArray** (the kernel's page-cache index, an array indexed by file offset with 64-way nodes, 6 bits per level) and **ART** in main-memory databases follow the same idea for integer keys.

## Other prefix-structure uses

- **IP access lists and firewalls**: each rule is a prefix or a range; ranges convert into a handful of prefixes, and multi-field classification is built from several tries (cross-producting, HiCuts, bit-vector schemes — or, in Linux, nftables sets implemented as interval tree or `rbtree`/`pipapo` sets).
- **Aho–Corasick** (strings note): a trie with failure links, matching many patterns in one pass.
- **URL and HTTP routers**: a compressed trie over path segments, often with parameter nodes.
- **Spell-check, T9, autocomplete**: tries or FSTs over the dictionary.
- **XOR tries (Kademlia DHT)**: peers are organized by shared bit prefix of their IDs.
- **Bitwise tries for integer sets**: van Emde Boas trees and y-fast tries answer predecessor queries in `O(log log U)`.

## Pitfalls

- **Pointer fan-out waste.** Choose the node representation from the alphabet and the expected density; do not default to `T *child[256]`.
- **Memory is the real cost.** Count bytes per key, including allocator overhead — small mallocs round up and carry headers. A pool or arena allocator, and 32-bit indices instead of 64-bit pointers (as the binary trie above uses), often halve the footprint.
- **Insertion order for expanded prefixes.** In expansion schemes (multibit, DIR-24-8), insert short before long — or track prefix lengths per entry — or a short prefix will overwrite a longer one. Deletion needs per-entry prefix lengths (or a rebuild) to restore the next-longest match.
- **Unicode.** A trie over bytes of UTF-8 works and sorts correctly in code point order; a trie over "characters" needs a decision about normalization.
- **Concurrency.** Readers walking a trie while a writer splits an edge need RCU, atomic pointer publication, or copy-on-write; Morris-style in-place tricks are unsafe.
- **Worst-case depth.** The depth equals the key length; adversarial or very long keys (URLs, paths) need a compressed trie and length limits.

## Key takeaways

1. A trie's cost depends on the key length, not on the number of keys, and it makes prefix operations natural.
2. The child representation and path compression decide whether it is practical: the 26-way array trie is simple but memory-hungry; radix trees, bitmaps and adaptive nodes fix that.
3. A sorted array with binary search is a strong baseline for static string sets; tries win when keys change, for bit strings, and for prefix-keyed lookups such as routing.
4. Longest-prefix match is the canonical use: linear scan ~130 µs, binary trie hundreds of ns, a flat DIR-24-8 table ~20 ns — memory and update cost are the price.
5. Linux uses an LC-trie for the IPv4 FIB and radix-tree-style XArrays for the page cache; DPDK uses DIR-24-8.
6. Insert prefixes short-to-long in expansion schemes, and verify every fast structure against a slow linear oracle.
