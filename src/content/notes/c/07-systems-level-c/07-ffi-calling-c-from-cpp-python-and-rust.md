---
title: "Interoperability: Designing C APIs and Calling Them from C++, Python and Rust"
track: c
module: Systems-Level C
moduleOrder: 7
order: 7
level: advanced
summary: "Why C is the lingua franca of foreign function interfaces and how to design a C library that other languages can use safely: opaque handles, plain structs, ownership, callbacks, error codes, symbol visibility and versioning, demonstrated with a shared library driven from C++ (RAII) and Python (ctypes), including the classic ctypes prototype bugs."
prerequisites:
  - Static and Shared Libraries, Symbols, Dynamic Loading
  - Portability, Feature-Test Macros and the C ABI
  - Stack Frames and the x86-64 System V Calling Convention
sources:
  - label: "System V Application Binary Interface, AMD64 Architecture Processor Supplement"
    url: https://gitlab.com/x86-psABIs/x86-64-ABI
  - label: "Python documentation — ctypes, a foreign function library for Python"
    url: https://docs.python.org/3/library/ctypes.html
  - label: "Python documentation — Extending and Embedding the Python Interpreter (C API)"
    url: https://docs.python.org/3/extending/index.html
  - label: "ISO C++ — [dcl.link] language linkage (extern \"C\")"
    url: https://eel.is/c++draft/dcl.link
  - label: "The Rustonomicon — Foreign Function Interface and repr(C)"
    url: https://doc.rust-lang.org/nomicon/ffi.html
  - label: "Ulrich Drepper — How To Write Shared Libraries (visibility, versioning, DSO design)"
    url: https://www.akkadia.org/drepper/dsohowto.pdf
  - label: "GNU ld — symbol versioning (VERSION scripts)"
    url: https://sourceware.org/binutils/docs/ld/VERSION.html
  - label: "Itanium C++ ABI — name mangling"
    url: https://itanium-cxx-abi.github.io/cxx-abi/abi.html#mangling
---

Almost every language can call C and almost none can call each other directly. Python, Ruby, Java (JNI and the Foreign Function & Memory API), Go (`cgo`), Rust, Swift, C#, Lua and Node.js can all load a shared library and call a function by its name using the platform's C calling convention. The reason is not that C is elegant. It is that the C **ABI** on a given platform is small, stable and fully specified: it fixes how arguments are passed in registers and on the stack (see the stack-frame note), how structs are laid out, and how a symbol is named in the object file. C++ has no such stable ABI across compilers and versions: its names are mangled, its exceptions and templates have no universal representation. So the usual way two languages interoperate is for both to meet at a **C interface**.

## API versus ABI

- The **API** is the source-level contract: names, types, behavior. Changing it breaks compilation.
- The **ABI** is the binary contract: symbol names, calling convention, sizes and offsets of structs, enum values, the meaning of return values. Changing it breaks programs that were compiled earlier and now link against the new library, **without any compile error**.

Examples of silent ABI breaks: adding a field in the middle of a public struct (offsets shift), changing a parameter from `int` to `long`, reordering enum values, changing a struct's alignment, removing a symbol.

## Designing a library that others can bind to

Rules that keep a C API both safe and bindable (the sample library below follows each of them):

1. **Opaque handles**: expose `typedef struct acc acc_t;` in the header and define `struct acc` only in the `.c` file. Callers can only hold pointers, so the layout can change freely. A binding needs just "an opaque pointer".
2. **Plain data structs for results**: fixed-size members, no bit-fields, no unions with tricky layout, no pointers to internal state. Other languages must mirror the struct field by field, so keep it small and use fixed-width types (`uint32_t`, `double`, `size_t`).
3. **Error codes, never exceptions or `longjmp`, across the boundary**: return an `int` status and put results in out-parameters; supply `strerror`-style text. A C++ exception that unwinds through a C frame, or into Python, is undefined behavior or a crash.
4. **Explicit ownership**: say in the header who allocates and who frees. A library should free what it allocated with its own function (`acc_string_free`), because the caller's `free()` might be a different allocator (a different C runtime on Windows, a debug allocator, a language runtime's own heap).
5. **Callbacks carry a user-data pointer** (`void (*cb)(double, void *user)`), otherwise a language cannot attach per-call state to a function pointer. Document threading: on which thread and while holding which lock the callback runs, and whether it may call back into the library.
6. **No global state or hidden thread-affinity**, or document it; support `NULL` for free functions.
7. **Strings**: say the encoding (UTF-8) and the lifetime of returned pointers.
8. **Export only the API**: build with `-fvisibility=hidden` and mark public functions with an attribute (`ACC_API`), so internal helpers do not leak into the symbol table or collide with other libraries.
9. **Version the library**: a `SONAME` (`libacc.so.1`) changes when the ABI breaks; GNU symbol versioning (`VERSION` scripts) lets you keep old and new implementations of one function side by side.

