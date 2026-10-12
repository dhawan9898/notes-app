---
title: "Dynamic Memory: malloc Internals, Classic Bugs and Custom Allocators"
track: c
module: Memory and Pointers
moduleOrder: 2
order: 3
level: advanced
summary: How malloc/free work under the hood, the memory-error taxonomy with AddressSanitizer reports, safe allocation patterns, growth policies, and a tested arena allocator.
prerequisites:
  - Pointers, Arrays and Decay
sources:
  - label: "ISO/IEC 9899:2018 (C17) §7.22.3 Memory management functions; §6.2.4 Storage durations"
  - label: "man 3 malloc, man 3 posix_memalign, man 3 mallopt, man 3 malloc_usable_size, man 3 mtrace"
    url: https://man7.org/linux/man-pages/man3/malloc.3.html
  - label: "glibc malloc internals (MallocInternals wiki)"
    url: https://sourceware.org/glibc/wiki/MallocInternals
  - label: "AddressSanitizer algorithm and documentation (Clang/GCC)"
    url: https://github.com/google/sanitizers/wiki/AddressSanitizer
  - label: "Wilson et al. — Dynamic Storage Allocation - A Survey and Critical Review (1995)"
  - label: "SEI CERT C - MEM30-C, MEM31-C, MEM34-C, MEM35-C (memory rules)"
    url: https://wiki.sei.cmu.edu/confluence/display/c/Rule+08.+Memory+Management+%28MEM%29
---

C gives you three storage durations: **static** (globals, `static` locals: live for the whole program), **automatic** (locals: live until the block ends, on the stack) and **allocated** (the heap, lives until you `free` it). Heap memory is flexible (size known at run time, lifetime independent of scope) and is the source of the language's most expensive bugs.

## The API

| Function | Behavior |
|---|---|
| `void *malloc(size_t n)` | `n` bytes, **uninitialized**; returns `NULL` on failure; `malloc(0)` returns `NULL` *or* a unique pointer (implementation-defined; either way freeable) |
| `void *calloc(size_t cnt, size_t sz)` | zero-initialized `cnt*sz` bytes; **checks the multiplication for overflow** and returns `NULL` instead |
| `void *realloc(void *p, size_t n)` | resize, possibly moving; contents preserved up to `min(old,new)`; **on failure returns `NULL` and the old block stays valid** |
| `void free(void *p)` | release; `free(NULL)` is a no-op; any other invalid pointer is UB |
| `aligned_alloc(align, size)` (C11), `posix_memalign` | alignment stricter than `max_align_t` (size must be a multiple of `align` for `aligned_alloc`) |
| `reallocarray(p, n, sz)` (glibc/BSD) | `realloc` with the overflow check of `calloc` |

`malloc` returns memory aligned for any standard type (16 bytes on x86-64 glibc). Casting the result is unnecessary in C (and hides a missing `#include <stdlib.h>`).

**Checking for `NULL`**: do it, but know that on Linux with overcommit (default heuristic mode) `malloc` almost never fails for ordinary sizes; the failure surfaces later as an OOM-kill when pages are touched. Library code and embedded/safety-critical code must still handle `NULL`; applications often choose to abort with a clear message (`xmalloc` wrapper). **Always compute the size with overflow checking** (`calloc`, `reallocarray`, or the `SIZE_MAX / n` guard): `malloc(n * sizeof(T))` with attacker-controlled `n` is a classic heap overflow.

```c
p = realloc(p, new_size);            /* BUG: on failure p becomes NULL and the old block leaks */
tmp = realloc(p, new_size);          /* correct: use a temporary */
if (!tmp) { free(p); return ERR; }
p = tmp;
```

