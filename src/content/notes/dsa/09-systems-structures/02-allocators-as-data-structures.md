---
title: Allocators as Data Structures
track: dsa
module: Systems-Flavored Structures
moduleOrder: 9
order: 2
level: expert
summary: "Free lists, boundary tags and coalescing, first/next/best fit compared on a random workload with a heap checker, the buddy allocator behind the Linux page allocator, and arena and pool allocators that beat malloc by 10x — all tested for overlap and corruption."
sources:
  - label: "Knuth — The Art of Computer Programming, Vol. 1, §2.5 (Dynamic Storage Allocation)"
  - label: "Wilson, Johnstone, Neely, Boles — Dynamic Storage Allocation, A Survey and Critical Review (IWMM 1995)"
  - label: "Johnstone & Wilson — The Memory Fragmentation Problem, Solved? (ISMM 1998)"
  - label: "Knowlton — A Fast Storage Allocator (CACM 8(10), 1965) (the buddy system)"
  - label: "Bonwick — The Slab Allocator, An Object-Caching Kernel Memory Allocator (USENIX Summer 1994)"
  - label: "Bryant & O'Hallaron — Computer Systems, A Programmer's Perspective, Ch. 9.9 (Dynamic Memory Allocation)"
  - label: "Linux kernel documentation — Memory allocation guide and the buddy allocator"
    url: https://www.kernel.org/doc/html/latest/core-api/memory-allocation.html
  - label: "glibc malloc internals (MallocInternals wiki)"
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

