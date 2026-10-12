---
title: Allocators as Data Structures
track: dsa
module: Systems-Flavored Structures
moduleOrder: 9
order: 2
level: expert
summary: "Free lists, boundary tags and coalescing, first/next/best fit compared on a random workload with a heap checker, the buddy allocator behind the Linux page allocator, and arena and pool allocators that beat malloc by 10x — all tested for overlap and corruption."
sources:
  - label: Knuth — The Art of Computer Programming, Vol. 1, §2.5 (Dynamic Storage Allocation)
  - label: Wilson, Johnstone, Neely, Boles — Dynamic Storage Allocation, A Survey and Critical Review (IWMM 1995)
  - label: Johnstone & Wilson — The Memory Fragmentation Problem, Solved? (ISMM 1998)
  - label: Knowlton — A Fast Storage Allocator (CACM 8(10), 1965) (the buddy system)
  - label: Bonwick — The Slab Allocator, An Object-Caching Kernel Memory Allocator (USENIX Summer 1994)
  - label: Bryant & O'Hallaron — Computer Systems, A Programmer's Perspective, Ch. 9.9 (Dynamic Memory Allocation)
  - label: Linux kernel documentation — Memory allocation guide and the buddy allocator
    url: https://www.kernel.org/doc/html/latest/core-api/memory-allocation.html
  - label: glibc malloc internals (MallocInternals wiki)
    url: https://sourceware.org/glibc/wiki/MallocInternals
---

`malloc` and `free` look like a simple pair of functions, but behind them is a data structure that must manage a contiguous region of memory under an unpredictable stream of variable-sized requests — with no ability to move allocated blocks (C programs hold raw pointers). The *Dynamic Memory: malloc Internals, Classic Bugs and Custom Allocators* note in the C track covers using and debugging the allocator; this note builds allocators from scratch and measures what the data-structure choices cost.

## What an allocator must do

- `alloc(n)` returns a block of at least `n` bytes, suitably aligned (16 bytes on x86-64), in **O(1)-ish** time.
- `free(p)` returns it, **without being told its size** — so the allocator must find the size from `p` alone (a header, or an address-based table).
- It must avoid **overlap**, **corruption of metadata** by users, and ideally **fragmentation**:
  - *Internal* fragmentation: space wasted inside allocated blocks (rounding, headers).
  - *External* fragmentation: enough free memory in total, but split into pieces too small for a request.

No allocator avoids fragmentation for every workload — Robson (1971) proved that worst-case behavior is bad for any policy — so the design is about trade-offs, and about matching the allocator to the workload.

## General-purpose: boundary tags, free lists and placement policy

