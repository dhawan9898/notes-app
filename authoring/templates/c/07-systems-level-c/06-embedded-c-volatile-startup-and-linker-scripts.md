---
title: "Embedded C: volatile, Memory-Mapped I/O, Startup Code and Linker Scripts"
track: c
module: Systems-Level C
moduleOrder: 7
order: 6
level: advanced
summary: "What bare-metal C needs beyond hosted C: volatile and register access shown in compiler output, a freestanding program with its own entry point and linker script, what startup code does with .data and .bss, interrupt-safe ring buffers, and shrinking images with section garbage collection."
prerequisites:
  - "The Compilation Pipeline and the ELF Object Model"
  - "Qualifiers, Storage Classes, Scope and Linkage"
  - "Threads, Atomics and the C11 Memory Model"
sources:
  - label: ISO/IEC 9899:2011 §6.7.3 and §5.1.2.3 — volatile, observable behavior; §5.1.2.1 freestanding environment
    url: https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
  - label: GNU ld manual — Linker scripts (SECTIONS, AT, PROVIDE, MEMORY)
    url: https://sourceware.org/binutils/docs/ld/Scripts.html
  - label: Arm Architecture Reference Manual for Armv7-M — exception model and vector table (B1.5)
    url: https://developer.arm.com/documentation/ddi0403/latest/
  - label: Arm CMSIS-Core — device startup and register access conventions
    url: https://arm-software.github.io/CMSIS_6/latest/Core/index.html
  - label: GCC manual — Optimize Options (-ffunction-sections, -fdata-sections), Link Options (--gc-sections)
    url: https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
  - label: MISRA C:2012 — Guidelines for the use of the C language in critical systems
    url: https://misra.org.uk/
---

"Embedded C" is ordinary C compiled for a machine with no operating system, a few kilobytes to megabytes of memory, hardware controlled by **registers at fixed addresses**, and events delivered by **interrupts**. The language is the same; the assumptions change. In a *hosted* environment the OS loads your program and the C runtime runs `main`. In a *freestanding* environment (C11 §5.1.2.1) the entry point is implementation-defined and only a few headers (`<stdint.h>`, `<stddef.h>`, `<stdbool.h>`, `<limits.h>`, `<float.h>`, `<stdatomic.h>`, ...) are guaranteed; you supply everything else.

This note shows each mechanism with something you can run on an ordinary Linux PC. No board was used for these examples: the compiler output, ELF structure and linker behavior are the same ones used for microcontrollers; where hardware behavior is described it is from the cited architecture manuals.

## volatile and memory-mapped I/O

A peripheral register is just an address. A UART might have a status register at `0x4000C018` whose bit 0 means "ready", and a data register at `0x4000C000`. The usual idiom:

```c
#define UART_DR  (*(volatile uint32_t *)0x4000C000u)
#define UART_SR  (*(volatile uint32_t *)0x4000C018u)
```

`volatile` tells the compiler that accessing the object has **side effects it cannot see**: every access written in the source must happen, exactly as many times, in the order written, relative to other volatile accesses (C11 §5.1.2.3). The compiler normally assumes memory only changes when your code changes it, which breaks for hardware that changes a status bit on its own. The difference is visible in generated code:

{{include c/embedded/poll.c}}

{{run c/embedded :: gcc -O2 -S -o - poll.c | grep -v -E '^\s*\.(cfi|file|text|globl|type|size|section|ident|p2align)|endbr64'}}

`wait_plain` loads the status **once**, and if the bit was clear it jumps to itself forever (`.L3: jmp .L3`): the compiler reasoned that nothing in the loop changes `*status`. `wait_volatile` reloads on every iteration, and reads again for the return value as the source says. The stores in `write_pair` stay in program order, which matters when the device treats "write index then write data" as a command.

What `volatile` does **not** do:

- It gives no atomicity: a 32-bit access on a 16-bit bus can tear, and a read-modify-write (`REG |= bit`) is three steps that an interrupt can split.
- It orders only against other volatile accesses *as the compiler sees them*; it does not emit CPU memory barriers. On cores with write buffers or caches (and on ARM Cortex-M7, Cortex-A), device drivers add explicit barriers (`DMB`/`DSB`) between a register write and a dependent action such as enabling an interrupt or starting DMA.
- It is **not** a thread synchronization tool; use atomics for data shared with interrupts or other cores (previous note).

Practical register habits:

- Describe a register block as a `volatile struct` overlaid on its base address, and check offsets with `_Static_assert(offsetof(...) == 0x18)`.
- Use named masks and shifts, not magic numbers. Prefer masks to bit-fields for hardware registers: the layout of C bit-fields (bit order, straddling) is implementation-defined (see the structs note).
- Some registers have **read side effects** (reading clears a flag) or **write-one-to-clear** semantics: `SR = SR & ~FLAG` is wrong, `SR = FLAG` is right. Read the reference manual's access type for each field.
- Read a register once into a local, test the local, rather than reading it repeatedly.
- Use `uint8_t`, `uint16_t`, `uint32_t` for register widths, never `int` or `unsigned`.

## What happens before main

On a hosted system the kernel maps your ELF segments, builds a stack, and the C library's `_start` calls `main`. On a microcontroller, hardware reset does one thing: it loads the stack pointer and program counter from the **vector table** at a fixed address (on Arm Cortex-M: word 0 is the initial SP, word 1 is the address of the reset handler, then the other exceptions and interrupts). The reset handler, usually in the vendor's startup file, has to establish C's assumptions:

1. Set up the stack (already done by the hardware on Cortex-M; written in assembly on other cores).
2. **Copy `.data` from flash to RAM**: initialized globals (`int counter = 41;`) are *stored* in non-volatile memory but must *live* in RAM, because they are writable. The linker gives each such section two addresses: the **LMA** (load address, in flash) and the **VMA** (run address, in RAM).
3. **Zero `.bss`**: uninitialized and zero-initialized globals are not stored in the image at all (this is why they are free in flash). C promises them zero, so startup code clears their RAM region.
4. Initialize clocks, the FPU, the heap, static constructors where applicable, then call `main`.

On Linux the kernel does steps 2 and 3 for us (it maps `.data` from the file and fills `.bss` with zero pages), which lets us show the *structure* faithfully. Here is a complete freestanding program: it has no libc, defines its own `_start`, makes system calls by hand, and prints values of symbols created by the linker script.

{{include c/embedded/bare/start.c}}

The linker script is what makes the layout explicit:

{{include c/embedded/bare/link.ld}}

{{run c/embedded/bare :: gcc -O2 -ffreestanding -fno-stack-protector -fno-pic -no-pie -nostdlib -static -Wall -Wextra -T link.ld start.c -o start 2>&1 | grep -v build-id; ./start}}

{{run c/embedded/bare :: size start; readelf -lW start | sed -n '1,12p'}}

Reading the result:

- `_stext`, `_etext`, `_sdata`, `_edata`, `_sbss`, `_ebss` are not variables: they are **symbols whose value is an address** chosen by the linker (`extern char _sbss[]`, then use the address). Startup code uses exactly these pairs to find what to copy and clear.
- `counter` started at 41 and prints 0x2a (42) after the increment, so `.data` initial values are in place; the array in `.bss` reads zero.
- `size` shows the three regions that matter on a microcontroller: **text** (code + read-only data, goes to flash), **data** (initialized RAM, costs flash *and* RAM), **bss** (RAM only). The `readelf` program headers show `.bss` as the difference between file size and memory size of the writable segment (the third LOAD, FileSiz `0x4`, MemSiz `0x20`).
- On a microcontroller the script additionally defines a `MEMORY { FLASH ..., RAM ... }` block and writes `.data : { ... } > RAM AT> FLASH` so the LMA is in flash and the VMA in RAM, and exports `_sidata = LOADADDR(.data)` so the startup loop can do `while (dst < &_edata) *dst++ = *src++;`.

## Interrupts: sharing data safely

An **interrupt service routine (ISR)** preempts the main loop at any instruction. Rules that follow:

- Keep ISRs short: acknowledge the device, move data into a queue, set a flag, return.
- Never call blocking functions, `malloc`, or `printf` from an ISR.
- Data shared with an ISR must be `volatile` (so the main loop re-reads it) **and** accessed so that torn reads cannot happen: on an 8-bit core a 16-bit counter needs interrupts disabled around reads; on 32-bit cores single aligned 32-bit accesses are atomic, but read-modify-write is not. Use `<stdatomic.h>` types or disable interrupts for the shortest possible critical section.
- The standard structure is a **single-producer, single-consumer ring buffer**: the ISR is the only writer of `head`, the main loop is the only writer of `tail`, so no lock is needed, only the right ordering (publish the element before publishing the index).

A POSIX signal handler is the closest thing to an ISR on a PC (it interrupts the main flow at any instruction and has restrictions on what it may call). This program is the pattern on that stand-in: a timer fires `SIGALRM` every 200 µs, the handler pushes a counter into the ring, and the main loop drains it, checking that the values arrive in order.

{{include c/embedded/isr_ring.c}}

{{run c/embedded :: gcc -O2 -Wall -Wextra -pthread isr_ring.c -o isr && ./isr}}

If the main loop falls behind, the ring fills; the handler then **drops** the sample and counts it, rather than blocking (an ISR cannot wait). Whether drops occur depends on how fast the consumer is; the invariant that holds either way is that what arrives is in order.

## Squeezing the image

Flash and RAM are scarce, so embedded builds routinely use:

- `-Os` (optimize for size) or `-O2` with `-ffunction-sections -fdata-sections` plus the linker flag `-Wl,--gc-sections`, so every function and variable gets its own section and the linker discards the unreferenced ones:

{{include c/embedded/unused.c}}

{{run c/embedded :: gcc -Os unused.c -o u1 && gcc -Os -ffunction-sections -fdata-sections -Wl,--gc-sections unused.c -o u2 && size u1 u2 && echo "unused symbols kept: $(nm u1 | grep -c unused) vs $(nm u2 | grep -c unused)"}}

- `-fstack-usage` and `-Wstack-usage=N`: per-function stack size; combine with the call graph to bound the worst-case stack, since there is no virtual memory to grow it and overflow silently corrupts other data.
- `-fno-exceptions`, `-fno-rtti` (C++), `-nostdlib` or a small libc (newlib-nano, picolibc), no `printf("%f")`.
- Map files (`-Wl,-Map=out.map`) to see where every byte of flash and RAM went.

## Other things embedded C requires you to know

- **Integer widths and promotion**: `int` may be 16 bits; use fixed-width types and be careful with `uint8_t` arithmetic promoting to `int`.
- **Endianness and alignment**: unaligned access faults on some cores (Cortex-M0), is slow on others; do not cast `uint8_t *` to `uint32_t *` to parse packets; use `memcpy` or explicit shifts.
- **No dynamic allocation** (or only at startup): use static pools; fragmentation on a device that must run for years is a bug in waiting.
- **Watchdogs, brown-out, flash wear**: design for resets; flash has a limited number of erase cycles, so do not write a counter to the same page on every event.
- **Coding standards**: MISRA C and CERT C restrict undefined and implementation-defined behavior; static analyzers enforce them.
- **Testing without hardware**: compile the same logic for the host with the hardware layer behind a small interface, run unit tests there, and run on target for integration.

## Key takeaways

- Freestanding C has no OS: you provide the entry point, the stack, the initialization of `.data`, and the zeroing of `.bss`.
- `volatile` forces each access to happen as written; it is for registers and signal-handler flags, not a substitute for atomics or barriers.
- The linker script decides where sections go and exports boundary symbols (addresses, not variables) that startup code uses; data has a load address in flash and a run address in RAM.
- ISRs must be short and non-blocking; share data through a single-producer/single-consumer ring with publish-before-index ordering.
- `-ffunction-sections -fdata-sections --gc-sections`, `size`, map files and `-fstack-usage` are the everyday tools for fitting in flash and RAM.