### The sample library

The header:

```c
// A small C library designed to be called from other languages: opaque handle, plain structs, error codes,
// a callback with a user-data pointer, and explicit ownership rules for anything it allocates.
#ifndef ACC_H
#define ACC_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {                                   // C++ callers: do not mangle these names
#endif

#define ACC_API __attribute__((visibility("default")))

typedef struct acc acc_t;                      // opaque: callers never see the layout, so it can change without breaking the ABI

enum { ACC_OK = 0, ACC_ERR_NOMEM = -1, ACC_ERR_EMPTY = -2, ACC_ERR_ARG = -3 };

typedef struct {                               // plain data, fixed layout: safe to mirror in other languages
    double min, max, mean;
    size_t n;
} acc_stats;

ACC_API acc_t *acc_new(void);                                   // caller owns the result: release with acc_free
ACC_API void   acc_free(acc_t *a);                              // accepts NULL
ACC_API int    acc_add(acc_t *a, double x);                     // returns ACC_OK or a negative error
ACC_API int    acc_stats_get(const acc_t *a, acc_stats *out);   // out-parameter filled on success
ACC_API int    acc_foreach(const acc_t *a, void (*cb)(double value, void *user), void *user);   // callback + opaque user pointer
ACC_API char  *acc_describe(const acc_t *a);                    // returns malloc'd text: release with acc_string_free, not free()
ACC_API void   acc_string_free(char *s);
ACC_API const char *acc_strerror(int code);                     // static string, never freed

#ifdef __cplusplus
}
#endif
#endif
```

and the implementation:

```c
#include "acc.h"
#include <stdio.h>
#include <stdlib.h>

struct acc { double *v; size_t n, cap; };

acc_t *acc_new(void) { return calloc(1, sizeof(acc_t)); }
void acc_free(acc_t *a) { if (a) { free(a->v); free(a); } }

int acc_add(acc_t *a, double x) {
    if (!a) return ACC_ERR_ARG;
    if (a->n == a->cap) {
        size_t nc = a->cap ? a->cap * 2 : 8;
        double *p = realloc(a->v, nc * sizeof *p);
        if (!p) return ACC_ERR_NOMEM;
        a->v = p; a->cap = nc;
    }
    a->v[a->n++] = x;
    return ACC_OK;
}

int acc_stats_get(const acc_t *a, acc_stats *out) {
    if (!a || !out) return ACC_ERR_ARG;
    if (a->n == 0) return ACC_ERR_EMPTY;
    double lo = a->v[0], hi = a->v[0], sum = 0;
    for (size_t i = 0; i < a->n; i++) { if (a->v[i] < lo) lo = a->v[i]; if (a->v[i] > hi) hi = a->v[i]; sum += a->v[i]; }
    *out = (acc_stats){ lo, hi, sum / a->n, a->n };
    return ACC_OK;
}

int acc_foreach(const acc_t *a, void (*cb)(double, void *), void *user) {
    if (!a || !cb) return ACC_ERR_ARG;
    for (size_t i = 0; i < a->n; i++) cb(a->v[i], user);
    return ACC_OK;
}

char *acc_describe(const acc_t *a) {
    acc_stats s;
    if (acc_stats_get(a, &s) != ACC_OK) return NULL;
    char *buf = malloc(96);
    if (buf) snprintf(buf, 96, "n=%zu min=%g max=%g mean=%g", s.n, s.min, s.max, s.mean);
    return buf;
}
void acc_string_free(char *s) { free(s); }

const char *acc_strerror(int code) {
    switch (code) {
    case ACC_OK: return "ok"; case ACC_ERR_NOMEM: return "out of memory";
    case ACC_ERR_EMPTY: return "no data"; case ACC_ERR_ARG: return "invalid argument";
    default: return "unknown error";
    }
}
```

Notice that `#ifdef __cplusplus extern "C" {` appears in the header: when a C++ client includes it, the declarations get C linkage, so the compiler refers to the plain symbol `acc_add` instead of a mangled name.

## From C++: RAII on top of the C API

A C++ program gets the best of both: it can wrap the opaque handle in a class whose destructor frees it, so the handle is released on every path, even when an exception is thrown, and translate the C error codes into exceptions on the C++ side only.

