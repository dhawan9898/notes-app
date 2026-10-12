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
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., §16.4 (Dynamic tables)"
  - label: "Stroustrup — Are lists evil? (and Bjarne Stroustrup's keynote \"Software Development for Infrastructure\", GoingNative 2012)"
    url: https://isocpp.org/blog/2014/06/stroustrup-lists
  - label: "Facebook folly — FBVector (growth factor analysis), \"folly/docs/FBVector.md\""
    url: https://github.com/facebook/folly/blob/main/folly/docs/FBVector.md
  - label: "Drepper — What Every Programmer Should Know About Memory, §6 (Optimizing for locality)"
  - label: "man 3 realloc, man 3 reallocarray; glibc malloc internals (mremap for large blocks)"
    url: https://man7.org/linux/man-pages/man3/realloc.3.html
  - label: "Linux kernel — include/linux/vmalloc.h, lib/flex_array (historical) and kvmalloc discussion"
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

```c
#ifndef VEC_H
#define VEC_H
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* A generic dynamic array: elements are stored contiguously; size and capacity are in ELEMENTS. */
typedef struct {
    void  *data;
    size_t elem_size;
    size_t len, cap;
} Vec;

static inline void vec_init(Vec *v, size_t elem_size) { v->data = NULL; v->elem_size = elem_size; v->len = v->cap = 0; }
static inline void vec_free(Vec *v) { free(v->data); vec_init(v, v->elem_size); }

/* Ensure room for at least `want` elements. Returns 0 on success, -1 on overflow or allocation failure. */
static inline int vec_reserve(Vec *v, size_t want) {
    if (want <= v->cap) return 0;
    size_t cap = v->cap ? v->cap : 4;
    while (cap < want) { if (cap > SIZE_MAX / 2) return -1; cap *= 2; }               /* geometric growth: amortized O(1) push */
    if (cap > SIZE_MAX / v->elem_size) return -1;                                        /* byte-size overflow check */
    void *p = realloc(v->data, cap * v->elem_size);
    if (!p) return -1;                                                                   /* old block still valid on failure */
    v->data = p; v->cap = cap;
    return 0;
}

static inline void *vec_at(const Vec *v, size_t i) { assert(i < v->len && "vec index out of range"); return (char *)v->data + i * v->elem_size; }

static inline int vec_push(Vec *v, const void *elem) {
    if (vec_reserve(v, v->len + 1)) return -1;
    memcpy((char *)v->data + v->len * v->elem_size, elem, v->elem_size);
    v->len++;
    return 0;
}

static inline int vec_pop(Vec *v, void *out) {
    if (!v->len) return -1;
    v->len--;
    if (out) memcpy(out, (char *)v->data + v->len * v->elem_size, v->elem_size);
    return 0;
}

/* Insert at index i (0 <= i <= len): shifts the tail with memmove, O(len - i). */
static inline int vec_insert(Vec *v, size_t i, const void *elem) {
    if (i > v->len) return -1;
    if (vec_reserve(v, v->len + 1)) return -1;
    char *base = v->data;
    memmove(base + (i + 1) * v->elem_size, base + i * v->elem_size, (v->len - i) * v->elem_size);
    memcpy(base + i * v->elem_size, elem, v->elem_size);
    v->len++;
    return 0;
}

/* Remove at index i, preserving order: O(len - i). vec_swap_remove is O(1) but changes order. */
static inline int vec_remove(Vec *v, size_t i) {
    if (i >= v->len) return -1;
    char *base = v->data;
    memmove(base + i * v->elem_size, base + (i + 1) * v->elem_size, (v->len - i - 1) * v->elem_size);
    v->len--;
    return 0;
}
static inline int vec_swap_remove(Vec *v, size_t i) {
    if (i >= v->len) return -1;
    char *base = v->data;
    if (i != v->len - 1) memcpy(base + i * v->elem_size, base + (v->len - 1) * v->elem_size, v->elem_size);
    v->len--;
    return 0;
}

/* Give memory back (call after a big shrink). */
static inline int vec_shrink_to_fit(Vec *v) {
    if (v->len == v->cap) return 0;
    if (!v->len) { free(v->data); v->data = NULL; v->cap = 0; return 0; }
    void *p = realloc(v->data, v->len * v->elem_size);
    if (!p) return -1;
    v->data = p; v->cap = v->len;
    return 0;
}

/* Typed convenience macros on top of the generic core. */
#define VEC_GET(T, v, i)  (*(T *)vec_at((v), (i)))
#define VEC_PUSH(T, v, x) do { T _tmp = (x); vec_push((v), &_tmp); } while (0)
#endif
```

