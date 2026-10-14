---
title: "Functions, Headers and Multi-File Programs"
track: c
module: C Essentials
moduleOrder: 0.5
order: 2
level: beginner
summary: "How to split a C program into several files: declarations versus definitions, header files and include guards, static for private names, a Makefile that rebuilds only what changed, and how to read the two errors you will meet first, 'implicit declaration' and 'undefined reference'."
prerequisites:
  - "C From Zero: Your First Programs"
  - "Structs, Enums and Typedef"
sources:
  - label: Kernighan & Ritchie — The C Programming Language, 2nd ed., Ch. 4 (Functions and Program Structure)
  - label: King — C Programming, A Modern Approach, 2nd ed., Ch. 15 (Writing Large Programs)
  - label: ISO/IEC 9899:2018 §6.9 (External definitions), §6.10.2 (Source file inclusion)
  - label: GNU Make Manual
    url: https://www.gnu.org/software/make/manual/make.html
---

Every example so far fits in one file. Real programs do not: they are split into many `.c` files, each doing one job, so you can find things, reuse them and compile only what changed. Splitting a program needs only three ideas: a **declaration** says a function exists, a **definition** is its actual code, and a **header file** shares declarations between files.

## Declaration versus definition

- A **definition** is the function with its body: `double vec_length(Vec v) { ... }`. There must be exactly one in the whole program.
- A **declaration** (also called a *prototype*) is only the first line followed by a semicolon: `double vec_length(Vec v);`. It tells the compiler the function's name, parameter types and return type, so calls to it can be checked. You can declare a function as many times as you like.

The compiler reads one file at a time, top to bottom. When it meets a call, it must already know the function's shape. That is the whole reason headers exist.

## A program in three files

The header holds the declarations (what the module offers). The `.c` file holds the definitions (how it does it). `main.c` just uses it.

{{include c/essentials/multi/geometry.h}}

{{include c/essentials/multi/geometry.c}}

{{include c/essentials/multi/main.c}}

Points to understand:

- `#include "geometry.h"` literally pastes the header's text into the file during **preprocessing**, before compilation. Quotes search the project directory first; angle brackets (`<stdio.h>`) search the system directories.
- The **include guard** (`#ifndef GEOMETRY_H` ... `#endif`) makes a second inclusion of the same header produce nothing. Without it, a header included twice (directly and through another header) would declare `Vec` twice and fail to compile. Every header you write needs one.
- `geometry.c` includes its own header too, so the compiler checks that the definitions match the declarations.
- `static` on a file-level variable or function means "private to this file": other files cannot see it, and two files may each have their own `static int calls` without clashing. Make everything `static` unless another file needs it.
- Put **declarations** in headers; do not put function definitions or variable definitions in them, or every file that includes the header gets its own copy and the linker complains about duplicates.

## Compiling several files: compile, then link

Each `.c` file is compiled separately into an **object file** (`.o`). The **linker** then joins the object files into one program and connects every call to the matching definition. You can run the steps by hand (`gcc -c main.c`, `gcc -c geometry.c`, `gcc -o app main.o geometry.o -lm`), but a `Makefile` automates it and rebuilds only what changed:

{{include c/essentials/multi/Makefile}}

{{run c/essentials/multi :: make clean >/dev/null; make && ./app; make clean >/dev/null}}

The rule `app: main.o geometry.o` means "`app` depends on these object files; rebuild it with the command on the next line". The command lines must start with a **tab**, not spaces. Edit only `main.c` and run `make` again: only `main.o` is recompiled, then the program is relinked. `-lm` links the math library (needed for `sqrt`). The later note *Build Systems, Compiler Flags and Hardening Options* goes deeper.

## The two errors you will meet first

Forgetting a declaration gives a **compile** error (the compiler does not know the function):

{{include c/essentials/multi/broken.c}}

{{run c/essentials/multi :: gcc -std=c17 -Wall -Wextra broken.c}}

"Implicit declaration" means: you called a function the compiler has never heard of. The fix is to `#include` the right header or add a prototype above the call. Old C silently guessed a signature here, which caused real bugs; C99 and later make it an error.

Declaring a function but never defining it gives a **link** error (the linker cannot find the code):

{{include c/essentials/multi/missing_def.c}}

{{run c/essentials/multi :: gcc -std=c17 -Wall -Wextra -o m missing_def.c; rm -f m}}

"Undefined reference" comes from the linker (`ld`), not the compiler. It means a declaration exists but no object file provides the definition: you forgot to compile or list a `.c` file, or forgot a library such as `-lm`. A quick rule: *error in file:line from gcc* = compile error; *undefined reference from ld* = link error.

## Good habits for functions

- Give each function **one job** and a name that says it. If you need "and" to describe it, split it.
- Put the prototype in a header only when other files need it; otherwise make the function `static`.
- Use `const` on pointer parameters the function only reads (`const char *name`), so the compiler stops accidental writes.
- A function with no parameters is `int f(void)`, not `int f()`; the empty parentheses mean "unspecified arguments" in older C.
- Keep `main` short: parse inputs, call functions, return a status (`0` for success).

## Pitfalls

- **Missing include guard** gives "redefinition of ..." errors that seem to appear from nowhere.
- **Defining a variable in a header** (`int count = 0;`) creates one copy per including file and a link error. Declare it `extern int count;` in the header and define it once in a `.c` file.
- **Editing a header but not rebuilding dependents.** That is why the Makefile rule lists `geometry.h` as a dependency of every `.o`.
- **Circular includes** (A includes B includes A) are broken by include guards plus forward declarations (`struct node;`).
- **Mixing up `""` and `<>`** can pick up the wrong file when a project header shares a name with a system header.

## Key takeaways

1. A declaration says a function exists; a definition is its code. Headers share declarations; `.c` files hold definitions.
2. Every header needs an include guard, and `static` keeps names private to one file.
3. Building is *compile each `.c` to `.o`, then link*. A Makefile rebuilds only what changed.
4. "Implicit declaration" is a compile error (missing prototype); "undefined reference" is a link error (missing definition or library).
