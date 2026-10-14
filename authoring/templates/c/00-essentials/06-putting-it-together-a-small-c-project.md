---
title: "Putting It Together: A Small C Project"
track: c
module: C Essentials
moduleOrder: 0.5
order: 6
level: beginner
summary: "A complete multi-file C program built step by step: a word-frequency counter with a hash-table module, a main program, a Makefile with a sanitizer target and a regression test, plus the design habits (small modules, ownership, error handling, tests) that carry over to larger projects."
prerequisites:
  - "Structs, Enums and Typedef"
  - "Functions, Headers and Multi-File Programs"
  - "Arrays, Strings, Command-Line Arguments and Files"
  - "Hash Tables for Beginners: Fast Lookup by Key"
sources:
  - label: Kernighan & Pike — The Practice of Programming, Ch. 1 (Style), 5 (Debugging) and 6 (Testing)
  - label: King — C Programming, A Modern Approach, 2nd ed., Ch. 19 (Program Design)
  - label: Hanson — C Interfaces and Implementations (Addison-Wesley, 1996), Ch. 1 (Introduction)
  - label: GNU Make Manual
    url: https://www.gnu.org/software/make/manual/make.html
---

Knowing the pieces is not the same as building something with them. This note walks through one complete small program, a **word-frequency counter**, and points out the design decisions that keep a project understandable as it grows. It is the last note of the C Essentials module: after it you are ready for the deeper notes.

What the tool does: read text from a file or standard input, split it into words, count how often each word occurs, and print the most frequent ones.

```text
$ printf 'the cat the dog the end\n' | ./wordfreq -n 2
   3  the
   1  cat
(4 distinct words)
```

## Step 1: split the program into modules

A good first design question is "what are the separate jobs?". Here there are two: **a table that counts words** (data structure) and **a main program that reads input and prints results** (everything else). Each job becomes a `.c` file, and the table exposes only a small header. Callers cannot see how it is built, so you can change the inside later without touching `main.c`.

{{include c/essentials/project/table.h}}

Notice the habits in this header:

- `typedef struct Table Table;` declares an **opaque type**: users get a pointer but cannot look inside.
- Every function that can fail says how (`table_add` returns `0` or `-1`; `table_new` returns `NULL`).
- Ownership is written down: *the table owns copies of the words*, and `table_entries` returns an array that the caller must `free`, while the words inside stay owned by the table. Most memory bugs are ownership confusion; comments like these prevent them.
- The include guard and `size_t` from `<stddef.h>` keep the header self-contained.

## Step 2: the table implementation

{{include c/essentials/project/table.c}}

This reuses the chained hash table from *Hash Tables for Beginners*, with a few production habits added:

- **`calloc`** zeroes memory, so all buckets start as `NULL`.
- **Every `malloc` is checked**, and a half-finished insertion is cleaned up (`free(n)` if the word copy fails) so a failure leaks nothing.
- **`table_free` frees in the right order**: each node's word, then the node, then the bucket array, then the table.
- Functions that only read take `const Table *`.

## Step 3: the main program

{{include c/essentials/project/main.c}}

Points worth studying:

- **`count_words` reads one character at a time** with `fgetc`, builds a lowercase word while the characters are letters, and adds it when a non-letter ends it. The length check `len < MAX_WORD` cuts over-long words instead of overflowing the buffer: *every* write into a fixed buffer needs a bounds check.
- **The comparator is deterministic**: higher counts first, then alphabetical order for ties. Without a tie-break the output order would depend on hash-table internals, and a test could not rely on it.
- **Error handling is explicit**: a missing file prints the reason with `perror` and exits with status 1; reading or memory errors are reported on `stderr`.
- **Cleanup on every path**: the table and the entries array are freed before `main` returns.
- `strcmp(argv[i], "-n") == 0 && i + 1 < argc` checks that an option has its value before reading `argv[++i]`.

## Step 4: a Makefile with a safety build and a test target

{{include c/essentials/project/Makefile}}

- `make` builds `wordfreq`; compile flags `-Wall -Wextra -Wpedantic` turn on warnings, which should stay at zero.
- `make asan` rebuilds the same sources with **AddressSanitizer and UBSan**, turning any memory bug into a precise report. Habit: run your tests under it regularly.
- Each `.o` depends on `table.h`, so editing the header rebuilds everything that includes it.
- `make test` runs the regression script.

## Step 5: a test that runs every time

{{include c/essentials/project/test.sh}}

A test does not need a framework. This one feeds known input, compares the full output with the expected text, and also checks two error cases (a missing file gives exit status 1; empty input is handled). Run it:

{{run c/essentials/project :: make clean >/dev/null; make 2>&1 | grep -c -E "warning|error"; make test; make asan 2>&1 | tail -1; printf 'the cat the dog the end\n' | ./wordfreq-asan -n 2; make clean >/dev/null}}

The first number is the count of warnings and errors during the build: zero. All three tests pass, and the sanitizer build runs the program with no reports. A bug fix should start with a failing test that you then make pass, so it can never silently come back.

## Habits this project demonstrates

| Habit | Where it appears |
|---|---|
| One job per module, small public header | `table.h` exposes five functions and an opaque type |
| Write down who owns each allocation | comments in `table.h`; `table_free`, `free(e)` in `main` |
| Check every allocation, file open and library return value | `table_add`, `fopen`, `table_new` |
| Bound every write into a fixed buffer | `len < MAX_WORD` |
| Make output deterministic | comparator with a tie-break |
| Compile with warnings; test under sanitizers | Makefile `CFLAGS` and `asan` target |
| Test automatically | `test.sh` and `make test` |

## Try extending it

1. Add a `-i` option to print the *least* frequent words instead.
2. Make the table **resize** when the number of words exceeds the number of buckets (see *Hash Tables: Open Addressing, Deletion, Cuckoo and Hash Flooding* for how real tables do it).
3. Treat apostrophes as part of a word (`don't`), and add a test first.
4. Read several files given on the command line and merge the counts.
5. Add a test that feeds a 10,000-letter word and confirms the program does not crash.

## Pitfalls

- **Skipping the test until "later"**: the cheapest time to write one is when you first run the program by hand.
- **Putting everything in `main.c`** because it works today; the split into modules pays off the first time you want to reuse or test a part.
- **Hidden ownership**: returning an internal pointer without saying whether the caller may free or keep it.
- **Ignoring warnings** because the program "works"; each one is a possible bug on another compiler or input.

## Key takeaways

1. Design by splitting the program into jobs; give each module a small header and an opaque type where possible.
2. Document ownership and failure behaviour in the header, and check every call that can fail.
3. Keep builds warning-free, rebuild with sanitizers regularly, and automate at least one regression test with `make test`.
4. Deterministic output makes tests trivial; add a tie-break to every sort.
