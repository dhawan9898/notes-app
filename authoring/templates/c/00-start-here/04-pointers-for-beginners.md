---
title: Pointers for Beginners
track: c
module: Start Here
moduleOrder: 0
order: 4
level: beginner
summary: "What memory and addresses are, what & and * do, why functions need pointers to change a caller's variable, how arrays relate to pointers, malloc and free, and the three classic pointer bugs, shown with real crashes and sanitizer reports."
prerequisites:
  - "C From Zero: Your First Programs"
  - Binary, Hexadecimal and Bits
sources:
  - label: Kernighan & Ritchie — The C Programming Language, 2nd ed., Ch. 5 (Pointers and Arrays)
  - label: King — C Programming, A Modern Approach, 2nd ed., Ch. 11-12 and 17
  - label: ISO/IEC 9899:2018 §6.5.3.2 (Address and indirection operators), §6.5.6 (Additive operators), §7.22.3 (Memory management functions)
  - label: AddressSanitizer — Clang/GCC documentation
    url: https://github.com/google/sanitizers/wiki/AddressSanitizer
---

Pointers are the idea that makes C powerful and the one beginners fear. The fear is unnecessary once the picture is clear, so this note builds the picture first and the syntax second.

## The picture: memory is a long row of numbered boxes

Your program's memory is a huge row of **bytes**, each with a number called its **address** (0, 1, 2, ...). A variable lives in one or more of those boxes: an `int` occupies 4 consecutive bytes, so it has the address of its first byte. Think of memory as a street, a variable as a house, and its address as the house number.

A **pointer** is a variable whose value is *an address*: a piece of paper with a house number written on it. It lets you find, read and change another variable *indirectly*.

## The two operators

- `&x` means "the address of `x`" (what is x's house number?).
- `*p` means "the thing `p` points at" (go to the house number written on the paper).

And in a declaration, `int *p` says "`p` is a pointer to an `int`".

{{include c/beginner/ptr1.c}}

{{run c/beginner :: gcc -std=c17 -Wall -Wextra -o ptr1 ptr1.c && ./ptr1}}

Read the output line by line: `*p` reads `age` through the pointer; `*p = 31` *writes* `age` through the pointer; and `p == &age` is true because `p` holds exactly that address. A pointer on a 64-bit system is 8 bytes, because an address is a 64-bit number.

## Why do we need them? Functions get copies

When you call `f(age)`, C passes a **copy** of `age`. The function can scribble on its copy; the original is unchanged. That is what `add_one_wrong` shows: after the call, `age` is the same. If the function needs to *change* the caller's variable, give it the variable's **address** and let it write through the pointer: `add_one(&age)`. The same trick makes `swap` possible, and it is also how a function returns several results (by writing into pointers the caller passed in) and how big data is passed cheaply (an 8-byte address instead of a copy of a large structure).

## Arrays and pointers

An array is a row of same-typed boxes in consecutive memory. The name of an array, used in an expression, becomes **a pointer to its first element**. That is why `a[2]`, `*(p + 2)` and `p[2]` all mean the same thing. Adding to a pointer moves it by **whole elements**, not bytes: `p + 1` is the next `int`, 4 bytes along. You can walk an array with a pointer, as the loop in the program shows.

Important: an array is **not** a pointer. `sizeof a` gives the whole array's size, `sizeof p` gives 8. The two stories are told in full in *Pointers, Arrays and Decay* later in the track.

## Getting memory at run time: malloc and free

Local variables disappear when their function returns, and their size is fixed when you write the code. When you need memory whose size is known only at run time, or that must outlive a function, you ask for it from the **heap**: `malloc(bytes)` returns a pointer to that many fresh bytes (or `NULL` if there is no memory left). You must hand it back with `free` **exactly once**, or the program *leaks* memory.

{{include c/beginner/ptr2.c}}

{{run c/beginner :: gcc -std=c17 -Wall -Wextra -o ptr2 ptr2.c && ./ptr2}}

Habits that prevent most bugs:

- `malloc(n * sizeof *buf)`: the size of one element is taken from the pointer itself, so it stays right if the type changes.
- Always check for `NULL`.
- Strings need one extra byte for the terminating `'\0'` (`strlen(...) + 1`).
- `free` every `malloc`, once, then set the pointer to `NULL`.

## The three classic pointer bugs

Every beginner meets these. The program below triggers each one depending on how it is run, so you can see what they look like.

{{include c/beginner/ptr_bugs.c}}

**1. Dereferencing NULL.** `NULL` means "points at nothing"; reading through it makes the operating system kill the program with a *segmentation fault*:

{{run c/beginner :: gcc -std=c17 -g -Wall -Wextra -o ptr_bugs ptr_bugs.c 2>/dev/null; sh -c './ptr_bugs; echo "exit status: $?"' 2>&1 | sed 's/^sh: [0-9]*: //'}}

Exit status 139 is 128 + 11, where 11 is the signal number of SIGSEGV. A crash is the *good* outcome: it is loud.

**2. Use after free** (GCC can sometimes warn about this one at compile time) and **3. Reading past the end of an array** are worse: they often *do not crash*, and silently read or corrupt someone else's data. The **AddressSanitizer** catches them and tells you exactly what happened:

{{run c/beginner :: gcc -std=c17 -g -fsanitize=address -o ptr_bugs_asan ptr_bugs.c && (./ptr_bugs_asan x 2>&1 | grep -E "ERROR|READ of|freed by|previously allocated" | sed 's#/[^ ]*/##g; s/==[0-9]*==//; s/ on address.*//; s/ at 0x[0-9a-f]* thread T0//' ; ./ptr_bugs_asan x y 2>&1 | grep -E "ERROR|READ of" | sed 's#/[^ ]*/##g; s/==[0-9]*==//; s/ on address.*//; s/ at 0x[0-9a-f]* thread T0//')}}

The first report says the program read 4 bytes from memory that had been freed (and where it was allocated and freed); the second that it read past the end of a stack array. **Always develop with `-fsanitize=address,undefined`.**

## A mental checklist before you use a pointer

1. Does it point at something valid (not NULL, not freed, not past the end)?
2. Is the thing it points at still alive (a local variable's address must not be returned from its function)?
3. Do I know how many elements are there?
4. Who owns it: who must call `free`, and when?

## Pitfalls

- Forgetting `&` when passing a variable the function should modify.
- `int *a, b;` declares one pointer and one plain `int`.
- Using an uninitialised pointer (random address): initialise to `NULL` or a valid address.
- Returning a pointer to a local variable.
- Using `sizeof` on a pointer parameter expecting the array size (it gives 8).
- Freeing twice, or freeing a pointer you did not get from `malloc`.
- Pointer arithmetic outside the array.

## Key takeaways

1. Memory is numbered bytes; a pointer holds an address; `&` takes an address, `*` follows one.
2. Functions receive copies, so pass an address when a function must change the caller's variable.
3. An array name becomes a pointer to its first element and `p + 1` moves by one element.
4. `malloc`/`free` manage run-time memory: check for `NULL`, free exactly once.
5. NULL dereference, use after free and out-of-bounds access are the classic bugs; sanitizers find them.

**Next:** *Practice: C Exercises*, then module 2 (*Memory and Pointers*) for the precise rules.
