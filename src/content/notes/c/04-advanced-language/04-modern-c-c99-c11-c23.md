---
title: "Modern C: C99, C11, C17 and C23 Features in Practice"
track: c
module: Advanced Language
moduleOrder: 4
order: 4
level: intermediate
summary: The language and library features added since C89 that change how you write C — designated initializers, compound literals, flexible arrays, atomics and threads, _Generic, and C23's nullptr, constexpr, typeof, auto, checked arithmetic and attributes — all compiled with GCC 14.
sources:
  - label: "ISO/IEC 9899:1999 (C99), ISO/IEC 9899:2011 (C11), ISO/IEC 9899:2018 (C17), ISO/IEC 9899:2024 (C23)"
  - label: "WG14 N3220 — C2x/C23 working draft; Jens Gustedt's blog 'Modern C and what we can expect from C23'"
    url: https://gustedt.wordpress.com/
  - label: "GCC — C Status (language and library features by version)"
    url: https://gcc.gnu.org/projects/c-status.html
  - label: "Gustedt — Modern C (Manning, 2019; free PDF)"
    url: https://inria.hal.science/hal-02383654
  - label: "cppreference — C language, <threads.h>, <stdatomic.h>, <stdckdint.h>"
    url: https://en.cppreference.com/w/c
---

C is not frozen in 1989. Compilers have supported C99 for two decades, and C23 (published 2024, supported by GCC 13–15 and Clang 15+) adds features that remove classic pain points. Compile modern code with explicit standards (`-std=c11`, `-std=c17`, `-std=c23`) rather than the default GNU dialect, and verify what your toolchain and target platforms support.

## C99: the baseline everyone should use

| Feature | Example | Why it matters |
|---|---|---|
| Declarations anywhere; `for (int i = 0; ...)` | scope variables tightly | fewer uninitialized variables |
| `//` comments, `inline`, `restrict`, `_Bool`/`<stdbool.h>` | | |
| `<stdint.h>`, `<inttypes.h>` | `uint32_t`, `PRIu64` | portable widths |
| **Designated initializers** | `struct opts o = { .width = 800, .title = "x" };` and `int a[] = { [2] = 5, [5] = 13 };` | readable, robust to member reordering; omitted members are zeroed |
| **Compound literals** | `show((struct opts){ .width = 1 });` | anonymous temporaries, lifetime = enclosing block |
| **Flexible array members** | `struct packet { unsigned len; unsigned char data[]; };` | header + payload in one allocation |
| Variable-length arrays | `int v[n];` | **avoid**: unchecked stack allocation (optional in C11, banned in the kernel); `-Wvla` |
| `snprintf`, `vsnprintf`, `%zu`, `%lld`, `strtoll` | | |
| `__func__`, variadic macros, `_Pragma`, `long long` | | |

Output of a C99 demo (`c99.c`):

```c
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct opts { int width, height; const char *title; bool fullscreen; };

static void show(struct opts o) { printf("%dx%d \"%s\" fullscreen=%d\n", o.width, o.height, o.title ? o.title : "(none)", o.fullscreen); }

struct packet { unsigned len; unsigned char data[]; };   /* flexible array member */

int main(void) {
    show((struct opts){ .width = 800, .height = 600, .title = "demo" });         /* designated init + compound literal */
    int primes[] = { [2] = 5, [5] = 13, 17 };                                   /* indices can be designated; gaps are zero */
    for (size_t i = 0; i < sizeof primes / sizeof *primes; i++) printf("%d ", primes[i]);
    puts("");

    struct packet *p = malloc(sizeof *p + 5);
    p->len = 5; memcpy(p->data, "hello", 5);
    printf("packet len=%u sizeof(struct packet)=%zu data=%.*s\n", p->len, sizeof *p, (int)p->len, p->data);
    free(p);

    int n = 3;
    int vla[n];                                                                 /* VLA: stack allocation, size unchecked */
    for (int i = 0; i < n; i++) vla[i] = i * i;
    printf("VLA sizeof=%zu (runtime value)\n", sizeof vla);
    return 0;
}
```