```text
// C++ client: RAII wrapper over the C API, errors turned into exceptions on the C++ side only.
#include "acc.h"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

class Accumulator {
    acc_t *h_;
public:
    Accumulator() : h_(acc_new()) { if (!h_) throw std::bad_alloc(); }
    ~Accumulator() { acc_free(h_); }                           // the handle is released on every path, including exceptions
    Accumulator(const Accumulator &) = delete;                 // a copy would double-free
    Accumulator &operator=(const Accumulator &) = delete;
    void add(double x) { int rc = acc_add(h_, x); if (rc != ACC_OK) throw std::runtime_error(acc_strerror(rc)); }
    acc_stats stats() const { acc_stats s; int rc = acc_stats_get(h_, &s); if (rc != ACC_OK) throw std::runtime_error(acc_strerror(rc)); return s; }
    std::vector<double> values() const {
        std::vector<double> out;
        // a captureless lambda converts to a C function pointer; the vector travels through the void* user pointer
        acc_foreach(h_, [](double v, void *u) { static_cast<std::vector<double> *>(u)->push_back(v); }, &out);
        return out;
    }
    std::string describe() const {
        char *s = acc_describe(h_);                            // library-allocated: must go back to acc_string_free
        std::string r = s ? s : "";
        acc_string_free(s);
        return r;
    }
};

int main() {
    Accumulator a;
    for (double x : {3.0, 1.5, 4.5, 9.0}) a.add(x);
    std::printf("C++ : %s\n", a.describe().c_str());
    std::printf("C++ : values copied through the callback:");
    for (double v : a.values()) std::printf(" %g", v);
    std::printf("\n");
    try { Accumulator empty; empty.stats(); } catch (const std::exception &e) { std::printf("C++ : error from the C library became an exception: %s\n", e.what()); }
    return 0;
}
```

Points to observe: the copy operations are deleted because a copy would free the same handle twice; the callback is a captureless lambda (which converts to a plain function pointer) and the `std::vector` travels through the `void *user` argument; the string allocated by the library goes back through `acc_string_free`.

## From Python: ctypes

`ctypes` (standard library) loads a shared library and calls its functions without any compiled glue code. Its major pitfall is that it **does not read your header**: unless you declare each function's `restype` and `argtypes`, it assumes the function returns a C `int` and guesses argument types from the Python values.

```python
"""Python client through ctypes. Run with: python3 use.py [broken]"""
import ctypes, sys

lib = ctypes.CDLL("./libacc.so")

class Stats(ctypes.Structure):                                   # must mirror acc_stats field by field, in order
    _fields_ = [("min", ctypes.c_double), ("max", ctypes.c_double), ("mean", ctypes.c_double), ("n", ctypes.c_size_t)]

if len(sys.argv) > 1 and sys.argv[1] == "broken":
    pass                                                          # forget the prototypes: ctypes assumes every function returns a C int
else:
    lib.acc_new.restype = ctypes.c_void_p                         # the handle is a 64-bit pointer
    lib.acc_free.argtypes = [ctypes.c_void_p]
    lib.acc_add.argtypes = [ctypes.c_void_p, ctypes.c_double]     # without this, a Python float would be passed as an int
    lib.acc_stats_get.argtypes = [ctypes.c_void_p, ctypes.POINTER(Stats)]
    CALLBACK = ctypes.CFUNCTYPE(None, ctypes.c_double, ctypes.c_void_p)
    lib.acc_foreach.argtypes = [ctypes.c_void_p, CALLBACK, ctypes.c_void_p]
    lib.acc_describe.restype = ctypes.c_void_p                    # not c_char_p: that would copy the text and lose the pointer we must free
    lib.acc_describe.argtypes = [ctypes.c_void_p]
    lib.acc_string_free.argtypes = [ctypes.c_void_p]
    lib.acc_strerror.restype = ctypes.c_char_p
    lib.acc_strerror.argtypes = [ctypes.c_int]

h = lib.acc_new()
if len(sys.argv) > 1 and sys.argv[1] == "broken":
    # 1. the default restype is a 32-bit int: a 64-bit pointer is cut to its low half. Compare with the full value from a correct prototype.
    full = ctypes.CFUNCTYPE(ctypes.c_void_p)(("acc_new", lib))()
    print(f"acc_new without restype gave {h & 0xffffffffffffffff:#x}; a correct prototype gives {full:#x} for another object")
    print("  (both are below 4 GiB here because this process's heap happens to live there: the bug is latent, and it appears with PIE/ASLR/mmap'd allocations)")
    # 2. a Python int is passed in an integer register, but acc_add takes a double, which the C ABI expects in a floating-point register
    for x in (3, 1, 4): lib.acc_add(h, x)
    s = Stats(); lib.acc_stats_get(h, ctypes.byref(s))
    print(f"added the integers 3, 1, 4 without argtypes -> the library stored n={s.n} values with min={s.min}, max={s.max} (expected 1 and 4)")
    sys.exit(0)

for x in (3.0, 1.5, 4.5, 9.0): assert lib.acc_add(h, x) == 0
s = Stats(); rc = lib.acc_stats_get(h, ctypes.byref(s))
print(f"Python: rc={rc} n={s.n} min={s.min} max={s.max} mean={s.mean}")

seen = []
cb = CALLBACK(lambda v, user: seen.append(v))                      # keep a reference to cb alive for the duration of the call
lib.acc_foreach(h, cb, None)
print("Python: values delivered to a Python callback:", seen)

p = lib.acc_describe(h); print("Python:", ctypes.string_at(p).decode()); lib.acc_string_free(p)
empty = lib.acc_new(); rc = lib.acc_stats_get(empty, ctypes.byref(s))
print("Python: empty accumulator ->", rc, lib.acc_strerror(rc).decode())
lib.acc_free(empty); lib.acc_free(h)
```