Idiom: `T *p = malloc(sizeof *p);` (tracks the pointee's type) and `T *a = calloc(n, sizeof *a);`.

## How the allocator works (glibc ptmalloc)

The kernel hands out memory in pages (via `brk`/`sbrk` for the main heap, `mmap` for large and secondary arenas); the allocator carves that into **chunks** and manages free ones.

- A chunk has a small header: the **size** (with low bits as flags: previous-in-use, mmapped, non-main-arena) and, when free, forward/back links in the user area. Chunks are 16-byte aligned, minimum 32 bytes on 64-bit, so `malloc(1)` costs 32 bytes of address space.
- Free chunks are kept in **bins** by size: per-thread **tcache** (fast, lock-free, up to 7 chunks per size class), **fastbins** (small chunks, LIFO, not coalesced), **small bins** (exact sizes), **large bins** (ranges, sorted), and the **unsorted bin**. Adjacent free chunks are **coalesced** to fight fragmentation; the **top chunk** at the heap's end is split for new requests and trimmed when large.
- Requests ≥ `M_MMAP_THRESHOLD` (128 KiB default, dynamic) use `mmap` directly and are returned to the OS on `free`.
- **Threads** get separate **arenas** (up to 8× cores) to avoid lock contention, at the cost of memory footprint.

Consequences you can observe: freed memory is usually *not* returned to the OS (RSS stays high; `malloc_trim(0)`), small allocations have per-chunk overhead, and the allocator state sits *next to your data*: heap overflows and use-after-free corrupt the allocator's own metadata, which is why they are exploitable (modern glibc adds safe-linking and integrity checks, e.g. `free(): invalid pointer`, `malloc(): corrupted top size`).

Alternative allocators: **jemalloc** (low fragmentation, good profiling; FreeBSD/Redis/Firefox), **tcmalloc** (thread caches; Google), **mimalloc** (Microsoft; fast), **scudo** (hardened; Android), **musl's** simple malloc (slow under contention), debugging allocators (ASan, Electric Fence/`libefence`, `guarder`). Swap via `LD_PRELOAD=libjemalloc.so` to test.

## The memory bug taxonomy

| Bug | Description | Typical symptom |
|---|---|---|
| **Heap buffer overflow/underflow** | write/read outside `[p, p+n)` | silent corruption, later crash in `free`, security exploit |
| **Use after free** | access after `free` | works until the block is reused; "random" corruption |
| **Double free** | `free` twice | abort `free(): double free detected` or exploitable |
| **Invalid free** | `free` of non-heap/interior pointer | abort / corruption |
| **Memory leak** | the last pointer to a block is lost | growing RSS, OOM in long-running processes |
| **Uninitialized read** | reading `malloc`'d bytes before writing | nondeterministic values, info leaks |
| **Mismatched APIs** | `free` on `new[]` memory, `realloc` on stack | UB |
| **NULL dereference after failed alloc** | missing check | crash |
| **Stack/global overflows** | adjacent categories | see stack note |

A tiny program with selectable bugs (`bugs.c`):

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    int which = argc > 1 ? atoi(argv[1]) : 0;
    char *p = malloc(16);
    switch (which) {
    case 1: p[16] = 'x'; break;                         /* heap buffer overflow by one byte */
    case 2: free(p); p[0] = 'x'; return 0;              /* use after free */
    case 3: free(p); free(p); return 0;                 /* double free */
    case 4: p = NULL; break;                            /* leak: the only pointer is lost */
    case 5: { int *a = malloc(4 * sizeof *a); int v = a[2]; printf("%d\n", v); free(a); } break;   /* uninitialized read (MSan/valgrind) */
    case 6: { char *q = realloc(p, 1 << 20); if (!q) return 1; p = q; memset(p, 0, 1 << 20); } break;
    default: break;
    }
    free(p);
    return 0;
}
```

Built with **AddressSanitizer** (`gcc -g -fsanitize=address`), each case gets a precise report with the access type, the offending allocation and free stacks:

```text
$ gcc -g -fsanitize=address -fno-omit-frame-pointer bugs.c -o bugs_asan && for i in 1 2 3 4; do echo "=== case $i"; ./bugs_asan $i 2>&1 | grep -E "ERROR|WRITE|located|SUMMARY" | head -4 | cut -c1-150; done
=== case 1
==2427571==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x502000000020 at pc 0x55dee366a2c2 bp 0x7ffea7e48660 sp 0x7ffea7e48658
WRITE of size 1 at 0x502000000020 thread T0
0x502000000020 is located 0 bytes after 16-byte region [0x502000000010,0x502000000020)
allocated by thread T0 here:
=== case 2
==2427575==ERROR: AddressSanitizer: heap-use-after-free on address 0x502000000010 at pc 0x5649122bc30a bp 0x7ffd0766ef30 sp 0x7ffd0766ef28
WRITE of size 1 at 0x502000000010 thread T0
0x502000000010 is located 0 bytes inside of 16-byte region [0x502000000010,0x502000000020)
previously allocated by thread T0 here:
=== case 3
==2427579==ERROR: AddressSanitizer: attempting double-free on 0x502000000010 in thread T0:
0x502000000010 is located 0 bytes inside of 16-byte region [0x502000000010,0x502000000020)
previously allocated by thread T0 here:
SUMMARY: AddressSanitizer: double-free ../../../../src/libsanitizer/asan/asan_malloc_linux.cpp:52 in free
=== case 4
==2427583==ERROR: LeakSanitizer: detected memory leaks
Direct leak of 16 byte(s) in 1 object(s) allocated from:
SUMMARY: AddressSanitizer: 16 byte(s) leaked in 1 allocation(s).
```

ASan places **redzones** (poisoned bytes) around each allocation and quarantines freed blocks, tracking validity in a shadow memory (one shadow byte per 8 application bytes); overhead ≈ 2× time, 2–3× memory. `LeakSanitizer` (included on Linux) reports leaks at exit. Alternatives: `valgrind --leak-check=full --track-origins=yes ./prog` (no recompile, ~20× slower; finds uninitialized reads), `MALLOC_CHECK_=3`/`MALLOC_PERTURB_=165` (glibc: poison allocations and freed memory to surface bugs), `mtrace`, `heaptrack`, `bpftrace`/`memleak` (long-running processes in production). Make "run the tests under ASan+UBSan" a CI job.

## Safe patterns

1. **Ownership convention**: decide who frees. Document it per function (`returns an allocated string; caller frees`, `takes ownership`, `borrows`). The usual API shape is `T *t_new(...)` / `void t_free(T *)`, where `t_free(NULL)` is a no-op.
2. **Single exit with cleanup** (the `goto` idiom), avoiding leaks on error paths:

```c
int process(const char *path) {
    int rc = -1;
    char *buf = NULL; FILE *f = NULL;
    f = fopen(path, "rb");        if (!f) goto out;
    buf = malloc(4096);           if (!buf) goto out;
    /* ... use f and buf ... */
    rc = 0;
out:
    free(buf);                    /* free(NULL) is fine */
    if (f) fclose(f);
    return rc;
}
```

3. **Null out after free** (`free(p); p = NULL;`) where pointers outlive the block; a second `free` then becomes a no-op and a stale dereference crashes deterministically.
4. **Pair allocation with size** in a struct; never recompute from untrusted input twice; use `size_t` and check overflow.
5. **Zero secrets before freeing** (`explicit_bzero`, `memset_explicit` C23: plain `memset` may be optimized away).
6. **Avoid allocation in hot paths**: reuse buffers, pre-size, use arenas/pools.
7. Don't `alloca`/VLA with untrusted sizes (stack exhaustion); prefer fixed-size stack buffers for small cases plus heap fallback.

## Growth policy: why containers double

Appending to a dynamic array needs a growth rule. Doubling gives **amortized O(1)** appends (total copy work < 2n); adding a constant is O(n²) in the worst case. Measured with `realloc` (`growth.c`):

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

/* Append n ints, growing capacity with the given policy; return seconds and count how often the block moved. */
static double fill(size_t n, int geometric, int *moves) {
    double t0 = now();
    int *a = NULL; size_t cap = 0; void *prev = NULL; *moves = 0;
    for (size_t i = 0; i < n; i++) {
        if (i == cap) {
            cap = geometric ? (cap ? cap * 2 : 16) : cap + 1024;     /* x2 vs +1024 */
            int *t = realloc(a, cap * sizeof *a);
            if (!t) exit(1);
            a = t;
            if ((void *)a != prev) (*moves)++;
            prev = a;
        }
        a[i] = (int)i;
    }
    double dt = now() - t0;
    free(a);
    return dt;
}

int main(void) {
    int m;
    for (size_t n = 250000; n <= 1000000; n *= 2) {
        double g = fill(n, 1, &m);
        printf("n=%8zu  doubling: %7.2f ms (moved %2d times)   ", n, g * 1e3, m);
        double c = fill(n, 0, &m);
        printf("+1024: %8.2f ms (moved %4d times)\n", c * 1e3, m);
    }
    return 0;
}
```

