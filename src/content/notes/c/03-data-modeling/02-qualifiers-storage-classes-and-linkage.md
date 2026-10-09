---
title: Qualifiers, Storage Classes, Scope and Linkage
track: c
module: Data Modeling
moduleOrder: 3
order: 2
level: advanced
summary: What const, volatile, restrict and _Atomic promise (and the optimizer does with them), the difference between scope, lifetime and linkage, tentative definitions, the three meanings of static, and C99 inline semantics, each demonstrated.
sources:
  - label: "ISO/IEC 9899:2018 (C17) §6.2.1 Scopes of identifiers; §6.2.2 Linkages; §6.2.4 Storage durations; §6.7.3 Type qualifiers; §6.7.4 Function specifiers; §6.9.2 External object definitions"
  - label: "GCC manual — An Inline Function is As Fast As a Macro; Common Variable Attributes; -fcommon"
    url: https://gcc.gnu.org/onlinedocs/gcc/Inline.html
  - label: "Regehr — Volatile (and \"Nine ways to break your systems code using volatile\")"
    url: https://blog.regehr.org/archives/28
  - label: "SEI CERT C — DCL, EXP40-C (do not modify constant objects), EXP43-C (restrict)"
    url: https://wiki.sei.cmu.edu/confluence/display/c/Rule+02.+Declarations+and+Initialization+%28DCL%29
---

Three independent properties describe every identifier. Mixing them up is the root of many link errors and "why does this keep its value?" surprises.

| Property | Question | Values |
|---|---|---|
| **Scope** | Where in the source can the *name* be used? | file, block, function prototype, function (labels) |
| **Storage duration (lifetime)** | When does the *object* exist? | static (whole program), automatic (block), thread, allocated |
| **Linkage** | Do the same names in different places refer to the *same entity*? | external (across translation units), internal (within this file), none |

## Storage classes

| Specifier | Meaning |
|---|---|
| `auto` | default for block-scope variables (C23 reuses `auto` for type inference) |
| `register` | hint to keep in a register; you can't take the address; ignored by modern compilers (removed in C++17) |
| `static` | see below: three different meanings |
| `extern` | declares an identifier defined elsewhere (external linkage) |
| `_Thread_local` (C11, `thread_local` in C23) | one instance per thread; static duration per thread |
| `typedef` | syntactically a storage class, creates a type alias |

### The three meanings of `static`

1. **File-scope** `static int x; static void f(void)` → **internal linkage**: the name is private to this translation unit. Use it for everything not part of the file's public interface.
2. **Block-scope** `static int id` inside a function → **static storage duration**: one object for the program's lifetime, initialized once (to zero or the constant initializer), retaining its value between calls, but still block scope (not visible elsewhere). Not thread-safe unless atomic or protected; makes functions non-reentrant.
3. **Array parameter** `void f(int a[static 10])` (C99) → promise that `a` points to at least 10 elements (an optimization/diagnostic hint).

```c
static int next_id(void) { static int id = 100; return id++; }
int a = next_id(), b = next_id(), c = next_id();   /* a=100 b=101 c=102 */
```

(An earlier draft of this demo printed `next_id()` three times in one `printf` and showed `102 101 100`, because GCC evaluates arguments right to left: unspecified order again.)

```c
#include <stdio.h>
static int next_id(void) { static int id = 100; return id++; }      /* persists across calls, initialized once */
_Thread_local int per_thread = 7;                                    /* one copy per thread */
int main(void) {
    int a = next_id(), b = next_id(), c = next_id();                 /* separate statements: argument order is unspecified */
    printf("%d %d %d\n", a, b, c);
    printf("per_thread=%d\n", per_thread);
    return 0;
}
```

```text
$ gcc -std=c11 statics.c -o statics && ./statics
100 101 102
per_thread=7
```

### Lifetime and initialization