It is validated with a **differential test**: 200,000 random operations (push, pop, insert at a random index, order-preserving remove, swap-remove, periodic shrink) are applied to both `Vec` and a trivially correct fixed array, comparing the full contents after every step, under AddressSanitizer and UBSan:

```c
#include <stdio.h>
#include <stdlib.h>
#include "vec.h"

/* Differential test: apply random operations to Vec and to a simple fixed-size reference array; compare after every step. */
int main(void) {
    enum { MAXN = 512 };
    int ref[MAXN]; size_t rn = 0;
    Vec v; vec_init(&v, sizeof(int));
    srand(12345);
    long ops = 0;
    for (int step = 0; step < 200000; step++) {
        int op = rand() % 6, x = rand();
        switch (op) {
        case 0: case 1: if (rn < MAXN) { VEC_PUSH(int, &v, x); ref[rn++] = x; ops++; } break;
        case 2: { int out; int rc = vec_pop(&v, &out); if (rn) { assert(rc == 0 && out == ref[rn - 1]); rn--; ops++; } else assert(rc == -1); } break;
        case 3: if (rn < MAXN) { size_t i = (size_t)rand() % (rn + 1); vec_insert(&v, i, &x); memmove(ref + i + 1, ref + i, (rn - i) * sizeof(int)); ref[i] = x; rn++; ops++; } break;
        case 4: if (rn) { size_t i = (size_t)rand() % rn; vec_remove(&v, i); memmove(ref + i, ref + i + 1, (rn - i - 1) * sizeof(int)); rn--; ops++; } break;
        case 5: if (rn) { size_t i = (size_t)rand() % rn; vec_swap_remove(&v, i); ref[i] = ref[rn - 1]; rn--; ops++; } break;
        }
        assert(v.len == rn);
        for (size_t i = 0; i < rn; i++) assert(VEC_GET(int, &v, i) == ref[i]);
        if (step % 50000 == 49999) vec_shrink_to_fit(&v);
    }
    printf("%ld random operations matched the reference model; final len=%zu cap=%zu\n", ops, v.len, v.cap);
    vec_free(&v);
    return 0;
}
```

```text
$ gcc -std=c11 -O1 -Wall -Wextra -fsanitize=address,undefined test_vec.c -o test_vec && ./test_vec
199564 random operations matched the reference model; final len=2 cap=2
```

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

