---
title: Portability, Feature-Test Macros and the C ABI
track: c
module: Systems-Level C
moduleOrder: 7
order: 5
level: advanced
summary: Writing C that survives different compilers, operating systems and architectures — data models, feature-test macros, endianness and alignment, and how the C ABI lets C++, Python and Rust call your library, with verified examples.
prerequisites:
  - Stack Frames and the x86-64 System V Calling Convention
  - Structs, Alignment, Padding, Unions, Bit-fields and Endianness
  - Static and Shared Libraries, Symbols, Dynamic Loading
sources:
  - label: "ISO/IEC 9899:2018 §4 Conformance; §5.2.4.2 Numerical limits; Annex J.3 Implementation-defined behavior"
  - label: "man 7 feature_test_macros; POSIX.1-2017 Rationale (Portability)"
    url: https://man7.org/linux/man-pages/man7/feature_test_macros.7.html
  - label: "System V ABI supplements (x86-64, AArch64 AAPCS64); Itanium C++ ABI for name mangling"
    url: https://itanium-cxx-abi.github.io/cxx-abi/abi.html
  - label: "Python ctypes and CFFI documentation; Rust FFI (Nomicon) and cbindgen/bindgen"
    url: https://docs.python.org/3/library/ctypes.html
  - label: "Drepper — ABI considerations for library authors; Sourceware \"Binary compatibility\" notes"
    url: https://www.akkadia.org/drepper/dsohowto.pdf
  - label: "Year 2038 problem — glibc _TIME_BITS=64 documentation"
    url: https://www.gnu.org/software/libc/manual/html_node/Feature-Test-Macros.html
---

