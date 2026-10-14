---
title: "Practice: Bits, I/O and Byte Order"
track: c
module: Systems-Level C
moduleOrder: 7
order: 9
level: advanced
summary: "Five exercises on systems-level C: bit tricks (population count, power-of-two test, safe rotate, single-bit edits, byte swap), a read loop that survives short reads, portable big-endian serialisation, and buffering questions, with verified answers."
prerequisites:
  - "Bit Manipulation"
  - "File I/O in C: stdio vs POSIX, Buffering, Short Reads and Durability"
  - "Portability, Feature-Test Macros and the C ABI"
sources:
  - label: Warren — Hacker's Delight, 2nd ed., Ch. 2 and 5
  - label: "POSIX.1-2017 — read(), write(), pipe(); IEEE Std 1003.1"
    url: https://pubs.opengroup.org/onlinepubs/9699919799/functions/read.html
  - label: "RFC 1700 / RFC 791 Appendix B — network byte order"
    url: https://www.rfc-editor.org/rfc/rfc791
  - label: "man 3 htonl, man 2 read"
    url: https://man7.org/linux/man-pages/man3/htonl.3.html
---

Low-level C is full of small idioms that are easy to get subtly wrong. Write each answer yourself, then compare. Build with `gcc -std=gnu17 -Wall -Wextra -g -fsanitize=address,undefined`.

## Exercises

**1. Bit toolkit.** Implement `popcount32`, `is_pow2`, `rotl32`, and functions to set, clear and toggle bit *i* of a `uint32_t`, plus `byte_swap` that reverses the four bytes. Constraints: `is_pow2(0)` is false; `rotl32` must be well defined for a rotation of 0 (hint: `x >> 32` is undefined for a 32-bit type). Try to write `popcount32` so it loops once per *set* bit.

**2. Short reads.** `read(fd, buf, 64)` on a pipe may return 3. Write `read_all(fd, buf, cap)` that keeps reading until end of file or the buffer is full, retrying on `EINTR`. Demonstrate it with a child process that writes `abc`, pauses, then writes `defgh`.

**3. Portable serialisation.** Write `put_u16`, `put_u32` and `get_u32` that store and load integers in big-endian (network) order using only shifts, so they work on any host. Check them against `ntohl`.

**4. Short answer.** (a) Why does a program that prints with `printf` and then crashes sometimes lose its last lines, while `write(1, ...)` does not? (b) What does `fsync(fd)` guarantee that `fflush(fp)` does not? (c) When is `read()` returning 0 *not* an error?

**5. Find the bug.** `uint32_t rotl(uint32_t x, unsigned r) { return (x << r) | (x >> (32 - r)); }` works for `r = 1..31`. What breaks for `r = 0`, and what does UBSan say?

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

{{include c/practice/p7_bits.c}}

{{run c/practice :: gcc -std=gnu17 -Wall -Wextra -fsanitize=address,undefined -o p7_bits p7_bits.c && ./p7_bits; rm -f p7_bits}}

`x &= x - 1` clears the lowest set bit, so `popcount32` loops once per 1 bit (Kernighan's method; the hardware instruction is available as `__builtin_popcount`). A power of two has exactly one set bit, so `x & (x - 1)` is zero, and the `x != 0` guard handles 0. `rotl32` masks the count to 0..31 and special-cases 0 so no shift by 32 occurs. Single-bit edits use `|`, `& ~` and `^` with `1u << i` (the `u` matters: `1 << 31` on a signed `int` is undefined).

**2.**

{{include c/practice/p7_readall.c}}

{{run c/practice :: gcc -std=gnu17 -Wall -Wextra -fsanitize=address,undefined -o p7_readall p7_readall.c && ./p7_readall; rm -f p7_readall}}

The first `read` asked for 64 bytes and got only the 3 that had arrived: a **short read**, which is normal for pipes, sockets and terminals. `read_all` loops: `n > 0` advances the buffer, `n == 0` means the writer closed its end (end of file), and `n < 0` with `errno == EINTR` means a signal interrupted the call, so retry. Any other negative result is a real error. The same discipline applies to `write`, which may also write less than requested.

**3.**

{{include c/practice/p7_netorder.c}}

{{run c/practice :: gcc -std=gnu17 -Wall -Wextra -fsanitize=address,undefined -o p7_netorder p7_netorder.c && ./p7_netorder; rm -f p7_netorder}}

Shifting out each byte gives the same result on every host because shifts operate on *values*, not on memory layout. `memcpy` into a `uint32_t` (rather than casting the buffer pointer) avoids alignment and aliasing problems, and `ntohl` then converts from the wire order to the host order. Never write a struct to the network with `fwrite`: padding and byte order differ between machines.

**4.** (a) `printf` writes into a **user-space buffer** that is flushed at exit, on newline for a terminal, or when full; a crash skips the flush. `write` goes straight to the kernel. (Use `fflush`, `setvbuf`, or write demos to `stderr`.) (b) `fflush` only moves data from your process's buffer to the kernel; `fsync` asks the kernel to push it to **stable storage**, which is what makes data survive a power failure (directories need their own `fsync` too). (c) At **end of file**: `read` returns 0 when the other end of a pipe or socket is closed or the file offset is at the end. Only `-1` signals an error.

**5.** For `r = 0` the second shift is `x >> 32`, which is undefined for a 32-bit type: the CPU may use only the low 5 bits of the count and return `x` itself, producing `x | x` (accidentally correct on x86), or the compiler may assume it cannot happen. UBSan reports "shift exponent 32 is too large for 32-bit type". The safe form is `r &= 31; return r ? (x << r) | (x >> (32 - r)) : x;`, or `(x << r) | (x >> (-r & 31))`, which compilers recognise as a single rotate instruction.

</details>

## Key takeaways

1. `x & (x - 1)` clears the lowest set bit; use unsigned types and `1u << i`, and never shift by the full width.
2. `read` and `write` may transfer fewer bytes than requested and may be interrupted; loop until done and handle `EINTR`.
3. Serialise integers with shifts into a defined byte order, never by dumping structs or casting pointers.
4. `printf` buffers in user space, `fflush` reaches the kernel, and only `fsync` reaches stable storage.
