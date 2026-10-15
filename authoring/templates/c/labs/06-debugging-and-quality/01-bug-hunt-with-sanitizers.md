---
title: A Bug Hunt with Sanitizers and Static Analysis
track: c
module: Debugging and Quality
moduleOrder: 6
order: 1
level: advanced
minutes: 30
summary: "Five short programs with real memory and arithmetic bugs (heap overflow, use after free, signed overflow and bad shift, double free, off-by-one read), each to be diagnosed with AddressSanitizer, UBSan and GCC's analyzer, with the reports explained and the fixes shown."
prerequisites:
  - "Debugging with gdb and the Sanitizers"
  - "Static Analysis, Fuzzing and Testing C Code"
  - "Secure C Coding"
sources:
  - label: AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer documentation
    url: https://github.com/google/sanitizers/wiki/AddressSanitizer
  - label: GCC manual — Static Analyzer Options (-fanalyzer) and Warning Options
    url: https://gcc.gnu.org/onlinedocs/gcc/Static-Analyzer-Options.html
  - label: "MITRE CWE: 121/122 (buffer overflow), 415 (double free), 416 (use after free), 190 (integer overflow)"
    url: https://cwe.mitre.org/
---

Debugging is a skill you build by meeting bugs. Each program below has exactly one planted mistake. For every one: **predict** what will go wrong, **run** it under the sanitizers with the helper script, **read** the report from the top (bug class, access, location), then **fix** it and re-run. Do not look at the answers until you have a diagnosis.

The helper builds with `-fsanitize=address,undefined -g`, runs the program and prints the lines of the report that matter (addresses are masked):

{{include c/practice/san.sh}}

## Exercises

**1.** What is wrong here, and what sizes does the report give?

{{include c/practice/p6_bug1.c}}

**2.** The program prints a plausible number without crashing in a normal build. Why is that worse than a crash?

{{include c/practice/p6_bug2.c}}

**3.** Two different undefined behaviours; find both.

{{include c/practice/p6_bug3.c}}

**4.** This crashes only when `fail` is set. Which tool can find it **without running** the program?

{{include c/practice/p6_bug4.c}}

**5.** The sanitizer points at a stack location, not the heap. Explain the report.

{{include c/practice/p6_bug5.c}}

## Answers

Try before opening.

<details>
<summary>Show the reports and the fixes</summary>

**1. Heap buffer overflow.**

{{run c/practice :: ./san.sh p6_bug1.c}}

`malloc(5)` holds four letters and a terminator, but `"Alice"` needs six bytes (five letters and the NUL). The report reads: a **WRITE of size 6** at an address **0 bytes after a 5-byte region**, inside `memcpy` (which `strcpy` became). The compiler even warned at build time ("writing 6 bytes into a region of size 5"): **always read warnings**. Fix: allocate `strlen(s) + 1` bytes, or better use `strdup` / `snprintf` with the size. Overwriting the byte after a heap block corrupts the allocator's bookkeeping or a neighbour, so the crash may come much later and elsewhere, which is why sanitizers beat guessing.

**2. Heap use after free.**

{{run c/practice :: ./san.sh p6_bug2.c}}

The report has three stacks you should read in this order: *where it was used* (line 8, a read of 4 bytes), *where it was freed* and *where it was allocated*. The block is 12 bytes (three ints) and the read is "0 bytes inside" it. Without the sanitizer the freed memory still contains the old value until something reuses it, so the program prints a plausible `90` and passes every test, then fails mysteriously in production when the allocator reuses the block: a **latent** bug. Fix: do not touch memory after `free`; set the pointer to `NULL` after freeing so the mistake becomes an immediate, obvious crash.

**3. Signed overflow and an oversized shift.**

{{run c/practice :: ./san.sh p6_bug3.c}}

UBSan reports two *runtime errors*: `a + b` overflows `int` (undefined, and with optimisation the compiler may assume it cannot happen), and `1 << 32` shifts by the full width of a 32-bit type (undefined; the compiler warned at build time). Fixes: compute the average without overflow (`a + (b - a) / 2`, or use `long long`, or `unsigned` arithmetic where wraparound is acceptable), and use `1u << n` only with `n` below the width (`n < 32`), or `1ull << n` for wider values. UBSan does not stop the program by default, so *check the output for "runtime error"*; use `-fno-sanitize-recover=undefined` to make the first one fatal in tests.

**4. Double free (found two ways).**

{{run c/practice :: ./san.sh p6_bug4.c}}

AddressSanitizer reports the double free when the faulty path actually runs (`process(1)`) and shows both the first free and the allocation. GCC's **static analyzer** finds it *without running anything*, by reasoning along the error path:

{{run c/practice :: gcc -std=c17 -fanalyzer -c -o /dev/null p6_bug4.c 2>&1 | grep -E "warning" | sed 's/^[^ ]* //'}}

(`-fanalyzer` explains the path with numbered events; only the headline is shown.) Static analysis covers paths your tests never take, while sanitizers have no false positives but only see executed code: use both. Fix: remove the second `free` (and prefer one cleanup point: `goto cleanup`, see *Practice: Macros, Callbacks and Error Handling*).

**5. Stack buffer overflow (off by one).**

{{run c/practice :: ./san.sh p6_bug5.c}}

The loop condition `i <= n` visits index 4 of a 4-element array: one **READ of size 4** past the end of `data`, a local variable, so the report says **stack-buffer-overflow** and "at offset 48 in frame" (the position within `main`'s stack frame, not the heap). Everything else is the same story as the heap case. Fix: `i < n`. Off-by-one in loop bounds is one of the commonest C bugs; test with the empty case, the one-element case and a target in the last position.

</details>

## A debugging routine to keep

1. **Reproduce** the bug and make it deterministic (fixed inputs, seeds and thread counts).
2. **Build with `-g -fsanitize=address,undefined -Wall -Wextra`** and run; read the first error report completely, bottom stack frames included.
3. **Run `-fanalyzer`** or another static analyzer over the code, and treat each finding as a possible bug.
4. **Use a debugger** (`gdb`) when you need to inspect state; use watchpoints to find who changes a variable.
5. **Fix the cause, add a regression test**, and keep the sanitizer build in your test suite.

## Key takeaways

1. Read sanitizer reports in order: the bug class, the faulting access (read or write, size), the location relative to the object, then the allocation and free stacks.
2. Silent corruption (use after free, small overflows) can pass normal tests; sanitizers make it loud.
3. UBSan shows undefined arithmetic (signed overflow, bad shifts) but does not stop by default, so check the output.
4. Static analysis (`-fanalyzer`) covers paths tests miss; sanitizers confirm real behaviour. Use both.