The classic design (Knuth, 2.5; Bryant & O'Hallaron 9.9) gives every block a **header** and **footer** holding its size and an allocated bit. Free blocks are additionally linked on an explicit **free list**, stored in the block's own (otherwise unused) payload. The footer is what makes `free` fast: the block just below any block's header is its predecessor's footer, so a freed block can check **both neighbours** in `O(1)` and **coalesce** with free ones, preventing the heap from dissolving into crumbs.

On allocation the free list is searched by a **placement policy**, then the chosen block is *split* if the remainder is large enough to be a block itself:

- **First fit**: take the first block that is big enough. Fast when the list is short, but small leftovers accumulate at the front.
- **Next fit**: like first fit but resume from where the last search ended (a "rover"). Faster, historically thought worse for fragmentation.
- **Best fit**: scan for the smallest block that is big enough. Slowest search, but leaves the biggest leftovers intact.

{{include dsa/alloc/fit.h}}

The `heap_check` function is as important as the allocator: it walks every block by size, verifies header equals footer, that no two free blocks are adjacent (they would have been coalesced), and that the free list links and sums agree with the walk. Allocator bugs corrupt state far from where they occur, and a checker run periodically is the difference between a day and a month of debugging.

{{include dsa/alloc/test_fit.c}}

{{run dsa/alloc :: gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined test_fit.c -o test_fit -lm && ./test_fit}}

The workload: three million random operations on a 64 MiB heap, sizes log-uniform from 16 bytes to 16 KiB, with the live data steered towards 40 MiB. Every block is filled with a random tag when allocated and verified when freed (so overlapping or corrupted blocks are detected), and the checker validates the heap periodically and verifies that after everything is freed the heap collapses back into one block.

What the table shows (for this workload; allocator rankings are workload-dependent):

- **First fit with a LIFO free list fragmented badly**: more than 100,000 allocations failed even though about 23 MB was free — the largest free block was only ~10 KB. External fragmentation made free memory unusable. Inserting freed blocks at the list head scatters small blocks at the front where every search must step over them (900+ steps per allocation).
- **Next fit** skips most of that scanning (about 100 steps) but still fails several hundred requests: same free space, shredded into small pieces.
- **Best fit failed nothing**: its free memory was concentrated into a few large blocks (the largest free block was ~21 MB). Its search cost is high (the whole list unless it finds an exact fit) — which is why real allocators replace the single list with **segregated free lists** (one per size class) and bins, getting best-fit-like quality in near-constant time.
- Every policy coalesced back to a single block after the final frees: the footers did their job.

Production designs refine this: glibc's `ptmalloc` keeps *fastbins* and *tcache* (per-thread caches of recently freed small chunks), small bins, large bins sorted by size, and an unsorted bin; `jemalloc` and `tcmalloc` use size classes with thread-local caches and "slabs" per class; Go and Java use compacting or generational collectors, which can *move* objects to defragment — impossible in C.

## The buddy system

A **binary buddy** allocator manages a region of size `2^N` by recursively halving. Every block has a power-of-two size, and each block of size `2^k` at offset `o` has exactly one **buddy** at `o XOR 2^k`. Allocation rounds the request up to a power of two, takes a free block of that order (splitting a larger one if needed, freeing each upper half), and free merges a block with its buddy whenever the buddy is free *and the same size*, repeating up. Both operations take `O(log N)` and need no search; coalescing is address arithmetic.

{{include dsa/alloc/buddy.c}}

{{run dsa/alloc :: gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined buddy.c -o buddy && ./buddy}}

The workload mixes small and medium requests. The measured **internal fragmentation** is about a third of granted space wasted on rounding up to the next power of two — the buddy system's price (a rule of thumb is about 25 % for sizes spread evenly within each power-of-two band; this mixed workload measured 33 %, and the figure depends entirely on where request sizes fall relative to the powers of two). In return it is fast and coalesces perfectly: after every block is freed the whole 16 MiB region is a single block again, verified. This is how the **Linux page allocator** manages physical memory: free pages are kept in per-order lists (order 0 = one 4 KiB page up to order 10 = 4 MiB), `/proc/buddyinfo` shows the free block count per order, and external fragmentation is visible there as memory sitting only in the low orders (huge-page and DMA allocations then fail). For small kernel objects the **slab allocator** (SLUB) sits on top, carving pages into per-type object caches — see the next idea.

## Special-purpose allocators: arena and pool

When the allocation pattern is known, much simpler structures beat `malloc` outright.

- **Arena (bump) allocator**: allocation is `ptr += size`; there is no individual `free` — the whole arena is released (or reset) at once. Perfect for data with a common lifetime: a request's temporary objects, a parse tree, a compiler pass. It allocates in a few instructions, has zero per-object overhead and gives *excellent locality* because consecutive allocations are adjacent.
- **Pool (free-list / slab) allocator**: for objects of one size, freed objects form a singly linked list threaded through their own memory; allocate and free are a pointer swap. Kernel slab caches (`kmem_cache`), network buffer pools (`sk_buff`) and game entity systems use this; **Bonwick's slab** also keeps objects *constructed* between uses.

{{include dsa/alloc/pool_arena.c}}

{{run dsa/alloc :: gcc -std=gnu11 -O2 -Wall -Wextra pool_arena.c -o pool_arena && ./pool_arena}}

- Building a linked list of five million 32-byte nodes with the arena is more than ten times faster than `malloc`; **releasing it costs nothing** (one pointer reset) versus tens of milliseconds of `free` calls. Even *walking* the arena-built list is faster, because the nodes are contiguous and in allocation order.
- In a steady-state churn of free-and-reallocate, the pool allocator is roughly ten times faster than `malloc`/`free` per pair (single digits of nanoseconds against tens): no size-class lookup, no locks, no coalescing, no metadata.
- The trade-off: arenas can't free individual objects (memory use grows until reset) and pools only hold one size; use them where the structure of the problem allows.

## Allocator choice cheat sheet

| Need | Use |
|---|---|
| General purpose, mixed sizes, multithreaded | the platform's `malloc` (ptmalloc), or jemalloc / tcmalloc / mimalloc for heavy multithreaded workloads |
| Many same-sized objects, frequent free | pool / slab / free list |
| Data with a shared lifetime | arena / region / bump allocator |
| Power-of-two blocks, physical pages, simple fast coalescing | buddy system |
| Real-time with bounded latency | pre-allocated pools; TLSF (two-level segregated fit, O(1) worst case) |
| Safety debugging | AddressSanitizer, Valgrind, `MALLOC_CHECK_`, guard pages (Electric Fence style) |

## Pitfalls

- **Alignment**: returned pointers must satisfy the strictest alignment of any type (16 on x86-64 for SSE types); this implementation rounds every block to 16.
- **Header/footer overhead** (16 bytes per block here) dominates for small objects; pools avoid it.
- **Splitting thresholds**: a remainder smaller than the minimum block size (list links + header + footer) cannot become a free block, so it is handed to the allocation (internal fragmentation).
- **Use-after-free and double free** corrupt the free list itself (the links live in freed payload): the classic exploit surface. Hardened allocators check list integrity and randomize or quarantine.
- **Thread safety**: one global lock serialises everything; modern allocators use per-thread caches. Cross-thread frees (allocated in one thread, freed in another) are the hard case.
- **Fragmentation is invisible in the usual metrics**: track free-block counts and the largest free block, not just bytes in use.
- **Not zeroing recycled memory** leaks secrets between uses; `calloc` or explicit clearing is required for sensitive data.
- **Over-trusting benchmarks**: allocation speed in a microbenchmark says little about fragmentation or cache behavior in a long-running service.

## Key takeaways

1. An allocator is a data structure over a byte region: block headers and footers give O(1) size lookup and neighbour coalescing; explicit free lists give reuse.
2. Placement policy changes fragmentation dramatically: on the test workload first fit failed over 100,000 requests with 23 MB free, while best fit failed none.
3. A heap consistency checker plus a payload-tag overlap test finds allocator bugs where they happen, not where they crash.
4. The buddy system trades about a third of memory to internal fragmentation for O(log N) allocation and perfect coalescing; it manages Linux's physical pages.
5. Arenas and pools beat `malloc` by roughly 10x when lifetimes or sizes are uniform — at the cost of generality.
6. Choose the allocator to match the allocation pattern; measure fragmentation, not just speed.
