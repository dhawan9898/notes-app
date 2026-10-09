---
title: Stack Frames and the x86-64 System V Calling Convention
track: c
module: Memory and Pointers
moduleOrder: 2
order: 2
level: advanced
summary: How function calls really work — registers vs stack arguments, prologue and epilogue, struct returns, stack growth and limits, stack canaries — shown with disassembly and runtime addresses from real programs.
sources:
  - label: "System V Application Binary Interface - AMD64 Architecture Processor Supplement (psABI)"
    url: https://gitlab.com/x86-psABIs/x86-64-ABI
  - label: "Intel 64 and IA-32 Architectures Software Developer's Manual, Vol. 1 Ch. 6 (Procedure Calls)"
  - label: "GCC manual — Instrumentation Options (-fstack-protector), Function Attributes, __builtin_frame_address"
    url: https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html
  - label: "Aleph One — Smashing the Stack for Fun and Profit (Phrack 49, 1996)"
    url: http://phrack.org/issues/49/14.html
  - label: "Bryant & O'Hallaron — Computer Systems - A Programmer's Perspective, Ch. 3"
---

Every function call needs somewhere to keep arguments, local variables, saved registers and the way back to the caller. On x86-64 Linux the contract that governs this is the **System V AMD64 ABI**. Knowing it makes crash dumps, stack overflows, `-fstack-protector`, variadic functions, debuggers and profilers (frame-pointer unwinding) understandable.

## Registers and argument passing

| Role | Registers |
|---|---|
| Integer/pointer arguments 1–6 | `rdi, rsi, rdx, rcx, r8, r9` |
| Floating-point arguments 1–8 | `xmm0–xmm7` |
| Return value | `rax` (and `rdx` for 128-bit); `xmm0` for floating point |
| **Callee-saved** (a function must preserve them) | `rbx, rbp, r12–r15` |
| **Caller-saved** (may be clobbered by any call) | `rax, rcx, rdx, rsi, rdi, r8–r11`, all `xmm` |
| Stack pointer | `rsp` (must be **16-byte aligned at the point of a `call`**) |
| Syscalls (for comparison) | number in `rax`, arguments in `rdi, rsi, rdx, r10, r8, r9` |

Arguments beyond the sixth integer argument go on the **stack**, pushed right to left. Small structs (≤ 16 bytes) may be split across registers; larger structs are passed in memory and returned through a hidden pointer. For variadic functions (`printf`) `rax` holds the number of vector registers used (hence `mov $0, %eax` before `call printf`).

A program with eight arguments and a 24-byte struct return (`frames.c`):

```c
#include <stdio.h>

/* 8 arguments: the first six travel in registers, the rest on the stack */
long many(long a, long b, long c, long d, long e, long f, long g, long h) {
    long local = a + b + c + d + e + f + g + h;
    return local;
}

struct big { long x, y, z; };
struct big make_big(long v) { struct big s = {v, v + 1, v + 2}; return s; }   /* returned through a hidden pointer */

int main(void) {
    long r = many(1, 2, 3, 4, 5, 6, 7, 8);
    struct big b = make_big(10);
    printf("%ld %ld\n", r, b.z);
    return 0;
}
```

Compiled with `gcc -O0 -fno-omit-frame-pointer` (unoptimized, so every step is visible):

```text
$ awk '/^main:/,/\.size\tmain/' frames.s | sed -n 1,26p
awk: cannot open "frames.s" (No such file or directory)
```

In `main`, arguments 7 and 8 are pushed on the stack (`pushq $8; pushq $7`), arguments 1–6 go in registers, then `call many`; afterwards `addq $16, %rsp` cleans up the two pushed slots (the caller removes stack arguments in the System V convention). For `make_big`, `main` passes the **address of its local struct in `%rdi`** (`leaq -32(%rbp), %rax; movq %rax, %rdi`), so the real arguments shift: `v` arrives in `%rsi`.

```text
$ awk '/^many:/,/\.size\tmany/' frames.s | grep -v '^\s*\.' | sed -n 1,12p
awk: cannot open "frames.s" (No such file or directory)
```

