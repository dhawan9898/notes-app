---
title: "From Source to Running Process: A Bridge to Systems C"
track: c
module: C Essentials
moduleOrder: 0.5
order: 5
level: beginner
summary: "The four build stages (preprocess, compile, assemble, link) shown on a tiny program, what an object file and an executable contain, and the memory map of a running process (code, constants, data, bss, heap, stack), as the stepping stone to the ELF, ABI and memory notes."
prerequisites:
  - "C From Zero: Your First Programs"
  - Pointers for Beginners
  - "Functions, Headers and Multi-File Programs"
sources:
  - label: "GCC manual — Overall Options and Preprocessor Options"
    url: https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html
  - label: "man 5 elf; man 1 nm; man 1 size"
    url: https://man7.org/linux/man-pages/man5/elf.5.html
  - label: "Bryant & O'Hallaron — Computer Systems: A Programmer's Perspective, 3rd ed., Ch. 7 (Linking) and §9.7"
  - label: "ISO/IEC 9899:2018 §5.1.1.2 (Translation phases)"
---

The notes after this module go deep: the ELF object model, the System V calling convention, `malloc` internals, the linker. They assume a mental picture that beginner material rarely draws. This note draws it, once, at low resolution. Read it before *The Compilation Pipeline and the ELF Object Model*; then the deep notes will feel like zooming in rather than landing in a foreign country.

## What `gcc` really does: four stages

When you type `gcc hello.c`, four programs run in sequence. You can stop after each one to look at what it produced. Take a tiny program:

```c
#include <stdio.h>
#define GREETING "hello"
int main(void) { puts(GREETING); return 0; }
```

**1. Preprocess** (`gcc -E`): handles every line that starts with `#`. It pastes in headers and replaces macros. Here `GREETING` becomes `"hello"`; the `#include <stdio.h>` line expands into hundreds of declarations (we show only the tail):

```text
$ gcc -E hello_stages.c | tail -3

# 3 "hello_stages.c"
int main(void) { puts("hello"); return 0; }
```

**2. Compile** (`gcc -S`): translates C into **assembly**, the human-readable form of the CPU's instructions. This is where type checking, optimisation and most warnings happen. The program becomes: reserve a little stack, load the address of the string, call `puts`, put 0 in the return register, return.

```text
$ gcc -S -O1 -fno-asynchronous-unwind-tables hello_stages.c -o - | sed -n 1,16p
	.file	"hello_stages.c"
	.text
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC0:
	.string	"hello"
	.text
	.globl	main
	.type	main, @function
main:
	subq	$8, %rsp
	leaq	.LC0(%rip), %rdi
	call	puts@PLT
	movl	$0, %eax
	addq	$8, %rsp
	ret
	.size	main, .-main
```

**3. Assemble** (`gcc -c`): turns assembly into machine code inside an **object file** (`hello_stages.o`). It is not runnable yet, because it contains a call to `puts` whose address it does not know. The symbol table shows this: `T main` is a function this file *defines*, `U puts` is *undefined*, a hole the linker must fill.

```text
$ gcc -c hello_stages.c && file hello_stages.o && nm hello_stages.o
hello_stages.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
0000000000000000 T main
                 U puts
```

**4. Link** (`gcc -o ...`): the linker combines your object files with the C library, fills every hole such as `puts`, and writes an **executable** with a fixed layout so the operating system can load it. `size` reports how big each part is:

```text
$ gcc -o hello_stages hello_stages.c && size hello_stages; rm -f hello_stages hello_stages.o
   text	   data	    bss	    dec	    hex	filename
   1308	    584	      8	   1900	    76c	hello_stages
```

This is the same compile-then-link split you saw with `Makefile`s in *Functions, Headers and Multi-File Programs*. "Undefined reference" errors happen in stage 4; type errors and missing prototypes in stage 2.

## The executable's sections

An executable (on Linux, an **ELF** file) is divided into **sections** by purpose. The three that `size` shows are:

| Section | Holds | Example in your program |
|---|---|---|
| **text** | machine code (and read-only constants, in this summary) | the body of `main`, string literals |
| **data** | global/static variables that have a non-zero starting value | `int count = 42;` |
| **bss** | global/static variables that start at zero | `int total;` (takes no space in the file, only a size) |

