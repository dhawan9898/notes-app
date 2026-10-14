# C programming track — topic map

Status: [x] written, [ ] planned. Notes are assembled from `authoring/templates/` with the tested sources in `authoring/examples/` (`python3 authoring/build.py`).

## 1. Toolchain and Language Foundations
- [x] The compilation pipeline and the ELF object model
- [x] C standards, undefined and implementation-defined behavior
- [x] Types, integer promotion, conversions and overflow

## 2. Memory and Pointers
- [x] Pointers, arrays and decay
- [x] Stack frames and the x86-64 calling convention
- [x] Dynamic memory: malloc internals, bugs and allocators
- [x] Strings and buffer safety

## 3. Data Modeling
- [x] Structs, alignment, padding, unions, bit-fields and endianness
- [x] Qualifiers, storage classes, linkage and scope

## 4. Advanced Language
- [x] The preprocessor and macros
- [x] Function pointers, callbacks, opaque types and intrusive containers
- [x] Error handling, variadics and setjmp/longjmp
- [x] Modern C: C11, C17 and C23 features

## 5. Linking and Building
- [x] Static and shared libraries, symbols and dynamic loading
- [x] Build systems, compiler flags and hardening options

## 6. Debugging and Quality
- [x] Debugging with gdb and sanitizers
- [x] Static analysis, fuzzing and testing
- [x] Secure C coding

## 7. Systems-Level C
- [x] Bit manipulation
- [x] File I/O: stdio vs POSIX
- [x] Sockets programming
- [x] Performance-oriented C
- [x] Portability and ABI

## C Essentials (moduleOrder 0.5, beginner)

Added after the continuity review (beginner bridge between *Start Here* and module 1).

- [x] Structs, Enums and Typedef
- [x] Functions, Headers and Multi-File Programs
- [x] More Control Flow: switch, Loop Control and Recursion
- [x] Arrays, Strings, Command-Line Arguments and Files
- [x] From Source to Running Process: A Bridge to Systems C
