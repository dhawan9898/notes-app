---
title: C Standards, the Abstract Machine and Undefined Behavior
track: c
module: Toolchain and Language Foundations
moduleOrder: 1
order: 2
level: advanced
summary: The behavior categories the C standard defines, why undefined behavior lets compilers delete your checks, a gallery of real UB with the code GCC generates, and how to catch it with sanitizers and warnings.
prerequisites:
  - "From Source to Running Process: A Bridge to Systems C"
  - Pointers for Beginners
sources:
  - label: "ISO/IEC 9899:2018 (C17) §3.4 Behavior, §4 Conformance, §5.1.2.3 Program execution, Annex J.2 Undefined behavior"
  - label: "ISO/IEC 9899:2024 (C23) and WG14 N3220 working draft"
    url: https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3220.pdf
  - label: "Lattner — What Every C Programmer Should Know About Undefined Behavior (LLVM blog, 2011)"
    url: https://blog.llvm.org/2011/05/what-every-c-programmer-should-know.html
  - label: "Wang et al. — Towards Optimization-Safe Systems - Analyzing the Impact of Undefined Behavior (SOSP 2013)"
  - label: "SEI CERT C Coding Standard"
    url: https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard
  - label: "GCC manual — Language Standards Supported; Options for Debugging (-fsanitize)"
    url: https://gcc.gnu.org/onlinedocs/gcc/Standards.html
---

C is defined by an **abstract machine** in the ISO standard, not by any CPU. A compiler must produce a program whose *observable behavior* (volatile accesses, writes to files, input/output) matches what the abstract machine would do, **for programs that never trigger undefined behavior**. Everything beyond that is the compiler's freedom, and it uses that freedom aggressively. This note explains the rules of that contract.

## The standards

| Edition | Highlights |
|---|---|
| K&R C (1978) | the original; implicit `int`, old-style function definitions |
| **C89/C90** (ANSI/ISO) | prototypes, `void`, `const`, `volatile`, standard library |
| **C99** | `//` comments, declarations anywhere, `inline`, `long long`, `<stdint.h>`, `<stdbool.h>`, designated initializers, compound literals, VLAs (optional later), `restrict`, `snprintf`, flexible array members |
| **C11** | `_Static_assert`, `_Generic`, `<stdatomic.h>`, `<threads.h>`, `_Alignas/_Alignof`, anonymous structs/unions, `_Noreturn`, Unicode strings, bounds-checking Annex K (optional, rarely implemented) |
| **C17** | bug-fix release of C11 (no new features) |
| **C23** | `nullptr`, `constexpr` objects, `bool/true/false/alignas` as keywords, `typeof`, `auto` type inference, `[[attributes]]` (`[[nodiscard]]`, `[[maybe_unused]]`, `[[deprecated]]`), binary literals `0b1010`, digit separators `1'000'000`, `#embed`, `<stdckdint.h>` checked arithmetic, `_BitInt(N)`, `enum` underlying types, empty initializer `= {}`, UTF-8 `char8_t`, removal of K&R function definitions, two's complement mandated |

GCC defaults to a GNU dialect (`-std=gnu17`, `gnu23` in GCC 15) which adds extensions (statement expressions, `typeof`, `__attribute__`, computed goto, `__builtin_*`). Use `-std=c11 -pedantic-errors` (or `-std=c23`) when you want portable, standard-only code, and know that `-std=c11` also disables some POSIX declarations unless you define a feature macro such as `_POSIX_C_SOURCE=200809L` or `_GNU_SOURCE`.

## Kinds of behavior

The standard classifies what a construct may do:

| Category | Meaning | Example | Practical rule |
|---|---|---|---|
| **Well-defined** | exactly specified | unsigned wraparound `UINT_MAX + 1 == 0` | rely on it |
| **Implementation-defined** | implementation chooses and **must document** | whether plain `char` is signed; size of `int`/`long`; `-8 >> 1` | OK if you document the assumption or check with `static_assert` |
| **Unspecified** | one of several allowed behaviors; **not required to be documented or consistent** | order of evaluation of function arguments; layout of padding bytes | never depend on it |
| **Undefined (UB)** | the standard imposes **no requirements at all** | signed overflow, null dereference, out-of-bounds access, data race | must never happen: not even "implementation-specific wrong answers" are guaranteed |
| **Locale-specific** | depends on the locale | `isalpha` results | |
| **Constraint violation** | rule violated that requires a diagnostic | assigning a pointer to an `int` without a cast | compilers must warn; many accept with a warning, so use `-Werror` |

Host values on a typical x86-64 Linux system (`impl.c`):

```c
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
int main(void) {
    printf("char is %s, CHAR_BIT=%d\n", (char)-1 < 0 ? "signed" : "unsigned", CHAR_BIT);
    printf("sizeof short=%zu int=%zu long=%zu long long=%zu void*=%zu size_t=%zu\n",
           sizeof(short), sizeof(int), sizeof(long), sizeof(long long), sizeof(void *), sizeof(size_t));
    printf("-8 >> 1 = %d (implementation-defined for negative values)\n", -8 >> 1);
    printf("INT_MIN / -1 is undefined; (unsigned)-1 = %u\n", (unsigned)-1);
    uint32_t x = 0x01020304; unsigned char *b = (unsigned char *)&x;
    printf("byte order: %s-endian\n", b[0] == 4 ? "little" : "big");
    return 0;
}
```

```text
$ gcc impl.c -o impl && ./impl
char is signed, CHAR_BIT=8
sizeof short=2 int=4 long=8 long long=8 void*=8 size_t=8
-8 >> 1 = -4 (implementation-defined for negative values)
INT_MIN / -1 is undefined; (unsigned)-1 = 4294967295
byte order: little-endian
```

`long` is 8 bytes here (LP64) but 4 on 64-bit Windows (LLP64): never assume. Use `<stdint.h>` fixed-width types where sizes matter and `static_assert(sizeof(long) == 8, "...")` to document assumptions.

### Unspecified: argument evaluation order

```c
#include <stdio.h>
static int n;
static int next(void) { return ++n; }
int main(void) {
    /* Unspecified: the order in which the arguments are evaluated. */
    printf("%d %d\n", next(), next());
    return 0;
}
```

```text
$ gcc order.c -o order_gcc && ./order_gcc
2 1
```

GCC evaluated the arguments right to left and printed `2 1`; other compilers commonly print `1 2`. Both are conforming. (If both arguments modified the *same* variable directly, e.g. `f(i++, i++)`, it would be undefined behavior.) The only guaranteed ordering points are `&&`, `||`, `?:`, the comma operator, the end of a full expression and function-call boundaries (**sequence points** in C99; "sequenced before" in C11).

## Undefined behavior: what it really means

When a program has UB, **the entire execution is meaningless**, not just the offending operation: the compiler may produce code that "worked" in testing, crashes, gives wrong answers, deletes unrelated code, or becomes a security hole. This freedom exists because it makes compiled code fast: the compiler need not emit overflow checks, bounds checks or null checks that the language says the programmer already guarantees. **Optimizers reason as if UB can never happen**, deducing facts from it ("time travel").

### Example 1: signed overflow

```c
/* Does the compiler have to keep this comparison? */
int always_true(int x) { return x + 1 > x; }
```

Signed integer overflow is UB, so the compiler may assume `x + 1` never overflows, hence `x + 1 > x` is always true:

```text
$ gcc -O2 -S -fno-asynchronous-unwind-tables -o - overflow.c | grep -A3 '^always_true:'
always_true:
	movl	$1, %eax
	ret
	.size	always_true, .-always_true
```

