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
  - label: System V Application Binary Interface, AMD64 Architecture Processor Supplement
    url: https://gitlab.com/x86-psABIs/x86-64-ABI
  - label: Python documentation — ctypes, a foreign function library for Python
    url: https://docs.python.org/3/library/ctypes.html
  - label: Python documentation — Extending and Embedding the Python Interpreter (C API)
    url: https://docs.python.org/3/extending/index.html
  - label: ISO C++ — [dcl.link] language linkage (extern "C")
    url: https://eel.is/c++draft/dcl.link
  - label: The Rustonomicon — Foreign Function Interface and repr(C)
    url: https://doc.rust-lang.org/nomicon/ffi.html
  - label: Ulrich Drepper — How To Write Shared Libraries (visibility, versioning, DSO design)
    url: https://www.akkadia.org/drepper/dsohowto.pdf
  - label: GNU ld — symbol versioning (VERSION scripts)
    url: https://sourceware.org/binutils/docs/ld/VERSION.html
  - label: Itanium C++ ABI — name mangling
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

{{include c/ffi/acc.h}}

and the implementation:

{{include c/ffi/acc.c}}

Notice that `#ifdef __cplusplus extern "C" {` appears in the header: when a C++ client includes it, the declarations get C linkage, so the compiler refers to the plain symbol `acc_add` instead of a mangled name.

## From C++: RAII on top of the C API

A C++ program gets the best of both: it can wrap the opaque handle in a class whose destructor frees it, so the handle is released on every path, even when an exception is thrown, and translate the C error codes into exceptions on the C++ side only.

{{include c/ffi/use.cpp}}

Points to observe: the copy operations are deleted because a copy would free the same handle twice; the callback is a captureless lambda (which converts to a plain function pointer) and the `std::vector` travels through the `void *user` argument; the string allocated by the library goes back through `acc_string_free`.

## From Python: ctypes

`ctypes` (standard library) loads a shared library and calls its functions without any compiled glue code. Its major pitfall is that it **does not read your header**: unless you declare each function's `restype` and `argtypes`, it assumes the function returns a C `int` and guesses argument types from the Python values.

{{include c/ffi/use.py}}

Rules shown there: a `Structure` subclass must list the same fields in the same order and types as the C struct; a returned pointer that must be freed has to come back as `c_void_p` (a `c_char_p` result would be converted to a Python `bytes` copy and the original pointer lost, so it could never be freed); callbacks are created with `CFUNCTYPE` and the Python object must stay referenced while C may call it, otherwise it is garbage collected and C calls freed memory.

## The build and the run

{{include c/ffi/lab.sh}}

{{run c/ffi :: bash lab.sh 2>&1}}

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