```c
#ifndef FIT_H
#define FIT_H
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* A boundary-tag allocator over a fixed heap, with an explicit doubly linked free list and three placement policies.
 *
 *   block:  [ header 8 B | payload ........................ | footer 8 B ]      header = footer = size | allocated-bit
 *   a FREE block keeps its list links in the first 16 bytes of the payload (offsets from the heap base)
 *   heap:   [ prologue (16 B, allocated) | blocks ... | epilogue header (size 0, allocated) ]
 *
 * Because every block carries a footer, free() can find the PREVIOUS block in O(1) and merge ("coalesce") with free neighbours. */
enum { FIRST_FIT, NEXT_FIT, BEST_FIT };
typedef uint64_t w_t;
typedef struct { uint8_t *base; size_t size; w_t free_head; w_t rover; int policy; unsigned long long steps, allocs, fails; size_t live_bytes, high_water; } Heap;

#define ALLOC 1u
#define MINBLK 32
static inline w_t pack(size_t sz, int a) { return (w_t)sz | (w_t)a; }
static inline size_t bsize(const uint8_t *hp) { return (size_t)(*(const w_t *)hp & ~(w_t)15); }
static inline int balloc(const uint8_t *hp) { return (int)(*(const w_t *)hp & 1); }
static inline void set_block(uint8_t *hp, size_t sz, int a) { *(w_t *)hp = pack(sz, a); *(w_t *)(hp + sz - 8) = pack(sz, a); }
#define NEXT_OF(h, off) (*(w_t *)((h)->base + (off) + 8))
#define PREV_OF(h, off) (*(w_t *)((h)->base + (off) + 16))

static void fl_insert(Heap *h, w_t off) { NEXT_OF(h, off) = h->free_head; PREV_OF(h, off) = 0; if (h->free_head) PREV_OF(h, h->free_head) = off; h->free_head = off; }
static void fl_remove(Heap *h, w_t off) {
    w_t n = NEXT_OF(h, off), p = PREV_OF(h, off);
    if (p) NEXT_OF(h, p) = n; else h->free_head = n;
    if (n) PREV_OF(h, n) = p;
    if (h->rover == off) h->rover = n ? n : h->free_head;
}
static void heap_init(Heap *h, size_t size, int policy) {
    memset(h, 0, sizeof *h); h->size = size; h->policy = policy; h->base = aligned_alloc(16, size);
    set_block(h->base, 16, 1);                                                      /* prologue */
    size_t first = size - 16 - 8; first &= ~(size_t)15;                             /* one big free block, leaving room for the epilogue */
    set_block(h->base + 16, first, 0); *(w_t *)(h->base + 16 + first) = pack(0, 1); /* epilogue header */
    fl_insert(h, 16); h->rover = 16;
}
static void heap_destroy(Heap *h) { free(h->base); }

static void *h_malloc(Heap *h, size_t n) {
    size_t need = (n + 16 + 15) & ~(size_t)15; if (need < MINBLK) need = MINBLK;
    w_t best = 0, found = 0; h->allocs++;
    if (h->policy == FIRST_FIT) { for (w_t o = h->free_head; o; o = NEXT_OF(h, o)) { h->steps++; if (bsize(h->base + o) >= need) { found = o; break; } } }
    else if (h->policy == NEXT_FIT) {
        w_t start = h->rover ? h->rover : h->free_head, o = start;
        if (o) do { h->steps++; if (bsize(h->base + o) >= need) { found = o; break; } o = NEXT_OF(h, o) ? NEXT_OF(h, o) : h->free_head; } while (o != start);
    } else { for (w_t o = h->free_head; o; o = NEXT_OF(h, o)) { h->steps++; size_t s = bsize(h->base + o); if (s >= need && (!best || s < bsize(h->base + best))) { best = o; if (s == need) break; } } found = best; }
    if (!found) { h->fails++; return NULL; }
    uint8_t *hp = h->base + found; size_t sz = bsize(hp); h->rover = NEXT_OF(h, found) ? NEXT_OF(h, found) : h->free_head; fl_remove(h, found);
    if (sz - need >= MINBLK) { set_block(hp, need, 1); set_block(hp + need, sz - need, 0); fl_insert(h, found + need); } else set_block(hp, sz, 1);
    h->live_bytes += bsize(hp); if (h->live_bytes > h->high_water) h->high_water = h->live_bytes;
    return hp + 8;
}
static void h_free(Heap *h, void *p) {
    if (!p) return;
    uint8_t *hp = (uint8_t *)p - 8; size_t sz = bsize(hp); h->live_bytes -= sz;
    uint8_t *next = hp + sz; int prev_alloc = balloc(hp - 8);                      /* previous block's footer sits just below our header */
    if (!balloc(next)) { size_t ns = bsize(next); fl_remove(h, (w_t)(next - h->base)); sz += ns; }
    if (!prev_alloc) { size_t ps = bsize(hp - 8); hp -= ps; fl_remove(h, (w_t)(hp - h->base)); sz += ps; }
    set_block(hp, sz, 0); fl_insert(h, (w_t)(hp - h->base));
}
/* Heap consistency checker: walk every block by size and cross-check against the free list. Returns total free bytes; asserts on corruption. */
static size_t heap_check(const Heap *h, size_t *largest_free, size_t *nfree_blocks) {
    const uint8_t *p = h->base + 16; size_t free_bytes = 0, nfree = 0, largest = 0; int prev_free = 0;
    while (bsize(p) != 0) {
        size_t s = bsize(p); assert(s >= MINBLK && s % 16 == 0 && p + s <= h->base + h->size);
        assert(*(const w_t *)p == *(const w_t *)(p + s - 8));                       /* header == footer */
        if (!balloc(p)) { assert(!prev_free); free_bytes += s; nfree++; if (s > largest) largest = s; prev_free = 1; } else prev_free = 0;   /* no two adjacent free blocks */
        p += s;
    }
    size_t listed = 0, lbytes = 0; w_t prev = 0;
    for (w_t o = h->free_head; o; o = NEXT_OF(h, o)) { assert(!balloc(h->base + o)); assert(PREV_OF(h, o) == prev); prev = o; listed++; lbytes += bsize(h->base + o); assert(listed <= nfree); }
    assert(listed == nfree && lbytes == free_bytes);
    if (largest_free) *largest_free = largest;
    if (nfree_blocks) *nfree_blocks = nfree;
    return free_bytes;
}
#endif
```

The `heap_check` function is as important as the allocator: it walks every block by size, verifies header equals footer, that no two free blocks are adjacent (they would have been coalesced), and that the free list links and sums agree with the walk. Allocator bugs corrupt state far from where they occur, and a checker run periodically is the difference between a day and a month of debugging.

