---
title: Dynamic Arrays
track: dsa
module: Linear Structures
moduleOrder: 2
order: 1
level: intermediate
summary: The workhorse data structure — contiguous storage with geometric growth. A generic, differentially tested C implementation, the amortized-cost argument, iterator invalidation, and measurements showing why arrays beat linked lists by 20–200× on real hardware.
prerequisites:
  - Complexity Analysis and Amortized Analysis
  - Pointers for Beginners
sources:
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., §16.4 (Dynamic tables)
  - label: Stroustrup — Are lists evil? (and Bjarne Stroustrup's keynote "Software Development for Infrastructure", GoingNative 2012)
    url: https://isocpp.org/blog/2014/06/stroustrup-lists
  - label: Facebook folly — FBVector (growth factor analysis), "folly/docs/FBVector.md"
    url: https://github.com/facebook/folly/blob/main/folly/docs/FBVector.md
  - label: Drepper — What Every Programmer Should Know About Memory, §6 (Optimizing for locality)
  - label: man 3 realloc, man 3 reallocarray; glibc malloc internals (mremap for large blocks)
    url: https://man7.org/linux/man-pages/man3/realloc.3.html
  - label: Linux kernel — include/linux/vmalloc.h, lib/flex_array (historical) and kvmalloc discussion
---

A **dynamic array** (vector, ArrayList, `std::vector`, Python `list`, Go slice) stores elements contiguously in a heap block and grows by reallocating when full. It offers O(1) indexing, O(1) amortized append, excellent cache behavior and the lowest memory overhead of any general-purpose sequence. It is the right default container in almost every program; the interesting question is when something else beats it.

## Representation and operations

```text
Vec { data ─────► [ e0 | e1 | e2 | e3 | … | e(len-1) | (unused capacity) ]
      len, cap, elem_size }
```

| Operation | Cost | Notes |
|---|---|---|
| `a[i]` (index) | O(1) | `base + i·elem_size`; one multiply-add, bounds check optional |
| push back | O(1) amortized, O(n) worst case | resize when `len == cap` |
| pop back | O(1) | decrement `len` |
| insert / erase at position `i` | O(n − i) | `memmove` of the tail |
| insert / erase at the front | O(n) | the worst position |
| search (unsorted) | O(n) | sequential scan, cache-friendly |
| search (sorted) | O(log n) | binary search |
| iterate | O(n) | prefetcher-friendly |
| size / capacity | O(1) | |

The capacity is separate from the length: **`reserve(n)`** allocates once when the final size is known (eliminating all copies); **`shrink_to_fit`** returns unused memory after a large shrink.

## A generic, tested implementation

The C implementation stores `elem_size` so one set of functions serves any type, using `memcpy`/`memmove` for elements (this requires elements to be trivially relocatable — plain data, no self-pointers). Key points are commented in the code: overflow-checked doubling, byte-size overflow check before `realloc`, keeping the old block valid if `realloc` fails, and an asserted index check in `vec_at`:

{{include dsa/arrays/vec.h}}

It is validated with a **differential test**: 200,000 random operations (push, pop, insert at a random index, order-preserving remove, swap-remove, periodic shrink) are applied to both `Vec` and a trivially correct fixed array, comparing the full contents after every step, under AddressSanitizer and UBSan:

{{include dsa/arrays/test_vec.c}}

{{run dsa/arrays :: gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined test_vec.c -o test_vec && ./test_vec}}

(The final line prints the number of successful operations; any mismatch, out-of-bounds access, leak or overflow would have aborted.) This pattern — random operations against a simple model — is the most efficient way to test any data structure.

### Design decisions

- **Growth factor.** Geometric growth (×2 or ×1.5) gives amortized O(1) appends; adding a constant gives O(n²) total (measured in the complexity note: one million appends cost 1.05 copies each with ×2, 2.1 with ×1.5 and 31,250 with +16). Factor 2 minimizes copies but can never reuse freed memory (the sum of all previous blocks is always smaller than the next request), while factors ≤ φ ≈ 1.618 allow the allocator to coalesce and reuse old blocks — one reason Facebook's `fbvector` uses 1.5 and why MSVC uses 1.5 while GCC's libstdc++ uses 2. With a modern allocator (`realloc` using `mremap` for big blocks) the difference narrows.
- **Pointer invalidation.** `realloc` may move the block: every pointer, reference or iterator into the array is invalid after any push/insert. Use indices instead of pointers for long-lived references, or reserve capacity first. This is the single most common dynamic-array bug (storing `&v[3]` and then pushing).
- **Failure handling.** Check `realloc`'s result through a temporary, leave the container unchanged on failure, and detect size overflow (`cap * elem_size` can wrap).
- **Order-preserving vs. unordered removal.** `vec_remove` shifts the tail (O(n)); if order doesn't matter, `vec_swap_remove` overwrites the hole with the last element in O(1) — typical for entity lists, particle systems and worklists.
- **Small-buffer optimization.** For vectors that are usually tiny, embed a few elements inline in the struct and only use the heap beyond that (LLVM `SmallVector`, many string implementations).
- **Shrinking.** Most implementations never shrink automatically (avoid thrashing at the boundary); some shrink when `len < cap/4`, giving amortized bounds in both directions.
- **Zeroing/secrets.** Clear (`explicit_bzero`) before freeing if elements are sensitive.
- **Alternatives for element types that are not trivially relocatable** (C++ objects with self-references): move constructors, or a deque/block list.

## Why arrays win: measured

Appending 2 million ints to a vector and to a singly linked list with one `malloc` per node, then summing both (`bench.c`):

{{include dsa/arrays/bench.c}}

{{run dsa/arrays :: gcc -std=gnu11 -O2 -Wall -Wextra bench.c -o bench && ./bench}}

- **Append**: the array is >10× faster — no per-element `malloc` (≈ 50 ns each plus 16 bytes of allocator overhead), amortized memcpy instead.
- **Traversal, best case for the list** (nodes allocated back to back, so they sit in memory in list order): the array is still several times faster (compact data: 4 bytes per element instead of 32, and SIMD-friendly loops).
- **Traversal, realistic list** (nodes scattered, as after many inserts and deletes): each step is a dependent load that misses cache; the measured cost is ~200 ns per element against ~1 ns for the array — **two orders of magnitude**. The CPU cannot prefetch what it cannot predict, and the loads form a serial dependency chain.
- **Memory**: 8 MB versus 64 MB for the same 2 million integers (a 16-byte node in a 32-byte `malloc` chunk).

The one place lists win is insertion and removal **at a known position** (or at the front) in a long sequence — which is rarer than it looks:

{{include dsa/arrays/middle.c}}

{{run dsa/arrays :: gcc -std=gnu11 -O2 -Wall -Wextra middle.c -o middle && ./middle}}

Front insertion into the array is O(n) per operation, so the total is quadratic (each doubling of `n` costs 4×: 23 → 96 → 386 → 1,516 ms), while the list's is O(1) each. But note the other columns: appending to the array is *always* the cheapest. And even that list advantage evaporates if you must first *find* the position (an O(n) cache-hostile traversal) — which is why `memmove` of 100–10,000 elements (a few µs, memory-bandwidth bound) usually beats list surgery in practice. When you truly need fast insertion at both ends, use a **deque** / ring buffer; for fast insertion in the middle with order, use a balanced tree, skip list, or a "gap buffer" (text editors: keep the gap at the cursor).

## Variants and relatives

| Structure | Idea | Use |
|---|---|---|
| **Ring buffer / circular array** | wrap indices modulo capacity | queues, deques, producer/consumer (next note) |
| **Deque (block list)** | array of fixed-size blocks | O(1) push/pop at both ends, stable element addresses, no big reallocations (`std::deque`) |
| **Gap buffer** | array with a movable gap | text editors |
| **Rope / piece table** | tree of chunks | large text editing with cheap inserts |
| **Chunked / segmented array** | list of fixed-size arrays | stable pointers, huge sequences, arena-friendly |
| **Sparse array / `mmap` array** | reserve address space, commit lazily | huge mostly-empty arrays, no copy on growth (`mmap` + `MAP_NORESERVE`, `mremap`) |
| **Slice / span / `(ptr, len)`** | non-owning view | passing sub-arrays without copying (Go, Rust, C++20 `span`) |
| **Structure of arrays (SoA)** | one array per field | bulk processing (see the performance note) |
| **Stack** | push/pop at one end only | next note |

## Practical advice

1. Default to a dynamic array for sequences; `reserve` when you can estimate the size.
2. Hold **indices**, not pointers, across modifications.
3. Prefer `swap-remove` when order is irrelevant; prefer batch operations (`memmove` once, `erase-remove`/compaction) over repeated single-element removals (O(n²) → O(n)).
4. Pass `(pointer, length)` to functions rather than the whole container where possible.
5. For huge arrays, `calloc`/`mmap` get zero pages lazily (no up-front cost) and `madvise(MADV_HUGEPAGE)` reduces TLB misses.
6. Test with the differential approach and ASan; keep invariants (`len <= cap`, `data == NULL` iff `cap == 0`) in assertions.

## Key takeaways

- A dynamic array is contiguous storage with geometric growth: O(1) index, O(1) amortized append, O(n) insert/erase in the middle or front.
- Growth must be multiplicative; ×2 is simple and fast, ×1.5 reuses memory better; adding a constant is quadratic overall.
- Any push can move the block, invalidating pointers; use indices and check `realloc` safely.
- Measured on real hardware, the array beat a linked list ~16× on append, ~7× on traversal in the best case and ~200× when list nodes were scattered; it used one eighth of the memory.
- Lists/deques/gap buffers only win for specific access patterns (frequent insertion at known positions or both ends).