The function became `return 1`. At `-O0` the comparison is performed (and wraps on typical hardware), but you can't depend on either. For *unsigned* types overflow is defined (modulo 2^N), which is why `if (x + 1 < x)` works for `unsigned x` but not for `int x`. To check signed overflow portably, test *before* the operation (`if (x > INT_MAX - 1)`) or use `__builtin_add_overflow(a, b, &r)` / C23 `ckd_add`.

### Example 2: the dereference that deletes the null check

```c
/* The dereference happens first, so p must be non-null; the check is "dead". */
int read_checked(int *p) {
    int v = *p;
    if (p == 0)
        return -1;
    return v;
}
```

`*p` precedes the test, so the compiler concludes `p` is non-null (dereferencing a null pointer is UB) and removes the check:

```text
$ gcc -O2 -S -fno-asynchronous-unwind-tables -fcf-protection=none -o - nullcheck.c | grep -A3 '^read_checked:'
read_checked:
	movl	(%rdi), %eax
	ret
	.size	read_checked, .-read_checked
```

No `test`, no branch, no `-1`. With `-fno-delete-null-pointer-checks` (the Linux kernel builds with it, since address 0 can be mapped) the check survives:

```text
$ gcc -O2 -S -fno-asynchronous-unwind-tables -fcf-protection=none -fno-delete-null-pointer-checks -o - nullcheck.c | grep -A6 '^read_checked:'
read_checked:
	movl	(%rdi), %eax
	testq	%rdi, %rdi
	je	.L5
	ret
.L5:
	movl	$-1, %eax
```

This pattern has caused real vulnerabilities (a 2009 Linux kernel `tun` driver bug where the optimizer dropped a null check after a dereference allowed privilege escalation). Rule: validate *before* using.

### A gallery of undefined behavior

| Category | Example |
|---|---|
| Arithmetic | signed overflow; `INT_MIN / -1`; division by zero; shift by negative or ≥ width (`1 << 32`); left-shifting into the sign bit (pre-C23 wording) |
| Memory access | null/dangling/wild pointer dereference; out-of-bounds array or pointer arithmetic (more than one past the end); use after `free` (use-after-free), double `free`; reading uninitialized automatic variables (indeterminate values, trap representations); accessing an object through an incompatible type (strict aliasing); misaligned access; modifying a string literal or a `const` object; `memcpy` with overlapping regions; stack-use-after-return |
| Sequencing | modifying a scalar twice without sequence points (`i = i++ + 1`, `a[i] = i++`) |
| Concurrency | data races on non-atomic variables |
| Control flow | reaching the end of a non-void function and *using* the return value; `longjmp` to a function that has returned; calling a function through a pointer of the wrong type; `va_arg` with a wrong type; `printf("%d", 3.0)`; infinite loops without side effects (only in C++, not C when the controlling expression is a constant) |
| Library | passing null to `memcpy`/`strlen`/`qsort` (even with length 0), `free` of a non-malloc pointer, `isalpha(c)` with `c` not `unsigned char`/EOF, calling `strtok` reentrantly |
| Preprocessor/linking | two different definitions of the same external symbol (ODR in practice), defining reserved identifiers (`_Foo`, `__x`, `str*`, `is*` names) |

Hardware often "makes UB look defined" (wraparound, reading garbage, segfault), which leads to *latent bugs* that surface after a compiler upgrade, a different optimization level, a new architecture, or an inlining change. The code did not change meaning; the compiler became better at exploiting the assumption.

## Finding UB