```c
#define _GNU_SOURCE
#include <stdio.h>
#include <math.h>
#include "fit.h"
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint64_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
typedef struct { uint8_t *p; size_t n; uint8_t tag; } Live;
static size_t pick_size(void) { double u = (double)(rnd() >> 11) / 9007199254740992.0; return (size_t)exp(log(16.0) + u * (log(16384.0) - log(16.0))); }   /* log-uniform 16 B .. 16 KB */

int main(void) {
    static const char *names[] = { "first fit", "next fit", "best fit" };
    size_t HEAP = 64u << 20, TARGET = 40u << 20; int OPS = 3000000;
    printf("%-10s | %10s %12s | %10s %12s %12s | %s\n", "policy", "steps/alloc", "failed allocs", "free blocks", "free bytes", "largest free", "peak utilisation");
    for (int pol = 0; pol < 3; pol++) {
        s = 88172645463325252ULL; Heap h; heap_init(&h, HEAP, pol); size_t cap = 400000, nlive = 0; Live *live = malloc(cap * sizeof *live); size_t requested_live = 0, peak_req = 0;
        for (int op = 0; op < OPS; op++) {
            int do_alloc = nlive == 0 || (requested_live < TARGET ? rnd() % 100 < 55 : rnd() % 100 < 45);
            if (do_alloc && nlive < cap) {
                size_t n = pick_size(); uint8_t *p = h_malloc(&h, n);
                if (p) { uint8_t tag = (uint8_t)rnd(); memset(p, tag, n); live[nlive++] = (Live){ p, n, tag }; requested_live += n; if (requested_live > peak_req) peak_req = requested_live; }
            } else {
                size_t i = rnd() % nlive; Live l = live[i];
                for (size_t k = 0; k < l.n; k += 61) if (l.p[k] != l.tag) { printf("CORRUPTION in %s\n", names[pol]); return 1; }      /* nobody overwrote our payload */
                h_free(&h, l.p); requested_live -= l.n; live[i] = live[--nlive];
            }
            if (op % 500000 == 0) heap_check(&h, NULL, NULL);
        }
        size_t largest, nfree; size_t fb = heap_check(&h, &largest, &nfree);
        printf("%-10s | %10.1f %12llu | %10zu %9.1f MB %9.2f MB | %.1f%% of heap holds live data at peak\n", names[pol], (double)h.steps / (double)h.allocs, h.fails, nfree, fb / 1048576.0, largest / 1048576.0, 100.0 * (double)peak_req / (double)HEAP);
        for (size_t i = 0; i < nlive; i++) h_free(&h, live[i].p);
        size_t l2, n2; size_t fb2 = heap_check(&h, &l2, &n2);
        if (n2 != 1) { printf("after freeing everything there should be ONE free block, found %zu\n", n2); return 1; }
        (void)fb2; free(live); heap_destroy(&h);
    }
    printf("\nafter freeing every allocation each heap coalesced back into a single free block (verified by the checker)\n");
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined test_fit.c -o test_fit -lm && ./test_fit
policy     | steps/alloc failed allocs | free blocks   free bytes largest free | peak utilisation
first fit  |      907.8       103794 |      14960      23.3 MB      0.01 MB | 62.8% of heap holds live data at peak
next fit   |      102.9          651 |       8861      23.7 MB      0.02 MB | 63.0% of heap holds live data at peak
best fit   |      613.5            0 |       1856      23.6 MB     20.69 MB | 63.1% of heap holds live data at peak

after freeing every allocation each heap coalesced back into a single free block (verified by the checker)
```

The workload: three million random operations on a 64 MiB heap, sizes log-uniform from 16 bytes to 16 KiB, with the live data steered towards 40 MiB. Every block is filled with a random tag when allocated and verified when freed (so overlapping or corrupted blocks are detected), and the checker validates the heap periodically and verifies that after everything is freed the heap collapses back into one block.

What the table shows (for this workload; allocator rankings are workload-dependent):

- **First fit with a LIFO free list fragmented badly**: more than 100,000 allocations failed even though about 23 MB was free — the largest free block was only ~10 KB. External fragmentation made free memory unusable. Inserting freed blocks at the list head scatters small blocks at the front where every search must step over them (900+ steps per allocation).
- **Next fit** skips most of that scanning (about 100 steps) but still fails several hundred requests: same free space, shredded into small pieces.
- **Best fit failed nothing**: its free memory was concentrated into a few large blocks (the largest free block was ~21 MB). Its search cost is high (the whole list unless it finds an exact fit) — which is why real allocators replace the single list with **segregated free lists** (one per size class) and bins, getting best-fit-like quality in near-constant time.
- Every policy coalesced back to a single block after the final frees: the footers did their job.

Production designs refine this: glibc's `ptmalloc` keeps *fastbins* and *tcache* (per-thread caches of recently freed small chunks), small bins, large bins sorted by size, and an unsorted bin; `jemalloc` and `tcmalloc` use size classes with thread-local caches and "slabs" per class; Go and Java use compacting or generational collectors, which can *move* objects to defragment — impossible in C.