Look at how `many` receives its parameters: the six register arguments are spilled to its frame (`-24(%rbp)` … `-64(%rbp)`) only because this is `-O0`; the 7th and 8th are read from **`16(%rbp)` and `24(%rbp)`**, above the saved `rbp` (offset 0) and the return address (offset 8). The result goes in `%rax`.

## The stack frame

The stack grows toward **lower addresses**. A call executes `push return-address; jmp callee`; the callee typically starts with a *prologue*:

```text
push  %rbp            ; save caller's frame pointer
mov   %rsp, %rbp      ; rbp now marks this frame's base
sub   $N, %rsp        ; allocate space for locals and spills (keeping rsp 16-byte aligned)
...
leave                 ; mov %rbp,%rsp ; pop %rbp   (epilogue)
ret                   ; pop return address into rip
```

```text
 higher addresses
 +---------------------+
 | argument 8          |  24(%rbp)
 | argument 7          |  16(%rbp)
 | return address      |   8(%rbp)   <- pushed by CALL
 | saved rbp           |   0(%rbp)   <- %rbp points here
 | locals / spills     |  -8(%rbp), ...
 | (alignment padding) |
 +---------------------+  <- %rsp
 lower addresses
```

- **Frame pointer**: GCC omits `rbp` as a frame pointer at `-O1+` (`-fomit-frame-pointer`), using `rsp`-relative addressing and freeing a register. Debuggers then rely on DWARF `.eh_frame` unwind tables; fast sampling profilers (`perf`) rely on frame pointers, which is why some distributions rebuild everything with `-fno-omit-frame-pointer`.
- **Red zone**: leaf functions may use the 128 bytes *below* `rsp` without adjusting it (the kernel and signal handlers respect this on x86-64 user space; the Linux kernel itself is built with `-mno-red-zone` because interrupts push onto the same stack).
- **Alignment**: `rsp` must be 16-byte aligned before `call` (so it's 8 mod 16 on function entry, after the return address is pushed). Misalignment causes crashes in SSE instructions (`movaps`) inside `printf` or in your own vectorized code, a typical symptom in hand-written assembly or after a bad `longjmp`.
- **Frame size** is visible at run time: with `__builtin_frame_address(0)` and `__builtin_return_address(0)` (`addrs.c`):

```c
#include <stdio.h>
#include <stdint.h>

static void show(int depth) {
    int local = depth;
    printf("depth %d: &local=%p frame=%p return address=%p\n", depth, (void *)&local, __builtin_frame_address(0), __builtin_return_address(0));
    if (depth < 2) show(depth + 1);
}

int main(void) {
    printf("main is at %p, show is at %p\n", (void *)main, (void *)show);
    show(0);
    return 0;
}
```

```text
$ gcc -O0 -no-pie -fno-omit-frame-pointer addrs.c -o addrs && ./addrs
main is at 0x401177, show is at 0x401126
depth 0: &local=0x7ffe092e644c frame=0x7ffe092e6450 return address=0x4011ad
depth 1: &local=0x7ffe092e641c frame=0x7ffe092e6420 return address=0x401174
depth 2: &local=0x7ffe092e63ec frame=0x7ffe092e63f0 return address=0x401174
```

Each recursive call moved the frame down by `0x30` = 48 bytes (frame pointer values 0x…640 → 0x…610 → 0x…5e0). The return address `0x4011ad` points into `main` (just after the call to `show`), and `0x401174` into `show` itself (the recursive call). Compile as PIE (the default) and the code addresses change on each run (ASLR), while the *relative* offsets stay fixed:

```text
$ gcc -O0 -fno-omit-frame-pointer addrs.c -o addrs_pie && ./addrs_pie | head -1 && ./addrs_pie | head -1
main is at 0x55cce096a18a, show is at 0x55cce096a139
main is at 0x555a1f8c918a, show is at 0x555a1f8c9139
```

## What lives on the stack, and what doesn't

- **Automatic variables** (locals), including arrays, *uninitialized* until assigned (they contain whatever the previous call left behind: reading them is undefined behavior and a source of information leaks).
- **Spilled registers and saved callee-saved registers**.
- **Arguments** beyond registers; **return addresses**; **saved frame pointers**.
- **Not** `static`/global variables (`.data`/`.bss`), string literals (`.rodata`) or `malloc`ed data (heap).

Lifetime: locals die when the function returns. Returning `&local` or storing it in a longer-lived structure gives a **dangling pointer** (stack-use-after-return); GCC warns (`-Wreturn-local-addr`), AddressSanitizer detects (`ASAN_OPTIONS=detect_stack_use_after_return=1`).

Large locals (big arrays, `alloca`, VLAs) can exhaust the stack. The stack of the main thread is limited by `ulimit -s` (default **8 MiB**, grows on demand up to the limit); other threads get a fixed size at creation (`pthread_attr_setstacksize`, default usually 8 MiB virtual, 2 MiB for some libcs/musl: 128 KiB). Runaway recursion (`depth.c`: 288-byte frames):

```text
$ ./depth 2>&1 | tail -n 3; echo "--- with ulimit -s 1024:"; (ulimit -s 1024; ./depth 2>&1 | tail -n 2); echo "exit status $?"
/bin/bash: line 1: ./depth: No such file or directory
--- with ulimit -s 1024:
/bin/bash: line 1: ./depth: No such file or directory
exit status 0
```

At 8 MiB the recursion crashed with `SIGSEGV` after about 30,000 frames (8 MiB / ~288 B); with a 1 MiB stack the limit is hit near 3,600. The kernel places a **guard page** below the stack so overflow faults immediately rather than silently corrupting the heap. Mitigations: convert deep recursion to iteration with an explicit heap-allocated stack, limit recursion depth on untrusted input (parsers, JSON/XML), allocate big buffers with `malloc`, and avoid VLAs/`alloca` with attacker-controlled sizes (`-Wvla`, `-Wstack-usage=N`, `-fstack-clash-protection`).

## Stack buffer overflows and canaries

Because the return address sits just above local buffers, writing past the end of a local array can overwrite it, redirecting control flow when the function returns ("smashing the stack"). With attacker-chosen data this yields code execution (return-oriented programming against NX stacks). A deliberately unsafe program:

```c
#include <stdio.h>
#include <string.h>

/* Deliberately unsafe: copies without checking the length. */
static void copy(const char *s) {
    char buf[16];
    strcpy(buf, s);
    printf("copied: %s\n", buf);
}

int main(int argc, char **argv) {
    copy(argc > 1 ? argv[1] : "short");
    puts("returned normally");
    return 0;
}
```

Built twice, with and without `-fstack-protector-strong`, and run with a 32-byte argument:

```text
$ gcc -O0 -fno-stack-protector -o canary_off canary.c 2>/dev/null; gcc -O0 -fstack-protector-strong -o canary_on canary.c; ./canary_off AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA; echo "exit status: $?"; ./canary_on AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA; echo "exit status: $?"
exit status: 139
exit status: 134
/bin/bash: line 1: 2427559 Segmentation fault      (core dumped) ./canary_off AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
*** stack smashing detected ***: terminated
/bin/bash: line 1: 2427560 Aborted                 (core dumped) ./canary_on AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
```

The unprotected binary crashed with `SIGSEGV` (status 139 = 128 + 11: the saved return address was overwritten with 'A' bytes and `ret` jumped to garbage); the protected one printed `*** stack smashing detected ***: terminated` and aborted with `SIGABRT` (status 134). The compiler inserted a **stack canary**:

```text
$ objdump -d --no-show-raw-insn canary_on | awk '/<copy>:/,/ret/' | grep -E 'fs:|stack_chk'
    1175:	mov    %fs:0x28,%rax
    11b7:	sub    %fs:0x28,%rax
    11c2:	call   1050 <__stack_chk_fail@plt>
```

On entry it loads a random value from thread-local storage (`%fs:0x28`, chosen at process start from the auxiliary vector `AT_RANDOM`) and stores it between the locals and the saved registers; before returning it re-reads and compares: any contiguous overflow that reaches the return address must also overwrite the canary, so the check fails and `__stack_chk_fail` aborts. Limits: it doesn't protect against non-contiguous writes (e.g. an arbitrary-write primitive), leaks of the canary, or corruption of other locals/function pointers before the canary; it detects the overflow only at return.

The layered defenses that make exploitation harder (none of which fixes the bug):

| Mitigation | Mechanism | Flag/where |
|---|---|---|
| **NX / W^X** | stack and heap not executable (`GNU_STACK` RW) | linker default (`-z noexecstack`) |
| **ASLR / PIE** | randomize stack, heap, libraries, executable base | kernel `randomize_va_space=2`, `-fPIE -pie` |
| **Stack canaries** | detect linear overflow | `-fstack-protector-strong` (Ubuntu default) |
| **`_FORTIFY_SOURCE`** | replace `strcpy/memcpy/sprintf` with checked versions when sizes are known | `-O2 -D_FORTIFY_SOURCE=2` (or `3`) |
| **RELRO / BIND_NOW** | make GOT read-only | `-Wl,-z,relro,-z,now` |
| **Shadow stack / CET** | hardware-protected return addresses | `-fcf-protection=full`, kernel/CPU support |
| **Stack clash protection** | probe each page when allocating big frames | `-fstack-clash-protection` |
| **Fix the bug** | bounds-checked code, safe APIs, sanitizers | next notes |

## Other consequences of the calling convention

- **Variadic functions** read arguments via `va_list`, which saves all register arguments into a "register save area" at entry; calling a variadic function through a non-variadic prototype is UB. `printf("%d", long_value)` reads the wrong width.
- **Tail calls**: at `-O2` a call in tail position can be compiled to a `jmp` (no new frame), which makes stack traces "lose" frames and allows unbounded tail recursion to work (not guaranteed by C).
- **Callee-saved registers** explain the push/pop sequences at function boundaries: a function using `rbx` saves and restores it.
- **Inlining** removes calls entirely; `__attribute__((noinline))` for profiling/benchmark code; `-fno-inline` for debugging.
- **setjmp/longjmp** save/restore `rsp`, `rbp`, callee-saved registers and the return address, skipping frames without running cleanup.
- **Signals** run handlers on the current stack below the red zone (or on `sigaltstack`).
- **Debuggers and unwinders** reconstruct the call chain either from the `rbp` chain (frame pointers) or from `.eh_frame` DWARF CFI; both are walked by `gdb`'s `bt`, `perf record -g`, `backtrace(3)`, and exceptions in C++.
- **Different ABIs**: Windows x64 passes the first four integer arguments in `rcx, rdx, r8, r9` with 32 bytes of shadow space; 32-bit x86 (cdecl) passes everything on the stack; ARM64 uses `x0–x7` and `lr`. Mixing assembly and C across ABIs is a classic porting bug.

## Practical debugging

```bash
gcc -g -O0 -fno-omit-frame-pointer prog.c           # best for stepping and exact frames
gdb ./prog  ->  bt, frame 2, info frame, info locals, x/16gx $rsp, p $rip, disassemble /s
objdump -d -M intel --no-show-raw-insn prog | less  # see prologues, calls
gcc -S -O2 -masm=intel prog.c -o -                  # what the optimizer did
valgrind --tool=memcheck ./prog ; gcc -fsanitize=address -g prog.c    # stack overflow reports with frame layout
ulimit -s unlimited | ulimit -s 65536               # change the main thread stack limit (shell)
```

An AddressSanitizer stack-buffer-overflow report prints the frame layout with variable names and offsets, which is the fastest way to find which local was overrun.

## Key takeaways

- System V x86-64: integer args in `rdi, rsi, rdx, rcx, r8, r9`, results in `rax`/`xmm0`, extra args on the stack, `rsp` 16-byte aligned at `call`; `rbx, rbp, r12–r15` are callee-saved.
- A frame holds saved `rbp`, locals and spills below the return address; frames are 48 bytes in the example, and the stack grows downward with a guard page and an 8 MiB default limit.
- Locals are uninitialized and die at return; never return pointers to them; deep recursion overflows the stack (≈ 30,000 frames of 288 B in 8 MiB).
- Stack smashing overwrites the return address; canaries, NX, ASLR, fortify, CET add defense in depth, but the real fix is bounds-correct code.