```text
$ gcc -O2 -o growth growth.c && ./growth
n=  250000  doubling:    0.92 ms (moved  4 times)   +1024:     0.50 ms (moved    1 times)
n=  500000  doubling:    1.46 ms (moved  2 times)   +1024:     0.67 ms (moved    1 times)
n= 1000000  doubling:    1.83 ms (moved  2 times)   +1024:     1.38 ms (moved    1 times)
```

Surprise: on glibc the constant-growth variant was *faster*, with the block moving only once. For large blocks glibc `realloc` uses **`mremap`**, which relocates pages in the page table without copying, so growth is cheap regardless of policy (and the doubling run touched more fresh pages). The theory still matters: small blocks are copied, other allocators and platforms (musl, Windows, embedded) copy, and non-realloc containers (e.g. a hash table rehash, arrays of structs with interior pointers) always do. **Choose geometric growth (×1.5 or ×2) as the portable default**, `reserve`/pre-size when the final size is known, and never assume pointer stability across `realloc`.

## A tested arena allocator

When many objects share a lifetime (a request, a parse, a compile unit, a frame), allocating each with `malloc` and freeing individually is wasteful and error-prone. An **arena** (a.k.a. region, bump allocator) takes one big block, hands out slices by advancing a pointer, and frees everything at once:

```c
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A bump-pointer arena: very fast allocation, freed all at once. */
typedef struct {
    unsigned char *base;
    size_t cap, used;
} Arena;

static int arena_init(Arena *a, size_t cap) {
    a->base = malloc(cap);
    a->cap = cap;
    a->used = 0;
    return a->base != NULL;
}

static void *arena_alloc(Arena *a, size_t size, size_t align) {
    uintptr_t cur = (uintptr_t)a->base + a->used;
    uintptr_t aligned = (cur + (align - 1)) & ~(uintptr_t)(align - 1);   /* align must be a power of two */
    size_t offset = (size_t)(aligned - (uintptr_t)a->base);
    if (offset > a->cap || size > a->cap - offset) return NULL;           /* overflow-safe bounds check */
    a->used = offset + size;
    return a->base + offset;
}

static void arena_reset(Arena *a) { a->used = 0; }
static void arena_free(Arena *a) { free(a->base); a->base = NULL; }

#define ARENA_NEW(a, T) ((T *)arena_alloc((a), sizeof(T), alignof(T)))
#define ARENA_ARRAY(a, T, n) ((T *)arena_alloc((a), sizeof(T) * (n), alignof(T)))

typedef struct Node { int value; struct Node *next; } Node;

int main(void) {
    Arena a;
    if (!arena_init(&a, 1 << 16)) return 1;

    Node *head = NULL;
    for (int i = 0; i < 1000; i++) {           /* 1000 nodes, no individual frees */
        Node *n = ARENA_NEW(&a, Node);
        n->value = i; n->next = head; head = n;
    }
    char *tag = arena_alloc(&a, 3, 1);          /* odd size, then an aligned double */
    double *d = ARENA_NEW(&a, double);
    memcpy(tag, "ab", 3);
    *d = 3.14;
    printf("nodes: head=%d used=%zu bytes of %zu; double aligned: %s\n", head->value, a.used, a.cap,
           ((uintptr_t)d % alignof(double)) == 0 ? "yes" : "NO");
    printf("big request fails cleanly: %s\n", ARENA_ARRAY(&a, double, 1000000) ? "BUG" : "NULL");
    arena_reset(&a);                            /* everything freed in O(1) */
    printf("after reset used=%zu\n", a.used);
    arena_free(&a);
    return 0;
}
```