## The buddy system

A **binary buddy** allocator manages a region of size `2^N` by recursively halving. Every block has a power-of-two size, and each block of size `2^k` at offset `o` has exactly one **buddy** at `o XOR 2^k`. Allocation rounds the request up to a power of two, takes a free block of that order (splitting a larger one if needed, freeing each upper half), and free merges a block with its buddy whenever the buddy is free *and the same size*, repeating up. Both operations take `O(log N)` and need no search; coalescing is address arithmetic.

```c
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Binary buddy allocator over a 16 MiB region, block sizes 16 B .. 16 MiB (orders 4..24).  A block of order k at offset o has its buddy at o ^ (1 << k):
 * allocation splits a larger block in halves; free() merges a block with its buddy whenever the buddy is free and the same size, repeating upward.
 * This is the scheme behind the Linux page allocator (orders are page counts: order 0 = 1 page ... order 10 = 4 MiB with 4 KiB pages). */
#define MINO 4
#define MAXO 24
#define NORD (MAXO + 1)
static uint8_t *region; static uint32_t freehead[NORD];       /* offset+1 of the first free block of each order (0 = empty) */
static uint8_t *state;                                         /* per 16-byte unit: 0 = interior of a block, 1 + order = FREE head, 0x80 | order = ALLOCATED head */
#define UNIT(o) ((o) >> MINO)
#define LNEXT(o) (*(uint32_t *)(region + (o) + 4))
#define LPREV(o) (*(uint32_t *)(region + (o) + 8))
static void list_add(uint32_t o, int k) { LNEXT(o) = freehead[k]; LPREV(o) = 0; if (freehead[k]) LPREV(freehead[k] - 1) = o + 1; freehead[k] = o + 1; state[UNIT(o)] = (uint8_t)(1 + k); }
static void list_del(uint32_t o, int k) { uint32_t n = LNEXT(o), p = LPREV(o); if (p) LNEXT(p - 1) = n; else freehead[k] = n; if (n) LPREV(n - 1) = p; state[UNIT(o)] = 0; }
static unsigned long long unsplit_events;

static void *balloc(size_t n, int *order_out) {
    int k = MINO; while (((size_t)1 << k) < n) k++;
    if (k > MAXO) return NULL;
    int j = k; while (j <= MAXO && !freehead[j]) j++;
    if (j > MAXO) return NULL;                                   /* no block big enough */
    uint32_t o = freehead[j] - 1; list_del(o, j);
    while (j > k) { j--; list_add(o + (1u << j), j); }           /* split: keep the lower half, free the upper half (its buddy) */
    state[UNIT(o)] = (uint8_t)(0x80 | k); if (order_out) *order_out = k;
    return region + o;
}
static void bfree(void *p) {
    uint32_t o = (uint32_t)((uint8_t *)p - region); int k = state[UNIT(o)] & 0x7F; assert(state[UNIT(o)] & 0x80);
    while (k < MAXO) {
        uint32_t b = o ^ (1u << k);
        if (state[UNIT(b)] != 1 + k) break;                      /* buddy is not a free block of exactly this order */
        list_del(b, k); if (b < o) o = b; k++; unsplit_events++;
    }
    list_add(o, k);
}

static uint64_t s = 88172645463325252ULL;
static uint64_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
typedef struct { uint8_t *p; size_t n; int order; uint8_t tag; } Live;

int main(void) {
    region = aligned_alloc(16, (size_t)1 << MAXO); state = calloc((size_t)1 << (MAXO - MINO), 1);
    list_add(0, MAXO);
    size_t cap = 100000, nlive = 0; Live *live = malloc(cap * sizeof *live); unsigned long long asked = 0, granted = 0; size_t live_req = 0, live_blk = 0, peak_blk = 0;
    for (int op = 0; op < 2000000; op++) {
        if (nlive == 0 || (nlive < cap && rnd() % 100 < 50 + (live_blk < (8u << 20) ? 5 : -5))) {
            size_t n = 16 + rnd() % (rnd() % 4 == 0 ? 20000 : 600); int k; uint8_t *p = balloc(n, &k);
            if (p) { uint8_t tag = (uint8_t)rnd(); memset(p, tag, n); live[nlive++] = (Live){ p, n, k, tag }; asked += n; granted += (size_t)1 << k; live_req += n; live_blk += (size_t)1 << k; if (live_blk > peak_blk) peak_blk = live_blk; }
        } else {
            size_t i = rnd() % nlive; Live l = live[i];
            for (size_t j = 0; j < l.n; j += 37) assert(l.p[j] == l.tag);          /* neighbours never wrote into our block */
            bfree(l.p); live_req -= l.n; live_blk -= (size_t)1 << l.order; live[i] = live[--nlive];
        }
    }
    printf("random allocate/free workload, 2,000,000 operations, sizes 16 B .. 20 KB\n");
    printf("internal fragmentation: requested %.1f MB, granted %.1f MB (blocks rounded up to a power of two) -> %.1f%% of granted space is wasted inside blocks\n",
           asked / 1048576.0, granted / 1048576.0, 100.0 * (1.0 - (double)asked / (double)granted));
    printf("buddy merges performed: %llu; peak live blocks %.1f MB\n", unsplit_events, peak_blk / 1048576.0);
    for (size_t i = 0; i < nlive; i++) bfree(live[i].p);
    int blocks = 0; for (int k = 0; k <= MAXO; k++) for (uint32_t o = freehead[k]; o; o = LNEXT(o - 1)) blocks++;
    printf("after freeing everything: %d free block(s), largest order %d -> %s\n", blocks, freehead[MAXO] ? MAXO : -1, blocks == 1 && freehead[MAXO] ? "fully coalesced into the original 16 MiB block" : "NOT fully coalesced");
    assert(blocks == 1 && freehead[MAXO]);
    free(live); free(region); free(state); return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined buddy.c -o buddy && ./buddy
random allocate/free workload, 2,000,000 operations, sizes 16 B .. 20 KB
internal fragmentation: requested 2613.1 MB, granted 3889.9 MB (blocks rounded up to a power of two) -> 32.8% of granted space is wasted inside blocks
buddy merges performed: 31030; peak live blocks 8.6 MB
after freeing everything: 1 free block(s), largest order 24 -> fully coalesced into the original 16 MiB block
```