"It works on my machine" in C usually means "it works with my compiler, libc, word size, byte order and optimization level". Portability is the discipline of knowing which of those your code depends on, writing to the standard where you can, and isolating the rest. A separate but related concern is **ABI compatibility**: whether independently compiled binaries (yours and your users', in different languages) can call each other.

## Layers of portability

| Layer | Risks | Strategy |
|---|---|---|
| **Language** | undefined / unspecified / implementation-defined behavior; compiler extensions | stay within ISO C (`-std=c11 -pedantic-errors`), isolate `__attribute__`/`__builtin_*` behind macros, test with several compilers (GCC, Clang, MSVC) |
| **Library** | POSIX vs Windows vs musl vs glibc differences; missing functions (`strlcpy`, `getline`) | feature-test macros, `configure`-style checks, compatibility shims |
| **OS** | process/file/socket APIs (`fork` vs `CreateProcess`), path separators, line endings, file name case/encoding, signals | a thin platform abstraction layer (`os_*.c`) |
| **Hardware** | word size, endianness, alignment, atomics, SIMD, page size, cache line | fixed-width types, explicit byte order, `memcpy`, `static_assert`, runtime dispatch |
| **Toolchain** | flags, linkers, shared-library conventions | CMake/Meson with checks; CI matrix |

Decide your **supported platform set** and test it in CI (Linux gcc/clang, macOS, Windows, 32- and 64-bit, big-endian via `qemu-user`/`s390x`, musl). Untested portability doesn't exist.

## Data models and fundamental types

C only guarantees minimum ranges. Common data models:

| Model | `int` | `long` | `long long` | pointer | Used by |
|---|---|---|---|---|---|
| ILP32 | 32 | 32 | 64 | 32 | 32-bit Linux/Windows/macOS |
| **LP64** | 32 | **64** | 64 | 64 | 64-bit Linux, macOS, BSD |
| **LLP64** | 32 | **32** | 64 | 64 | 64-bit Windows |

```c
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <wchar.h>

int main(void) {
    printf("%-12s %s\n", "type", "bytes on this platform  (ILP32 / LP64 Linux / LLP64 Windows)");
    printf("%-12s %zu   (4 / 4 / 4)\n", "int", sizeof(int));
    printf("%-12s %zu   (4 / 8 / 4)   <- differs\n", "long", sizeof(long));
    printf("%-12s %zu   (8 / 8 / 8)\n", "long long", sizeof(long long));
    printf("%-12s %zu   (4 / 8 / 8)   <- differs\n", "void *", sizeof(void *));
    printf("%-12s %zu   (4 / 8 / 8)   <- differs\n", "size_t", sizeof(size_t));
    printf("%-12s %zu   (4 / 4 / 2)   <- differs\n", "wchar_t", sizeof(wchar_t));
    printf("%-12s %zu   (4 / 8 / 8)\n", "time_t", sizeof(time_t));
    printf("char signed? %s    byte order: %s-endian\n", CHAR_MIN < 0 ? "yes" : "no", *(const uint16_t *)"\x01\x00" == 1 ? "little" : "big");
    return 0;
}
```

```text
$ gcc -std=gnu11 -Wall -Wextra models.c -o models && ./models
type         bytes on this platform  (ILP32 / LP64 Linux / LLP64 Windows)
int          4   (4 / 4 / 4)
long         8   (4 / 8 / 4)   <- differs
long long    8   (8 / 8 / 8)
void *       8   (4 / 8 / 8)   <- differs
size_t       8   (4 / 8 / 8)   <- differs
wchar_t      4   (4 / 4 / 2)   <- differs
time_t       8   (4 / 8 / 8)
char signed? yes    byte order: little-endian
```

Consequences:

- Don't store pointers in `int` or `long` (breaks on LLP64); use `uintptr_t`. Don't assume `sizeof(long) == sizeof(void *)`.
- Use fixed-width types (`int32_t`, `uint64_t`) for data with a defined size; `size_t` for sizes; `ptrdiff_t`, `intptr_t`; `PRIu64`/`%zu` format macros (`%ld` is wrong for `int64_t` on LLP64).
- `time_t` is 32-bit on legacy 32-bit systems (the **year 2038** problem); on glibc ≥ 2.34 compile 32-bit targets with `-D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64`.
- `char` signedness varies (x86 signed, ARM/PowerPC unsigned by default): use `signed char`/`unsigned char` when it matters; `wchar_t` is 2 bytes on Windows, 4 on Linux; `long double` is 80-bit x87 on x86, 128-bit IEEE on AArch64, equal to `double` on MSVC.
- Enum size, bit-field layout, and struct padding differ across ABIs (see the structs note).

Make assumptions explicit: `static_assert(sizeof(int) == 4 && CHAR_BIT == 8, "unsupported platform");`.

## Endianness and alignment

- Write byte-order-explicit serialization (shifts or `htonl`/`be32toh`), never raw struct dumps (structs note).
- Access unaligned data via `memcpy` (portable and often free) rather than casts; some architectures trap on misaligned loads, and compilers may assume alignment and emit instructions (SIMD, atomics) that fault.
- Test big-endian and 32-bit configurations with emulation: `qemu-s390x-static`, `qemu-mips`, or cross-compilers in CI; many bugs hide on little-endian x86-64 only.

## Feature-test macros: which declarations are visible

Headers expose only the functions allowed by the standard and macros you request. Under strict ISO C11, POSIX functions such as `strdup`, `fileno` or `getline` are not declared (and modern GCC treats implicit function declarations as errors):

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char *s = strdup("duplicate");                  /* POSIX, not ISO C11 */
    char *line = NULL; size_t cap = 0;
    printf("%s %d\n", s, fileno(stdin) >= 0);       /* fileno: POSIX */
    free(s); free(line);
    (void)cap;
    return 0;
}
```

```text
$ echo '--- strict ISO C11:'; gcc -std=c11 -pedantic-errors -Wall ftm.c -o ftm_strict 2>&1 | grep -E 'error' | cut -c1-110 | head -n 3; echo '--- with a feature-test macro:'; gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra ftm.c -o ftm_ok && ./ftm_ok; echo '--- default dialect (gnu17):'; gcc -Wall ftm.c -o ftm_gnu && ./ftm_gnu; rm -f ftm_strict ftm_ok ftm_gnu
--- strict ISO C11:
ftm.c:6:15: error: implicit declaration of function ‘strdup’; did you mean ‘strcmp’? [-Wimplicit-funct
ftm.c:6:15: error: initialization of ‘char *’ from ‘int’ makes pointer from integer without a cast [-W
ftm.c:8:26: error: implicit declaration of function ‘fileno’ [-Wimplicit-function-declaration]
--- with a feature-test macro:
duplicate 1
--- default dialect (gnu17):
duplicate 1
```

| Macro | Exposes |
|---|---|
| `_POSIX_C_SOURCE=200809L` | POSIX.1-2008 |
| `_XOPEN_SOURCE=700` | X/Open (SUSv4) additions |
| `_DEFAULT_SOURCE` | glibc defaults (BSD + POSIX), implied by `-std=gnu*` |
| `_GNU_SOURCE` | everything GNU: `asprintf`, `qsort_r`, `pipe2`, `accept4`, `memmem`, `strchrnul`, `CPU_SET`, `RTLD_NEXT` |
| `_FILE_OFFSET_BITS=64` | 64-bit `off_t` on 32-bit systems |
| `_REENTRANT`, `_LARGEFILE64_SOURCE`, `__STDC_WANT_LIB_EXT1__` | misc. |

Define them **before** any `#include` (or on the command line with `-D`). Prefer the narrowest macro that provides what you need; use `_GNU_SOURCE` only in files that need GNU extensions, or isolate them in a platform layer. On macOS/BSD, a function may exist under different headers or names (`strlcpy`, `arc4random_buf` exist there but not in old glibc; `getentropy` varies); on Windows POSIX APIs are partially available through the UCRT/MinGW or require replacements.

### Detecting capabilities

```c
#if defined(__linux__)
#  include <sys/epoll.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#  include <sys/event.h>
#endif
#if defined(__has_include)
#  if __has_include(<threads.h>)
#    define HAVE_C11_THREADS 1
#  endif
#endif
#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#  include <stdatomic.h>
#endif
```

Prefer build-time probing (CMake `check_symbol_exists`, `check_c_source_compiles`; Meson `cc.has_function`; Autoconf `AC_CHECK_FUNCS`) generating a `config.h`, over guessing from platform macros: macros tell you *where* you are, probes tell you *what is available*. Provide fallbacks (`#ifndef HAVE_STRLCPY` → own implementation in `compat.c`).

## Compiler extensions

GNU extensions (`__attribute__`, statement expressions, `typeof`, computed goto, `__builtin_expect`, variable-length arrays in structs, zero-length arrays) are convenient but ties code to GCC/Clang. Wrap them:

```c
#if defined(__GNUC__)
#  define LIKELY(x)   __builtin_expect(!!(x), 1)
#  define NORETURN    __attribute__((noreturn))
#  define PRINTF_FMT(a, b) __attribute__((format(printf, a, b)))
#else
#  define LIKELY(x)   (x)
#  define NORETURN
#  define PRINTF_FMT(a, b)
#endif
```

and use standard spellings where C11/C23 has them (`_Noreturn`/`[[noreturn]]`, `_Alignas`, `static_assert`, `[[nodiscard]]`). Compile with `-std=c11 -pedantic-errors` in CI to catch accidental dependence on extensions.

## The C ABI as the universal interface

An **ABI** (application binary interface) defines how compiled code interoperates: sizes and layout of types, calling convention (which registers, stack), symbol names, and object formats. Source portability (API) is distinct from binary compatibility (ABI). C has no standardized ABI in the language, but each platform defines one (System V AMD64, AAPCS64, Windows x64), and because it is simple, **C's ABI is the lingua franca for cross-language calls**: Python (`ctypes`, CFFI), Rust (`extern "C"`), Go (cgo), Java (JNI/Panama), C#, Swift, Lua, Node.js (N-API) and C++ all call or expose C-ABI functions.

### C++ and name mangling

C++ encodes overloads/namespaces into symbol names ("mangling") and uses its own conventions for classes and exceptions. To call a C library from C++ (or export C++ code with a C interface), declare it `extern "C"` so symbols keep their plain C names, usually inside `#ifdef __cplusplus` guards in the header:

```c
#ifndef CABI_H
#define CABI_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {                                   /* disable C++ name mangling for these declarations */
#endif

typedef struct { int32_t x; int32_t y; double weight; } cpoint;      /* plain data: layout is predictable */

int      cabi_add(int a, int b);
double   cabi_dot(const double *a, const double *b, size_t n);
void     cabi_fill(cpoint *out, size_t n);                           /* caller allocates, library fills */
const char *cabi_version(void);

#ifdef __cplusplus
}
#endif
#endif
```

```c
#include "cabi.h"
int cabi_add(int a, int b) { return a + b; }
double cabi_dot(const double *a, const double *b, size_t n) { double s = 0; for (size_t i = 0; i < n; i++) s += a[i] * b[i]; return s; }
void cabi_fill(cpoint *out, size_t n) { for (size_t i = 0; i < n; i++) out[i] = (cpoint){ (int32_t)i, (int32_t)(i * i), 0.5 * (double)i }; }
const char *cabi_version(void) { return "cabi 1.0"; }
```

```text
#include <cstdio>
#include "cabi.h"
int main() { std::printf("C++ calls C: %d, %s\n", cabi_add(40, 2), cabi_version()); }
```

```text
$ gcc -shared -fPIC -O2 cabi.c -o libcabi.so && g++ -O1 cpp_user.cpp -L. -lcabi -Wl,-rpath,'$ORIGIN' -o cpp_user && ./cpp_user; echo '--- C++ symbol for a declaration WITHOUT extern "C":'; printf 'int cabi_add(int a, int b);\nint f() { return cabi_add(1,2); }\n' > /tmp/nomangle.cpp && g++ -c /tmp/nomangle.cpp -o /tmp/nomangle.o && nm /tmp/nomangle.o | grep -i cabi; echo '--- WITH extern "C" (from the header):'; g++ -c cpp_user.cpp -o cpp_user.o && nm cpp_user.o | grep -i cabi; rm -f cpp_user.o
C++ calls C: 42, cabi 1.0
--- C++ symbol for a declaration WITHOUT extern "C":
                 U _Z8cabi_addii
--- WITH extern "C" (from the header):
                 U cabi_add
                 U cabi_version
```

Without `extern "C"` the C++ compiler looks for `_Z8cabi_addii` (mangled), which the C library doesn't define → `undefined reference`. With it, the plain `cabi_add` symbol resolves.

### Calling C from Python

`ctypes` loads the shared library and calls functions by name; you declare prototypes and mirror struct layouts exactly:

```python
import ctypes, os
lib = ctypes.CDLL(os.path.join(os.path.dirname(os.path.abspath(__file__)), "libcabi.so"))

class CPoint(ctypes.Structure):                     # must mirror the C layout EXACTLY (types, order, padding)
    _fields_ = [("x", ctypes.c_int32), ("y", ctypes.c_int32), ("weight", ctypes.c_double)]

lib.cabi_add.argtypes = [ctypes.c_int, ctypes.c_int]; lib.cabi_add.restype = ctypes.c_int
lib.cabi_dot.argtypes = [ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double), ctypes.c_size_t]; lib.cabi_dot.restype = ctypes.c_double
lib.cabi_fill.argtypes = [ctypes.POINTER(CPoint), ctypes.c_size_t]; lib.cabi_fill.restype = None
lib.cabi_version.restype = ctypes.c_char_p

print("Python calls C:", lib.cabi_add(40, 2), lib.cabi_version().decode())
a = (ctypes.c_double * 3)(1, 2, 3); b = (ctypes.c_double * 3)(4, 5, 6)
print("dot =", lib.cabi_dot(a, b, 3))
pts = (CPoint * 4)(); lib.cabi_fill(pts, 4)
print("points:", [(p.x, p.y, p.weight) for p in pts], " sizeof(CPoint) =", ctypes.sizeof(CPoint))
```

```text
$ python3 pyuser.py
Python calls C: 42 cabi 1.0
dot = 32.0
points: [(0, 0, 0.0), (1, 1, 0.5), (2, 4, 1.0), (3, 9, 1.5)]  sizeof(CPoint) = 16
```

The `CPoint` class must match the C struct in field types, order and padding (`sizeof` = 16 on both sides). A mismatch doesn't raise an error: it reads garbage or corrupts memory.

### Designing a stable C API/ABI

1. **Plain data and opaque handles**: expose `typedef struct Foo Foo;` with `foo_new/foo_free`, not struct layouts you may change. If you must expose a struct, add a `size` or `version` field as its first member, reserve padding, and only append fields.
2. **Fixed-width types** in the interface (`int32_t`, `uint64_t`, `size_t`), not `long` or `enum` of unspecified size (use an `int` with named constants).
3. **No C++ types** (`std::string`, exceptions) or compiler-specific features across the boundary. Don't throw exceptions out of `extern "C"` functions or `longjmp` across language runtimes.
4. **Ownership**: whoever allocates frees; export a `foo_free()` (the library may use a different allocator/CRT than the caller, especially on Windows with different C runtimes).
5. **Errors via return codes** plus an out-parameter or `foo_last_error()`; callbacks with `void *user_data`.
6. **Export control**: `-fvisibility=hidden` with explicit `API` macros (`__declspec(dllexport)` on Windows), symbol version scripts, SONAME bumps on incompatible change.
7. **Don't change semantics silently**: removing or reordering parameters, changing struct layout, enum values or function behavior breaks binaries. Tools: `abidiff`/`libabigail`, `abi-compliance-checker`, `pkg-config`, semantic versioning.
8. **Thread safety and reentrancy** documented; global state avoided or explicit (`foo_init()`).
9. **Language bindings**: generate them (`bindgen`/`cbindgen` for Rust, SWIG, CFFI `cdef`, `cgo`), and keep a test that loads the shared library from the target language.

### Windows notes

DLL exports need `__declspec(dllexport/dllimport)`; calling conventions (`__stdcall` in 32-bit) differ; each module may use a different CRT, so don't free across DLL boundaries; text vs binary file modes; path separators and UTF-16 APIs (`_wfopen`); `long` is 32-bit; `size_t` formatting (`%zu` works on modern UCRT). Prefer portable wrappers or libraries (libuv, SDL, APR) for OS abstraction.

## A portability checklist

1. Build with `-std=c11 -pedantic-errors -Wall -Wextra` on at least two compilers; run the tests on 32-bit and big-endian (QEMU) occasionally.
2. No assumptions about `sizeof(long)`, `char` signedness, struct layout, endianness, alignment, `time_t` width; `static_assert` the ones you keep.
3. Feature-test macros defined explicitly; platform code isolated and covered by CI.
4. Format specifiers via `%zu`, `PRI*`; no `int` for sizes/pointers.
5. Avoid reliance on evaluation order, UB, uninitialized memory, and locale-dependent behavior (`setlocale`, `strtod` decimal separators).
6. Filesystem: `/` separator tolerance, case sensitivity, `PATH_MAX` limits, UTF-8 vs UTF-16 names, line endings.
7. Document the ABI contract for shared libraries; test upgrades against binaries built with the previous version.

## Key takeaways

- Know the data model (LP64 vs LLP64): `long` and `wchar_t` differ; use `<stdint.h>`, `size_t`, `uintptr_t`, `PRI*` macros and `static_assert` assumptions.
- Feature-test macros control which POSIX/GNU declarations are visible (`strdup`, `getline` vanish under strict C11); define them first, narrowly, and probe capabilities at build time.
- Isolate compiler and OS specifics behind small wrappers; avoid raw struct I/O, unaligned casts and endianness assumptions.
- The C ABI is the interoperability layer: use `extern "C"`, plain data, opaque handles, fixed-width types and explicit ownership, and keep exports and versions under control. Verified above: the same `libcabi.so` was called from C++ and Python.