Rules shown there: a `Structure` subclass must list the same fields in the same order and types as the C struct; a returned pointer that must be freed has to come back as `c_void_p` (a `c_char_p` result would be converted to a Python `bytes` copy and the original pointer lost, so it could never be freed); callbacks are created with `CFUNCTYPE` and the Python object must stay referenced while C may call it, otherwise it is garbage collected and C calls freed memory.

## The build and the run

```bash
#!/bin/bash
set -e
gcc -O2 -Wall -Wextra -fPIC -shared -fvisibility=hidden acc.c -o libacc.so
echo '$ nm -D --defined-only libacc.so | grep acc_      (the exported C symbols: plain names, nothing mangled)'
nm -D --defined-only libacc.so | grep " T " | awk '{print "  " $3}'
echo
g++ -std=c++17 -O2 -Wall -Wextra use.cpp -L. -lacc -Wl,-rpath,'$ORIGIN' -o use_cpp
./use_cpp
echo
python3 use.py
echo
echo "--- the same calls with no ctypes prototypes:"
python3 use.py broken
echo
echo '--- name mangling: what the same function would be called if C++ had compiled it without extern "C"'
cat > /tmp/m.cpp <<'CPP'
int acc_add(void *a, double x);
namespace ns { int acc_add(void *a, double x) { return 0; } }
int acc_add(void *a, double x) { return 1; }
CPP
g++ -c /tmp/m.cpp -o /tmp/m.o && nm /tmp/m.o | grep " T " | awk '{print "  " $3}'; rm -f /tmp/m.cpp /tmp/m.o
```

```text
$ bash lab.sh 2>&1
$ nm -D --defined-only libacc.so | grep acc_      (the exported C symbols: plain names, nothing mangled)
  acc_add
  acc_describe
  acc_foreach
  acc_free
  acc_new
  acc_stats_get
  acc_strerror
  acc_string_free

C++ : n=4 min=1.5 max=9 mean=4.5
C++ : values copied through the callback: 3 1.5 4.5 9
C++ : error from the C library became an exception: no data

Python: rc=0 n=4 min=1.5 max=9.0 mean=4.5
Python: values delivered to a Python callback: [3.0, 1.5, 4.5, 9.0]
Python: n=4 min=1.5 max=9 mean=4.5
Python: empty accumulator -> -2 no data

--- the same calls with no ctypes prototypes:
acc_new without restype gave 0x20a1d590; a correct prototype gives 0x209cbd70 for another object
  (both are below 4 GiB here because this process's heap happens to live there: the bug is latent, and it appears with PIE/ASLR/mmap'd allocations)
added the integers 3, 1, 4 without argtypes -> the library stored n=3 values with min=0.0, max=0.0 (expected 1 and 4)

--- name mangling: what the same function would be called if C++ had compiled it without extern "C"
  _Z7acc_addPvd
  _ZN2ns7acc_addEPvd
```

What the run shows:

- The shared library exports exactly the eight API functions as unmangled symbols because the build used `-fvisibility=hidden` and only `ACC_API` functions are public.
- The C++ and Python clients both drive the same library: statistics, a callback delivering all values, library-allocated text that is released with the library's free function, and the "no data" error code translated to an exception in C++ and printed as text in Python.
- **Missing prototypes in ctypes** produce silent wrong behavior, the most dangerous kind. Two bugs are shown in the final scenario. First, the pointer returned by `acc_new` is read as a 32-bit `int`; in this run the heap happens to be below 4 GiB, so the value even looks right, but with ASLR, a position-independent executable or an `mmap`-based allocation, the upper half of the pointer is lost and the next call crashes. Second, calling `acc_add(h, 3)` passes the Python integer `3` in an **integer** register, whereas the function takes a `double`, which the System V ABI passes in an **XMM** register. The function reads whatever is in `xmm0` (zero here), so the library stores zeros: three integers added, `min` and `max` both 0.0, and no error anywhere. With `argtypes` declared, ctypes converts correctly.
- The last block shows **name mangling**: the same function declared in C++ without `extern "C"` becomes `_Z7acc_addPvd` (the encoded parameter types) and a namespaced overload becomes `_ZN2ns7acc_addEPvd`. The mangling scheme is part of the C++ ABI and differs between compilers, which is why C++ libraries expose a C interface when they want to be used from other languages.

## Other languages (descriptions; not run here)

The tools available when this note was written did not include a Rust or Go toolchain, so the following is from the documentation of each language and has not been compiled in this series:

- **Rust**: declare the foreign functions in an `extern "C"` block (all calls are `unsafe`), mirror structs with `#[repr(C)]`, use `std::ffi::{c_int, CStr, CString}` for strings. `bindgen` generates Rust declarations from a C header; `cbindgen` generates a C header from Rust code exposed with `#[no_mangle] pub extern "C" fn`. A panic must not unwind out of an `extern "C"` function (it aborts by default). Wrap the raw handle in a struct that implements `Drop` to call the free function, the Rust counterpart of the C++ RAII wrapper above.
- **Go**: `cgo` imports C code with `import "C"`; each Go-to-C call has a fixed overhead of the order of tens of nanoseconds to a microsecond because it switches stacks, so batch work per call. Pointers passed to C must not hold Go pointers beyond the call (the cgo pointer-passing rules).
- **Java**: JNI requires glue code; the Foreign Function & Memory API (final in Java 22) loads and calls C functions directly, with `MemorySegment` for structs.
- **Node.js**: N-API addons compiled against a stable ABI; FFI libraries (`koffi`, `ffi-napi`) do the ctypes job.
- **Python at scale**: `cffi` (reads declarations, ABI or API mode), `pybind11` and `nanobind` (C++), Cython, or writing a CPython extension module using the C API give better performance and type safety than raw `ctypes`; the GIL must be released around long C calls (`Py_BEGIN_ALLOW_THREADS`, which `ctypes` does automatically for `CDLL` calls) and re-acquired in callbacks.

## Type mapping cheat sheet

| C | Python ctypes | Rust | Notes |
|---|---|---|---|
| `int`, `unsigned` | `c_int`, `c_uint` | `c_int`, `c_uint` | 32-bit on LP64 Linux; use `int32_t` in APIs to be explicit |
| `long` | `c_long` | `c_long` | 64-bit on Linux, 32-bit on 64-bit Windows: avoid in public APIs |
| `size_t` | `c_size_t` | `usize` / `size_t` | |
| `double` / `float` | `c_double` / `c_float` | `f64` / `f32` | passed in vector registers |
| `char *` (text) | `c_char_p` (read-only) | `*const c_char` / `CStr` | encode text as UTF-8 and document it |
| `void *` | `c_void_p` | `*mut c_void` | |
| `T *` out-param | `byref(x)` / `POINTER(T)` | `&mut T` | |
| struct | `ctypes.Structure` | `#[repr(C)] struct` | same field order and types; check `sizeof` on both sides |
| function pointer | `CFUNCTYPE(...)` | `extern "C" fn(...)` | needs a user-data pointer to be useful |
| `bool` | `c_bool` | `bool` | `_Bool` is one byte; avoid in structs shared across compilers |

When in doubt, print `sizeof(struct)` and `offsetof` of every field in C and compare with the other side, or use `_Static_assert` in the C header to freeze the layout.

## Key takeaways

- Languages interoperate through the C ABI because it is simple, stable and specified; C++ names are mangled and its ABI differs between compilers, so expose `extern "C"` functions.
- Design the C API for binding: opaque handles, plain structs, error codes, explicit ownership with a matching free function, callbacks with a user-data pointer, and hidden symbols except the public API.
- ABI breaks (struct layout, enum values, argument types) do not produce compile errors; version the library with a SONAME and use symbol versioning when you must keep old behavior.
- In ctypes always declare `restype` and `argtypes`: without them pointers are truncated and floating-point arguments travel in the wrong registers, giving silent corruption.
- Wrap handles in RAII (C++) or `Drop` (Rust) or a context manager (Python) so they are freed on all paths, and never let an exception or panic unwind through a C frame.