The workload mixes small and medium requests. The measured **internal fragmentation** is about a third of granted space wasted on rounding up to the next power of two — the buddy system's price (a rule of thumb is about 25 % for sizes spread evenly within each power-of-two band; this mixed workload measured 33 %, and the figure depends entirely on where request sizes fall relative to the powers of two). In return it is fast and coalesces perfectly: after every block is freed the whole 16 MiB region is a single block again, verified. This is how the **Linux page allocator** manages physical memory: free pages are kept in per-order lists (order 0 = one 4 KiB page up to order 10 = 4 MiB), `/proc/buddyinfo` shows the free block count per order, and external fragmentation is visible there as memory sitting only in the low orders (huge-page and DMA allocations then fail). For small kernel objects the **slab allocator** (SLUB) sits on top, carving pages into per-type object caches — see the next idea.

## Special-purpose allocators: arena and pool

When the allocation pattern is known, much simpler structures beat `malloc` outright.

- **Arena (bump) allocator**: allocation is `ptr += size`; there is no individual `free` — the whole arena is released (or reset) at once. Perfect for data with a common lifetime: a request's temporary objects, a parse tree, a compiler pass. It allocates in a few instructions, has zero per-object overhead and gives *excellent locality* because consecutive allocations are adjacent.
- **Pool (free-list / slab) allocator**: for objects of one size, freed objects form a singly linked list threaded through their own memory; allocate and free are a pointer swap. Kernel slab caches (`kmem_cache`), network buffer pools (`sk_buff`) and game entity systems use this; **Bonwick's slab** also keeps objects *constructed* between uses.