- Objects with **static** or **thread** duration are initialized before `main` to zero unless an initializer is given; initializers must be constant expressions in C (no function calls: that's why C++ static-init-order problems don't exist, but C23 `constexpr` extends what counts as constant).
- **Automatic** variables are **uninitialized** (indeterminate) until assigned. Reading them is UB.
- Uninitialized globals go to `.bss` (no file size); initialized ones to `.data`; `const` objects with constant initializers to `.rodata` (read-only, shared between processes).

## Linkage

- **External linkage**: file-scope variables and functions without `static`. One definition in the whole program; declared in headers via `extern`.
- **Internal linkage**: `static` at file scope, or `const` variables in C++ (not C).
- **No linkage**: block-scope variables, parameters, typedefs.

### Declarations, definitions and tentative definitions

`extern int x;` is a **declaration**; `int x = 5;` a **definition**; `int x;` at file scope with no initializer is a **tentative definition**: legal to repeat within one file and equal to a definition with initializer 0 if no other appears. Historically many compilers placed tentative definitions in "common" storage so that **different files could each write `int counter;` and the linker would merge them**: a silent bug source. GCC 10 switched the default to `-fno-common`, making the second definition a link error; two files from the experiment:

```c
/* a.c */ int counter;  static int hidden = 1;  void bump(void);  int main(void){ bump(); bump(); printf("counter=%d hidden=%d\n", counter, hidden); }
/* b.c */ int counter;  static int hidden = 100; void bump(void){ counter++; hidden++; }
```