```text
$ gcc -std=c11 -O2 -Wall -Wextra -fsanitize=address,undefined arena.c -o arena && ./arena
nodes: head=999 used=16016 bytes of 65536; double aligned: yes
big request fails cleanly: NULL
after reset used=0
```

Points to notice: the alignment rounding `(cur + align-1) & ~(align-1)` requires a power-of-two alignment; the bounds check is written as `size > cap - offset` so that it **cannot overflow**; allocation is a few instructions; per-object `free` doesn't exist; `arena_reset` makes reuse O(1); and no individual destructors run (store only plain data, or register cleanup separately). Variants: chained arenas (grow by adding blocks), scratch/temp arenas with save/restore marks (`used = mark`), per-thread arenas to avoid locks, `mmap`-backed arenas with guard pages. This is the pattern behind Apache's APR pools, compilers (LLVM `BumpPtrAllocator`), game engines' frame allocators, and Go/Zig allocator APIs.

Other custom allocators:

| Allocator | Idea | Use |
|---|---|---|
| **Pool / object pool** | fixed-size blocks in a free list (`next` pointer stored inside free blocks) | many same-sized objects (nodes, connections); O(1) alloc/free, no fragmentation |
| **Slab** | pools per type with constructed-object caching, per-CPU caches | Linux kernel `kmem_cache`, `kmalloc` |
| **Buddy** | power-of-two blocks split/merged with a buddy | physical page allocator, simple coalescing |
| **Stack/linear with marks** | LIFO allocation | temporaries |
| **Free-list (first/best fit)** | general variable sizes with coalescing | simple `malloc` replacements, embedded |

## Memory in long-running processes

- **Fragmentation**: free memory split into pieces too small to reuse; RSS grows though live data doesn't. Mitigate with size classes, arenas by lifetime, jemalloc/tcmalloc, periodic restart, `MALLOC_ARENA_MAX`.
- **Leaks vs. caches**: monotonically growing RSS under steady load is a leak; plateau is a cache. Heap profilers (`valgrind --tool=massif`, `heaptrack`, `jemalloc prof`, `tcmalloc heap profile`) show growth by allocation site.
- **`malloc_usable_size`** reveals padding; **`mallinfo2`/`malloc_stats()`** print arenas and mmapped bytes.
- **Fork** duplicates the allocator state copy-on-write; avoid `malloc` in signal handlers (not async-signal-safe).

## Key takeaways

- `malloc` is uninitialized, `calloc` zeroes and checks overflow, `realloc` can move or fail (use a temporary), `free(NULL)` is safe; always size with overflow-checked arithmetic.
- The allocator keeps metadata beside your data (chunks, bins, tcache), so heap bugs corrupt it; freed memory usually stays in the process.
- Run tests under AddressSanitizer/LeakSanitizer (and Valgrind for uninitialized reads); the reports give exact allocation and free stacks.
- Decide ownership, use a single cleanup path, and null pointers after free; zero secrets explicitly.
- For many same-lifetime objects use an arena (bump allocator with an overflow-safe bounds check); use pools/slabs for fixed-size objects.