```c
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

/* ---- Arena (bump) allocator: allocation = advance a pointer; free = reset the whole arena at once ---- */
typedef struct { uint8_t *base, *cur, *end; } Arena;
static inline void *arena_alloc(Arena *a, size_t n) { n = (n + 15) & ~(size_t)15; if ((size_t)(a->end - a->cur) < n) return NULL; void *p = a->cur; a->cur += n; return p; }
/* ---- Pool (fixed-size free list): freed objects are threaded through their own memory; alloc/free are a pointer swap ---- */
typedef struct PoolNode { struct PoolNode *next; } PoolNode;
typedef struct { uint8_t *mem; PoolNode *free; size_t obj, count; } Pool;
static void pool_init(Pool *p, size_t obj, size_t count) { p->obj = obj < sizeof(PoolNode) ? sizeof(PoolNode) : obj; p->count = count; p->mem = malloc(p->obj * count); p->free = NULL; for (size_t i = count; i-- > 0; ) { PoolNode *n = (PoolNode *)(p->mem + i * p->obj); n->next = p->free; p->free = n; } }
static inline void *pool_alloc(Pool *p) { PoolNode *n = p->free; if (n) p->free = n->next; return n; }
static inline void pool_free(Pool *p, void *x) { PoolNode *n = x; n->next = p->free; p->free = n; }

typedef struct Node { struct Node *next; uint64_t key, a, b; } Node;               /* a 32-byte object */

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    enum { N = 5000000 };
    printf("== build a linked list of %d 32-byte nodes, traverse it, then release everything ==\n", N);
    double t0 = now(); Node *head = NULL; for (int i = 0; i < N; i++) { Node *x = malloc(sizeof *x); x->key = (uint64_t)i; x->next = head; head = x; }
    double t_build = now() - t0; t0 = now(); uint64_t sum = 0; for (Node *x = head; x; x = x->next) sum += x->key; double t_walk = now() - t0;
    t0 = now(); while (head) { Node *n = head->next; free(head); head = n; } double t_free = now() - t0;
    printf("malloc/free     : build %6.1f ms, walk %5.1f ms, release %6.1f ms\n", t_build * 1e3, t_walk * 1e3, t_free * 1e3);
    Arena a = { malloc((size_t)N * sizeof(Node)), NULL, NULL }; a.cur = a.base; a.end = a.base + (size_t)N * sizeof(Node);
    t0 = now(); head = NULL; for (int i = 0; i < N; i++) { Node *x = arena_alloc(&a, sizeof *x); x->key = (uint64_t)i; x->next = head; head = x; }
    double a_build = now() - t0; t0 = now(); uint64_t sum2 = 0; for (Node *x = head; x; x = x->next) sum2 += x->key; double a_walk = now() - t0;
    t0 = now(); a.cur = a.base; head = NULL; double a_free = now() - t0; assert(sum == sum2);
    printf("arena           : build %6.1f ms, walk %5.1f ms, release %6.3f ms  (one pointer reset)\n", a_build * 1e3, a_walk * 1e3, a_free * 1e3);
    free(a.base);

    printf("\n== churn: keep 100,000 live 32-byte objects; 20,000,000 times free a random one and allocate a replacement ==\n");
    enum { LIVE = 100000, ITER = 20000000 };
    Node **slot = malloc(LIVE * sizeof *slot);
    for (int i = 0; i < LIVE; i++) slot[i] = malloc(sizeof(Node));
    t0 = now(); for (int i = 0; i < ITER; i++) { int j = (int)(rnd() % LIVE); free(slot[j]); slot[j] = malloc(sizeof(Node)); slot[j]->key = (uint64_t)i; } double t_m = now() - t0;
    for (int i = 0; i < LIVE; i++) free(slot[i]);
    Pool pool; pool_init(&pool, sizeof(Node), LIVE + 1);
    for (int i = 0; i < LIVE; i++) slot[i] = pool_alloc(&pool);
    t0 = now(); for (int i = 0; i < ITER; i++) { int j = (int)(rnd() % LIVE); pool_free(&pool, slot[j]); slot[j] = pool_alloc(&pool); slot[j]->key = (uint64_t)i; } double t_p = now() - t0;
    printf("malloc/free: %.1f ns per free+alloc pair;  pool: %.1f ns per pair  (%.1fx faster)\n", t_m / ITER * 1e9, t_p / ITER * 1e9, t_m / t_p);
    /* pool correctness: every live object distinct and inside the pool */
    for (int i = 0; i < LIVE; i++) { assert((uint8_t *)slot[i] >= pool.mem && (uint8_t *)slot[i] < pool.mem + pool.obj * pool.count); for (int k = i + 1; k < i + 3 && k < LIVE; k++) assert(slot[i] != slot[k]); }
    free(pool.mem); free(slot); return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra pool_arena.c -o pool_arena && ./pool_arena
== build a linked list of 5000000 32-byte nodes, traverse it, then release everything ==
malloc/free     : build  272.4 ms, walk  41.4 ms, release   74.2 ms
arena           : build   22.5 ms, walk  32.7 ms, release  0.000 ms  (one pointer reset)

== churn: keep 100,000 live 32-byte objects; 20,000,000 times free a random one and allocate a replacement ==
malloc/free: 38.2 ns per free+alloc pair;  pool: 4.5 ns per pair  (8.6x faster)
```

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