```text
$ gcc -std=c11 a.c b.c -o ab 2>&1 | grep -E 'multiple definition' | head -1 | cut -c1-120; gcc -std=c11 -fcommon a.c b.c -o ab_common && ./ab_common
/bin/ld: /tmp/cc4ce80S.o:(.bss+0x0): multiple definition of `counter'; /tmp/ccLT1uOo.o:(.bss+0x0): first defined here
counter=2 hidden=1
```

With `-fcommon` it links and prints `counter=2 hidden=1`: the two `counter`s were merged into one, while the two `static hidden`s stayed separate (each file kept its own). Keep `-fno-common`, put exactly one definition in a `.c` file, and declare with `extern` in a header included by both.

Header rules: only declarations, `static inline` functions, macros, types and `extern` variable declarations; never ordinary function or variable *definitions* (they'd be duplicated in every including file → multiple definition errors).

## Type qualifiers

### `const`

`const` says "this access path won't modify the object". It is a compile-time check plus an optimization hint, and it documents intent. Reading pointer declarations right-to-left:

```c
const char *p;        /* pointer to const char: can't modify *p, can reseat p */
char *const q = buf;  /* const pointer to char: can modify *q, can't reseat q */
const char *const r;  /* both fixed */
void print(const struct item *it);   /* promise: this function won't modify the pointee */
```

- Make **input pointers `const`** (`strlen(const char *)`) so callers can pass both mutable and read-only data, and the compiler catches accidental writes. `const` does not propagate through nested pointers: `char **` → `const char **` isn't implicitly allowed (a deliberate safety rule that prevents smuggling a const pointer into a mutable slot), so such conversions need an explicit cast.
- **Modifying an object that was *defined* `const` is undefined behavior**, even through a cast. The compiler may place it in read-only memory or constant-fold uses:

```c
#include <stdio.h>
int main(void) {
    const int ro = 5;
    int *p = (int *)&ro;           /* casting away const and writing is UB */
    *p = 6;
    fprintf(stderr, "ro=%d *p=%d\n", ro, *p);   /* the compiler may print 5 and 6 for the same object */
    char *s = "literal";
    s[0] = 'L';                    /* UB: string literal lives in read-only memory */
    fprintf(stderr, "%s\n", s);
    return 0;
}
```

```text
$ gcc -O1 -Wwrite-strings constdemo.c -o constdemo 2>&1 | grep warning | cut -c1-110; ./constdemo; echo "exit status: $?"
constdemo.c:7:15: warning: initialization discards ‘const’ qualifier from pointer target type [-Wdiscarded
exit status: 139
ro=5 *p=6
/bin/bash: line 1: 2427679 Segmentation fault      (core dumped) ./constdemo
```

  The same object printed as `5` (folded constant) and `6` (via the pointer) in one statement, and the write to the **string literal** crashed with `SIGSEGV` (status 139) because literals live in `.rodata`. Declare literal pointers as `const char *`; use `char s[] = "..."` when you need a writable copy.
- Casting away `const` is legitimate only when the underlying object is not actually const (e.g. wrapping a legacy API like `strchr`, which takes `const char *` and returns `char *`, a known hole).
- `const` data can be shared and lives in `.rodata`: prefer `static const` tables to computed-at-startup arrays.

### `volatile`

Every access to a `volatile` object is an observable side effect: the compiler must perform each read and write exactly as written, in order with other volatile accesses, and may not cache the value in a register. Valid uses: memory-mapped hardware registers, variables modified by signal handlers (`volatile sig_atomic_t`), variables modified between `setjmp` and `longjmp`. Look at what it changes:

```c
int flag;                          /* set by a signal handler or another thread */
volatile int vflag;

void wait_plain(void)    { while (!flag)  ; }
void wait_volatile(void) { while (!vflag) ; }
```

```text
$ gcc -O2 -S -fno-asynchronous-unwind-tables -fcf-protection=none vol.c -o - | grep -E '^(wait_|\.L|\s+(movl|testl|je|jmp|ret))'
wait_plain:
	movl	flag(%rip), %eax
	testl	%eax, %eax
	je	.L3
	ret
.L3:
	jmp	.L3
wait_volatile:
.L6:
	movl	vflag(%rip), %eax
	testl	%eax, %eax
	je	.L6
	ret
```

`wait_plain` reads `flag` once, then **spins forever on `jmp .L3`** (the compiler assumed nobody else changes it); `wait_volatile` reloads `vflag` on every iteration. Therefore: signal handlers must communicate through `volatile sig_atomic_t`, and device registers must be `volatile`. But **`volatile` is not synchronization**: it provides no atomicity, no memory ordering between CPUs and no protection against data races. For threads use `_Atomic`/`<stdatomic.h>` or a mutex. Also `volatile` doesn't make a read-modify-write (`v++`) atomic.

### `restrict`

A `restrict` pointer promises that, within its scope, the object it points to is accessed *only through that pointer* (or pointers derived from it). The compiler may then reorder and vectorize without worrying about aliasing. Violating the promise is UB (that is why `memcpy`'s parameters are `restrict` and `memmove`'s are not).

```c
void add_alias(int *dst, const int *a, const int *b, int n) {
    for (int i = 0; i < n; i++) dst[i] = a[i] + b[i];
}
void add_restrict(int *restrict dst, const int *restrict a, const int *restrict b, int n) {
    for (int i = 0; i < n; i++) dst[i] = a[i] + b[i];
}
```

```text
$ gcc -O3 -fopt-info-vec-optimized -c restrict.c -o /dev/null 2>&1 | cut -c1-110
restrict.c:2:23: optimized: loop vectorized using 16 byte vectors
restrict.c:2:23: optimized:  loop versioned for vectorization because of possible aliasing
restrict.c:2:23: optimized: loop vectorized using 8 byte vectors
restrict.c:5:23: optimized: loop vectorized using 16 byte vectors
restrict.c:5:23: optimized: loop vectorized using 8 byte vectors
```

Without `restrict`, GCC vectorizes `add_alias` only after generating a **runtime overlap check** ("loop versioned for vectorization because of possible aliasing", line 2): both a vector and a scalar copy of the loop exist. The `restrict` version (line 5) needs no check. It matters most in numeric kernels. Use it only when you can guarantee non-overlap, and document the contract.

### `_Atomic`

`_Atomic int counter;` makes accesses atomic (and sequentially consistent by default) via `<stdatomic.h>` operations `atomic_fetch_add`, `atomic_load_explicit`... (see the threads and C11 notes).

## Function specifiers

- **`inline`** (C99): a *hint* to substitute the body at call sites. Its linkage semantics differ from C++/GNU89:
  - `inline int f(int x) {...}` in C99/C11 provides an **inline definition only**; the compiler *may* inline, but if it decides to emit a call (e.g. at `-O0`) it needs an **external definition** in some translation unit, otherwise: `undefined reference`.
  - Provide that by repeating the declaration with `extern` in exactly one `.c` file (`extern inline int f(int);`), or use the simpler convention below.
  - **`static inline`** in a header is the robust, portable pattern: each translation unit gets its own private copy that the compiler may inline or emit.

```c
#include <stdio.h>
inline int twice(int x) { return 2 * x; }     /* C99 inline definition: no external definition emitted here */
int main(void) { printf("%d\n", twice(21)); return 0; }
```

```text
$ gcc -std=c11 -O0 inl.c -o inl 2>&1 | grep -E 'undefined reference' | head -1; gcc -std=c11 -O2 inl.c -o inl_o2 && ./inl_o2; gcc -std=c11 -O0 inl2.c -o inl2 && ./inl2
inl.c:(.text+0xa): undefined reference to `twice'
42
42
```

  At `-O0` the call isn't inlined and the program fails to link; at `-O2` it was inlined and works (an inconsistency that hides the bug until the optimization level changes); the `static inline` version (`inl2.c`) works at every level. `__attribute__((always_inline))` / `noinline` override the heuristics; `inline` doesn't guarantee inlining and excessive inlining bloats code and instruction-cache footprint.
- **`_Noreturn`** / `[[noreturn]]` (C23): the function never returns (`exit`, `abort`); enables dead-code elimination and silences warnings.

## Scope and name spaces

- **File scope**: declared outside any function; visible from the point of declaration to the end of the translation unit.
- **Block scope**: from the declaration to the end of the enclosing `{}`; inner declarations **shadow** outer ones (`-Wshadow` finds the bugs).
- **Function prototype scope**: names in prototypes are for documentation only.
- **Labels** have function scope (`goto`).
- C has separate **name spaces** for ordinary identifiers, struct/union/enum **tags**, members (per struct) and labels, hence `struct stat stat;` is legal and `typedef struct node node;` is a common idiom.
- Reserved identifiers: leading underscore + uppercase or double underscore (`_Foo`, `__x`) everywhere; leading underscore at file scope; names beginning with `str`, `mem`, `is`, `to`, `E` + digit, `SIG`, `int*_t` are reserved for the library: defining them is UB.

## Organizing a module

```c
/* list.h  — the public interface */
#ifndef LIST_H
#define LIST_H
#include <stddef.h>
typedef struct List List;                 /* opaque type: the layout is hidden */
List *list_new(void);
int   list_push(List *l, int v);          /* documented ownership and errors */
void  list_free(List *l);
extern const int LIST_MAX;                /* declaration of a constant defined in list.c */
#endif

/* list.c  — the implementation: everything not in the header is static */
#include "list.h"
struct List { int *v; size_t n, cap; };
const int LIST_MAX = 1 << 20;
static int grow(List *l);                 /* private helper: internal linkage */
```

This gives encapsulation (callers can't touch fields or helpers), fast builds (the header rarely changes), and ABI freedom.

## Key takeaways

- Distinguish scope (visibility), lifetime (storage duration) and linkage (identity across files): `static` at file scope = internal linkage; `static` in a function = persistent object; `extern` = declaration.
- Define each external object exactly once; tentative definitions plus `-fcommon` merged duplicates silently, which is why modern GCC defaults to `-fno-common`.
- `const` is a contract (modifying a const object is UB; literals are read-only); `volatile` forces every access but gives no atomicity or ordering; `restrict` removes aliasing checks (and is a promise you must keep); `_Atomic` is for threads.
- `inline` in C needs an external definition unless you use `static inline`, which is the pattern for headers.