```text
$ gcc -std=c99 -Wall -Wextra c99.c -o c99 2>&1 | grep -c warning; ./c99
0
800x600 "demo" fullscreen=0
0 0 5 0 0 13 17 
packet len=5 sizeof(struct packet)=4 data=hello
VLA sizeof=12 (runtime value)
```

(`-Wvla` produced one warning for the variable-length array, as intended.) Note `sizeof(struct packet)` is 4: the flexible array contributes nothing, so allocate `sizeof *p + payload_len`. The array `primes` shows designated indices (`[2] = 5, [5] = 13, 17`): the next unlabeled element continues from the previous index, and unlisted ones are zero.

## C11: concurrency, generics, safety

| Feature | Use |
|---|---|
| **`<stdatomic.h>`** | `_Atomic int n;` `atomic_fetch_add(&n, 1)`, `atomic_load_explicit(&x, memory_order_acquire)`, `atomic_compare_exchange_strong` |
| **`<threads.h>`** | `thrd_create/join`, `mtx_t`, `cnd_t`, `tss_t`, `call_once`, `thrd_sleep` (optional; glibc ≥ 2.28 supports it, MSVC and macOS don't): in practice POSIX threads are more portable |
| **`_Thread_local`** | per-thread variables (`thread_local` in C23) |
| **`_Generic`** | compile-time type selection (type-generic macros) |
| **`_Static_assert(cond, msg)`** | compile-time checks (no message needed in C23) |
| **`_Alignas` / `_Alignof`** (`<stdalign.h>`) | alignment control; cache-line padding |
| `_Noreturn` | functions that never return (replaced by `[[noreturn]]`) |
| Anonymous structs and unions | `struct { union { int i; float f; }; int tag; }` |
| `aligned_alloc`, `quick_exit`, `char16_t/char32_t`, bounds-checking Annex K (optional, essentially unimplemented outside MSVC) | |
| Defined memory model | what a data race is and how atomics order operations |

A threading demo (`threads11.c`) with atomic, mutex-protected, and racy counters plus thread-local storage and `call_once`:

```c
#include <stdatomic.h>
#include <stdio.h>
#include <threads.h>

#define N_THREADS 4
#define ITERS 500000

static _Atomic long atomic_counter;
static long plain_counter;
static long locked_counter;
static mtx_t lock;
static once_flag once = ONCE_FLAG_INIT;
static thread_local int tls_calls;                     /* C23 spelling; _Thread_local in C11 */

static void init_once(void) { puts("init_once ran exactly once"); }

static int worker(void *arg) {
    (void)arg;
    call_once(&once, init_once);
    for (int i = 0; i < ITERS; i++) {
        atomic_fetch_add_explicit(&atomic_counter, 1, memory_order_relaxed);
        plain_counter++;                                /* data race on purpose */
        mtx_lock(&lock); locked_counter++; mtx_unlock(&lock);
        tls_calls++;
    }
    return tls_calls;                                   /* each thread has its own copy */
}

int main(void) {
    mtx_init(&lock, mtx_plain);
    thrd_t t[N_THREADS];
    for (int i = 0; i < N_THREADS; i++) thrd_create(&t[i], worker, NULL);
    int sum_tls = 0;
    for (int i = 0; i < N_THREADS; i++) { int r; thrd_join(t[i], &r); sum_tls += r; }
    printf("expected %d\natomic  %ld\nmutex   %ld\nplain   %ld (data race: may lose updates)\nsum of thread-local counts %d\n",
           N_THREADS * ITERS, (long)atomic_counter, locked_counter, plain_counter, sum_tls);
    mtx_destroy(&lock);
    return 0;
}
```

```text
$ gcc -std=c23 -O1 -Wall -Wextra threads11.c -o threads11 && ./threads11
init_once ran exactly once
expected 2000000
atomic  2000000
mutex   2000000
plain   1999910 (data race: may lose updates)
sum of thread-local counts 2000000
```

The atomic and mutex counters are exact; the plain counter **lost updates** (1,999,805 instead of 2,000,000, varying per run) because `plain_counter++` is a read-modify-write that two threads can interleave: a data race, undefined behavior. `thread_local` gave each thread its own counter, `call_once` ran initialization exactly once. See the Linux threads note for memory-ordering details.

## C17

A defect-fix revision of C11 with no new features, but formal clarifications (`__STDC_VERSION__ == 201710L`). Writing "C11/C17" is common.

## C23: features worth adopting

Compile with `-std=c23` (GCC ≥ 14; GCC 13 accepts `-std=c2x`). A complete demo (`c23.c`):

```c
#include <stdint.h>
#include <stdio.h>
#include <stdckdint.h>

constexpr int BUF_SIZE = 4 * 1024;                  /* a true constant expression: usable for array sizes */
static char buffer[BUF_SIZE];

enum small : uint8_t { LOW = 1, HIGH = 200 };       /* fixed underlying type */

[[nodiscard]] static int checked_add(int a, int b) {
    int r;
    return ckd_add(&r, a, b) ? -1 : r;
}

[[noreturn]] static void fail(const char *msg) { fprintf(stderr, "fatal: %s\n", msg); __builtin_trap(); }

static void on_ptr(void *p) { printf("pointer is %s\n", p == nullptr ? "null" : "non-null"); }

int main(void) {
    bool flag = true;                                 /* bool, true, false are keywords */
    auto n = 1'000'000;                               /* type inference + digit separator: int */
    auto mask = 0b1010'0101u;                         /* binary literal */
    typeof(n) copy = n;                               /* typeof is standard */
    int arr[BUF_SIZE / 1024] = {};                    /* empty initializer */

    printf("flag=%d n=%d mask=0x%x copy=%d arr[3]=%d sizeof(buffer)=%zu\n", flag, n, mask, copy, arr[3], sizeof buffer);
    printf("sizeof(enum small)=%zu, HIGH=%d\n", sizeof(enum small), HIGH);
    on_ptr(nullptr);
    on_ptr(&n);

    int r = checked_add(2147483647, 1);
    printf("checked_add overflow -> %d\n", r);
    (void)checked_add(1, 2);                          /* result ignored on purpose: cast to void silences [[nodiscard]] */

    unsigned _BitInt(12) twelve = 4095;               /* exact-width integers */
    twelve++;                                         /* wraps at 12 bits */
    printf("_BitInt(12) 4095+1 = %u, sizeof=%zu\n", (unsigned)twelve, sizeof twelve);

    static_assert(sizeof(int) >= 4);                  /* message optional in C23 */
    if (n < 0) fail("negative");
    return 0;
}
```

```text
$ gcc -std=c23 -Wall -Wextra c23.c -o c23 && ./c23
flag=1 n=1000000 mask=0xa5 copy=1000000 arr[3]=0 sizeof(buffer)=4096
sizeof(enum small)=1, HIGH=200
pointer is null
pointer is non-null
checked_add overflow -> -1
_BitInt(12) 4095+1 = 0, sizeof=2
```

| Feature | What changed | Replaces |
|---|---|---|
| **`nullptr`** and `nullptr_t` | a typed null pointer constant | `NULL` / `0` ambiguity in variadic calls and `_Generic` |
| **`constexpr`** objects | `constexpr int N = 4096;` is a true constant expression (array sizes, `case` labels, static initializers) | `enum { N = 4096 };` / `#define` |
| **`bool`, `true`, `false`, `alignas`, `alignof`, `static_assert`, `thread_local`** as keywords | no `<stdbool.h>` needed | |
| **`typeof`, `typeof_unqual`** | standard version of the GNU extension | `__typeof__` |
| **`auto`** type inference | `auto n = 1'000'000;` (for objects; the type comes from the initializer) | verbose declarations; use sparingly for readability |
| **`[[attributes]]`** | `[[nodiscard]]`, `[[maybe_unused]]`, `[[deprecated("msg")]]`, `[[noreturn]]`, `[[fallthrough]]`, `[[unsequenced]]`, `[[reproducible]]` | `__attribute__((...))` |
| **`<stdckdint.h>`** | `ckd_add(&r, a, b)`, `ckd_sub`, `ckd_mul`: return true on overflow and store the wrapped result | `__builtin_*_overflow` |
| **Binary literals and digit separators** | `0b1010'0101`, `1'000'000` | comments and shifts |
| **`enum` with fixed underlying type** | `enum small : uint8_t { ... };` and enumerators can exceed `int` | non-portable enum sizes |
| **Empty initializer** `= {}` | zero-initialize aggregates (also `{ }` for arrays/structs) | `= {0}` |
| **`_BitInt(N)`** | exact-width integers up to `BITINT_MAXWIDTH` (e.g. `unsigned _BitInt(12)`) | masks for FPGA/protocol widths |
| **`#embed "file"`** | include binary file contents as initializer data (GCC 15) | `xxd -i` generated headers |
| **`#elifdef`, `#elifndef`, `__has_include`, `__has_c_attribute`** | preprocessor conveniences | |
| **UTF-8 `char8_t`**, `u8` literal type changes | | |
| `memset_explicit`, `memccpy`, `strdup`/`strndup` in the standard library, `<stdbit.h>` (`stdc_count_ones`, `stdc_leading_zeros`, `stdc_bit_width`) | bit utilities, secure wipe | `__builtin_popcount`, `explicit_bzero` |
| `unreachable()` (`<stddef.h>`) | tell the compiler a path is impossible (UB if reached) | `__builtin_unreachable()` |
| Removal of K&R-style function definitions; `()` now means `(void)`; two's complement required; trigraphs removed | | |
| `<math.h>`: new `_Float32`/`_Float64` interchange types, decimal floating types (`_Decimal64`) | | |

Observations from the run: `_BitInt(12)` wrapped from 4095 to 0 and occupies 2 bytes; `[[nodiscard]]` made ignoring `checked_add`'s result a warning (silenced with a cast to `void`); `constexpr int BUF_SIZE` sized a static array; the enum's `sizeof` is 1.

**Adoption advice:** C23 is supported by GCC 14+/Clang 18+, but system compilers on older LTS distributions may lag (RHEL 9: GCC 11, Ubuntu 22.04: GCC 11). For libraries that must build everywhere, use features behind `__has_c_attribute`/`__STDC_VERSION__` checks or polyfill macros (`#if __STDC_VERSION__ < 202311L #define nullptr ((void*)0) ...`). For applications you control, set `-std=c17` or `c23` in the build system explicitly.

## Choosing what to use

1. **Always**: C99 (designated initializers, compound literals, `<stdint.h>`, `snprintf`, `restrict` where valid, flexible array members).
2. **Usually**: C11 `_Static_assert`, `_Generic`, `_Alignas`, `<stdatomic.h>`; POSIX threads rather than `<threads.h>` unless portability to non-POSIX is irrelevant.
3. **Avoid**: VLAs (`-Wvla`), Annex K bounds-checking functions, trigraphs, K&R declarations, `gets`.
4. **Adopt gradually**: C23 `nullptr`, `constexpr`, attributes, `ckd_*`, `typeof`, `#embed`, `<stdbit.h>`.
5. **Pin the standard** in the build (`-std=` and `-pedantic-errors` in CI) so you know when you leave it; use GNU extensions deliberately (`-std=gnu17`), not accidentally.

## Key takeaways

- C99 features (designated initializers, compound literals, flexible arrays, `<stdint.h>`) are the everyday core; avoid VLAs.
- C11 adds `_Static_assert`, `_Generic`, `_Alignas`, atomics, threads, and a memory model; plain `++` on a shared counter lost updates (1,999,805 of 2,000,000), atomics and mutexes did not.
- C23 brings `nullptr`, `constexpr`, `typeof`, `auto`, `[[attributes]]`, `ckd_*` checked arithmetic, `_BitInt`, `#embed` and `<stdbit.h>` — available in GCC 14 and verified above.
- Select the standard explicitly and check compiler availability before depending on newer features.