```c
#include <stdio.h>
#include <stdlib.h>
#include "vec.h"
#include "../../c/perf/timing.h"

struct node { int value; struct node *next; };

int main(void) {
    enum { N = 2000000 };
    double t0, t1;
    long sum = 0;

    t0 = now(); Vec v; vec_init(&v, sizeof(int)); for (int i = 0; i < N; i++) VEC_PUSH(int, &v, i); t1 = now();
    printf("append %d ints:     vector %7.1f ms   ", N, (t1 - t0) * 1e3);
    t0 = now(); struct node *head = NULL, *tail = NULL;
    for (int i = 0; i < N; i++) { struct node *n = malloc(sizeof *n); n->value = i; n->next = NULL; if (tail) tail->next = n; else head = n; tail = n; }
    t1 = now(); printf("linked list %7.1f ms\n", (t1 - t0) * 1e3);

    t0 = now(); for (int rep = 0; rep < 10; rep++) for (size_t i = 0; i < v.len; i++) sum += ((int *)v.data)[i]; t1 = now();
    double t_vec = t1 - t0;
    printf("traverse x10:          vector %7.1f ms   ", t_vec * 1e3);
    t0 = now(); for (int rep = 0; rep < 10; rep++) for (struct node *p = head; p; p = p->next) sum += p->value; t1 = now();
    printf("linked list %7.1f ms   (nodes allocated in order: best-case locality)\n", (t1 - t0) * 1e3);

    /* relink the nodes in random memory order: the realistic state of a list that has seen inserts and deletes */
    struct node **ptrs = malloc((size_t)N * sizeof *ptrs); size_t k = 0;
    for (struct node *p = head; p; p = p->next) ptrs[k++] = p;
    for (size_t i = k - 1; i > 0; i--) { size_t j = (size_t)rand() % (i + 1); struct node *t = ptrs[i]; ptrs[i] = ptrs[j]; ptrs[j] = t; }
    for (size_t i = 0; i + 1 < k; i++) ptrs[i]->next = ptrs[i + 1];
    ptrs[k - 1]->next = NULL; head = ptrs[0];
    t0 = now(); for (int rep = 0; rep < 3; rep++) for (struct node *p = head; p; p = p->next) sum += p->value; t1 = now();
    printf("traverse x3,  list nodes scattered in memory: %7.1f ms   (%.0f ns/element vs %.1f ns/element for the vector)\n", (t1 - t0) * 1e3, (t1 - t0) / (3.0 * N) * 1e9, t_vec / (10.0 * N) * 1e9);

    DO_NOT_OPTIMIZE(sum);
    printf("memory: vector %.0f MB, list %.0f MB (a 16-byte node occupies a 32-byte malloc chunk)\n", N * 4 / 1e6, N * 32.0 / 1e6);
    free(ptrs);
    for (struct node *p = head, *n; p; p = n) { n = p->next; free(p); }
    vec_free(&v);
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra bench.c -o bench && ./bench
append 2000000 ints:     vector     6.6 ms   linked list   107.1 ms
traverse x10:          vector    21.1 ms   linked list   135.8 ms   (nodes allocated in order: best-case locality)
traverse x3,  list nodes scattered in memory:  1410.4 ms   (235 ns/element vs 1.1 ns/element for the vector)
memory: vector 8 MB, list 64 MB (a 16-byte node occupies a 32-byte malloc chunk)
```

- **Append**: the array is >10× faster — no per-element `malloc` (≈ 50 ns each plus 16 bytes of allocator overhead), amortized memcpy instead.
- **Traversal, best case for the list** (nodes allocated back to back, so they sit in memory in list order): the array is still several times faster (compact data: 4 bytes per element instead of 32, and SIMD-friendly loops).
- **Traversal, realistic list** (nodes scattered, as after many inserts and deletes): each step is a dependent load that misses cache; the measured cost is ~200 ns per element against ~1 ns for the array — **two orders of magnitude**. The CPU cannot prefetch what it cannot predict, and the loads form a serial dependency chain.
- **Memory**: 8 MB versus 64 MB for the same 2 million integers (a 16-byte node in a 32-byte `malloc` chunk).

The one place lists win is insertion and removal **at a known position** (or at the front) in a long sequence — which is rarer than it looks:

```c
#include <stdio.h>
#include <stdlib.h>
#include "vec.h"
#include "../../c/perf/timing.h"

struct node { int value; struct node *next; };

/* Insert at the FRONT of a sequence repeatedly: O(n) for an array, O(1) for a list. */
int main(void) {
    printf("%8s %14s %14s %14s\n", "n", "vec front ins", "list front ins", "vec push/pop");
    for (int n = 20000; n <= 160000; n *= 2) {
        Vec v; vec_init(&v, sizeof(int));
        double t0 = now(); for (int i = 0; i < n; i++) vec_insert(&v, 0, &i); double t1 = now();
        struct node *head = NULL;
        double t2 = now(); for (int i = 0; i < n; i++) { struct node *x = malloc(sizeof *x); x->value = i; x->next = head; head = x; } double t3 = now();
        Vec w; vec_init(&w, sizeof(int));
        double t4 = now(); for (int i = 0; i < n; i++) vec_push(&w, &i); double t5 = now();
        printf("%8d %11.2f ms %11.2f ms %11.2f ms\n", n, (t1 - t0) * 1e3, (t3 - t2) * 1e3, (t5 - t4) * 1e3);
        for (struct node *p = head, *q; p; p = q) { q = p->next; free(p); }
        vec_free(&v); vec_free(&w);
    }
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra middle.c -o middle && ./middle
       n  vec front ins list front ins   vec push/pop
   20000       24.59 ms        0.83 ms        0.08 ms
   40000      103.31 ms        1.80 ms        0.15 ms
   80000      415.00 ms        3.09 ms        0.48 ms
  160000     1427.32 ms        3.36 ms        0.62 ms
```

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