The next note in the main track, *The Compilation Pipeline and the ELF Object Model*, opens these files with `readelf` and `objdump` and explains symbols, relocations and the loader.

## What the memory of a running process looks like

When the program starts, the operating system gives it a private **virtual address space**: a long row of addresses that belong only to this process. Each kind of data lives in its own region. The next program prints one address from each region:

```c
#include <stdio.h>
#include <stdlib.h>

int initialised_global = 42;           /* .data: has a starting value */
int zeroed_global;                     /* .bss: zero at start, takes no space in the file */
const char message[] = "read-only";    /* .rodata: constants */

int main(void) {
    int on_stack = 1;
    int *on_heap = malloc(sizeof *on_heap);
    if (!on_heap) return 1;

    printf("code   (main)             %p\n", (void *)main);
    printf("rodata (message)          %p\n", (const void *)message);
    printf("data   (initialised)      %p\n", (void *)&initialised_global);
    printf("bss    (zeroed)           %p\n", (void *)&zeroed_global);
    printf("heap   (malloc)           %p\n", (void *)on_heap);
    printf("stack  (local variable)   %p\n", (void *)&on_stack);
    free(on_heap);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -o layout layout.c && ./layout; rm -f layout
code   (main)             0x563aa1b44159
rodata (message)          0x563aa1b45008
data   (initialised)      0x563aa1b47028
bss    (zeroed)           0x563aa1b47030
heap   (malloc)           0x563ad21312a0
stack  (local variable)   0x7ffd765e9164
```

Reading the numbers (they change on every run, because the system randomises the layout, called **ASLR**; the *order and relative distances* are what matter):

| Region | Grows? | What lives there | Lifetime |
|---|---|---|---|
| **code / text** | no | machine instructions (`main`) | whole run, read-only |
| **rodata** | no | string literals, `const` arrays | whole run, read-only |
| **data** and **bss** | no | global and `static` variables | whole run |
| **heap** | upward | memory from `malloc` | until you `free` it |
| **stack** | downward | local variables, one **frame** per active function call | until the function returns |

Notice the very large gap between the heap's address and the stack's: they sit at opposite ends of the address space so each can grow without colliding.

This one table explains most beginner bugs:

- **Returning a pointer to a local variable**: the variable lived in a stack frame that is gone after the function returns (dangling pointer).
- **Writing past the end of a local array**: neighbouring stack data, possibly the return address, is overwritten (stack buffer overflow).
- **Forgetting `free`**: heap memory is never reclaimed until the process exits (memory leak).
- **Using memory after `free`**: the heap block may be reused by someone else (use-after-free).
- **Writing to a string literal** (`char *s = "abc"; s[0] = 'x';`): it lives in read-only memory, so the program crashes.
- **Very deep recursion**: each call adds a frame; the stack region is limited (typically 8 MiB) and overflows.

## What happens when you press Enter

1. The shell asks the kernel to start the program (`fork` and `exec`, see *Process Lifecycle — fork, exec, wait* in the Linux track).
2. The kernel maps the code, data and bss sections into a fresh address space, sets up a stack with `argc`/`argv`, and starts the **C runtime** startup code.
3. That startup code initialises the C library and then calls your `main`.
4. When `main` returns, the value becomes the process's **exit status** and the kernel reclaims all of its memory and open files.

## Where to go next

You now have enough background to start the main track:

1. *The Compilation Pipeline and the ELF Object Model* (stage 3 and 4 in detail),
2. *C Standards, the Abstract Machine and Undefined Behavior* (why some bugs are not "just wrong output"),
3. *Types, Integer Promotion, Conversions and Overflow*,
4. then *Pointers, Arrays and Decay* and *Dynamic Memory*.

## Key takeaways

1. Building a program is four stages: preprocess, compile, assemble, link; each can fail in its own way and each can be inspected (`-E`, `-S`, `-c`).
2. An object file has holes (undefined symbols) that the linker fills; "undefined reference" is a link-time error.
3. A running process has distinct regions: code, read-only data, data/bss for globals, a heap that grows upward and a stack that grows downward.
4. Most memory bugs are really "I used memory from the wrong region or after its lifetime ended".
