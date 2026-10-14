---
title: "Start Here: How to Use the C Track"
track: c
module: Start Here
moduleOrder: 0
order: 1
level: beginner
summary: "Who the C track is for, what you should know first, three reading paths (beginner, working programmer, expert) and how to practise with the code in each note."
sources:
  - label: Kernighan & Ritchie — The C Programming Language, 2nd ed.
  - label: ISO/IEC 9899:2018 — Programming languages, C (C17)
  - label: King — C Programming, A Modern Approach, 2nd ed.
---

C is the language operating systems, network equipment firmware, databases and language runtimes are written in. Learning it well teaches you how a computer really works: what memory is, what a function call costs, why programs crash. This track goes from a first program to the details senior engineers rely on (the abstract machine, undefined behavior, ABIs, linking, sanitizers, performance).

## What you need before you start

Almost nothing: a Linux machine (or a virtual machine, WSL or a container), a terminal, a text editor, and GCC or Clang. If you have never used a terminal, read **Linux Command Line Basics** in the Linux track first. Check your compiler:

```bash
gcc --version
```

## Pick your path

| You are… | Read in this order |
|---|---|
| **New to programming or new to C** | *C From Zero* → *Binary, Hexadecimal and Bits* → *Pointers for Beginners* → *Practice: C Exercises* → the five **C Essentials** notes (*Structs, Enums and Typedef*, *Functions, Headers and Multi-File Programs*, *More Control Flow*, *Arrays, Strings, Command-Line Arguments and Files*, *From Source to Running Process*) → then the track in order from module 1 |
| **A programmer in another language** | *C From Zero* (skim) → *Pointers for Beginners* → module 1 (toolchain) → module 2 (memory and pointers) → module 6 (debugging) |
| **Experienced; want depth** | Module 1 *C Standards, the Abstract Machine and Undefined Behavior* → module 4 (advanced language) → module 5 (linking) → module 7 (systems-level C) |

Each note states what you should know first in a **Before you read this** box at the top, and lists the primary sources it is based on at the bottom.

## How the notes are built

- **Every listing was compiled and run.** The output you see under a command is what that program printed, not what we expect it to print. Where the result depends on the machine (timings, addresses), the note says so.
- **Concepts first, then the rules, then the traps.** A section titled *Pitfalls* collects the mistakes that actually cost people days.
- **Key takeaways** at the end is a five-line summary you can use for revision.
- **Terms** you do not know are in the [Glossary](../../../glossary/).

## How to practise

1. Type the examples yourself instead of copying; mistakes teach faster than successes.
2. Always compile with warnings on and read every warning:

   ```bash
   gcc -std=c17 -Wall -Wextra -g -o program program.c
   ```
3. When something crashes, rebuild with a sanitizer, which turns silent memory bugs into a clear message (see *Debugging with gdb and the Sanitizers*):

   ```bash
   gcc -std=c17 -g -fsanitize=address,undefined -o program program.c
   ```
4. Do the exercises in *Practice: C Exercises* before reading the answers.
5. Change one thing at a time and predict the result before running it.

## What this track deliberately does not cover

C++, Rust and other languages (except where they interface with C); graphical or web programming; the many library APIs (only the standard library, POSIX file and socket I/O and the pieces needed to explain the systems topics are covered).

## Key takeaways

1. C gives you direct control of memory and the machine; the price is that mistakes are not checked for you.
2. Compile with warnings and sanitizers from day one.
3. Learn in the order: basics, binary and bits, pointers, then the toolchain and the memory model.
4. Run every example and predict its output first.