1. **Warnings as errors**: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wformat=2 -Wnull-dereference -Wstrict-aliasing=2 -Wuninitialized -Wcast-align -Werror` (selectively).
2. **UndefinedBehaviorSanitizer** (`-fsanitize=undefined`): inserts runtime checks for many categories. One program with six UB variants (`sanitize.c`):

```c
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int which = argc > 1 ? atoi(argv[1]) : 0;
    int big = INT_MAX, zero = 0, sh = 40, arr[4] = {1, 2, 3, 4};
    int *p = NULL;
    switch (which) {
    case 1: printf("%d\n", big + argc);            break;  /* signed overflow */
    case 2: printf("%d\n", 10 / (zero + argc - 2)); break;  /* division by zero */
    case 3: printf("%d\n", 1 << (sh + argc));       break;  /* shift exponent too large */
    case 4: printf("%d\n", arr[argc + 3]);          break;  /* out-of-bounds index */
    case 5: printf("%d\n", *p);                     break;  /* null dereference */
    case 6: { char buf[8]; int *q = (int *)(buf + 1); *q = 1; printf("%d\n", *q); } break;  /* misaligned */
    default: puts("ok");
    }
    return 0;
}
```

```text
$ gcc -g -fsanitize=undefined -fno-sanitize-recover=all sanitize.c -o san_ub && for i in 1 2 3 4 5; do ./san_ub $i 2>&1 | head -1; done
sanitize.c:10:13: runtime error: signed integer overflow: 2147483647 + 2 cannot be represented in type 'int'
sanitize.c:11:31: runtime error: division by zero
sanitize.c:12:30: runtime error: shift exponent 42 is too large for 32-bit type 'int'
sanitize.c:13:31: runtime error: index 5 out of bounds for type 'int [4]'
sanitize.c:14:13: runtime error: load of null pointer of type 'int'
```

   The same program **without** the sanitizer printed `-2147483647` (wrapped), `0`, and `32767` (whatever was on the stack) for the overflow, shift and out-of-bounds cases: no crash, no message, wrong results. `-fsanitize=address` finds out-of-bounds heap/stack/global accesses and use-after-free; `-fsanitize=thread` data races; `-fsanitize=memory` (Clang) uninitialized reads; `-fsanitize=leak` leaks. Run tests and fuzzers under sanitizers continuously (≈2× slower, fine for CI). Valgrind (`memcheck`) catches many memory errors without recompiling.
3. **Static analysis**: `gcc -fanalyzer`, `clang --analyze`/`clang-tidy`, `cppcheck`, CodeQL, Coverity.
4. **Limit what the optimizer may assume** when you must: `-fwrapv` (signed overflow wraps), `-fno-strict-aliasing`, `-fno-delete-null-pointer-checks`, `-fno-strict-overflow` (the Linux kernel uses all of these). This narrows UB-based optimization but does not make the code correct or portable.
5. Prefer constructs with no UB: unsigned arithmetic for bit manipulation, `memcpy` for type punning, `size_t` indexes with explicit bounds checks, `<stdckdint.h>`/builtins for checked math, and languages with memory safety where possible.

## The `volatile` and `restrict` contracts

- **`volatile`**: every access to the object is an observable side effect, which the compiler must perform exactly as written (no merging, no elision, no reordering relative to other volatile accesses). For memory-mapped I/O and signal flags (`volatile sig_atomic_t`). It does **not** give atomicity or inter-thread ordering.
- **`restrict`**: a promise that, for the lifetime of the pointer, the pointed-to object is accessed only through it (or pointers derived from it). It enables vectorization; violating it is UB.
- **`_Noreturn`/`[[noreturn]]`**, `__builtin_unreachable()`: promise paths can't happen; if they do, UB.

## Key takeaways

- Behavior categories: well-defined, implementation-defined (documented), unspecified (don't depend on), undefined (no guarantees at all). The compiler optimizes assuming UB never occurs.
- Signed overflow, null/out-of-bounds/dangling access, uninitialized reads, strict-aliasing violations and data races are UB: the optimizer may delete checks (`x+1 > x` becomes `1`, a null test after dereference vanishes).
- Check before operating; use unsigned or checked arithmetic; document implementation-defined assumptions with `static_assert`.
- Build with strict warnings, run tests under UBSan/ASan/TSan, and use static analysis; flags like `-fwrapv` only limit the damage.
