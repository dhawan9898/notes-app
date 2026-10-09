---
title: Static Analysis, Fuzzing and Testing C Code
track: c
module: Debugging and Quality
moduleOrder: 6
order: 2
level: advanced
summary: Finding bugs before users do — compiler warnings and GCC's static analyzer, a unit-test harness with line and branch coverage, and coverage-guided fuzzing explained through a working mutation fuzzer that finds a stack overflow in seconds.
sources:
  - label: GCC manual — Static Analyzer Options (-fanalyzer); Instrumentation Options (--coverage); gcov
    url: https://gcc.gnu.org/onlinedocs/gcc/Static-Analyzer-Options.html
  - label: LLVM libFuzzer documentation; AFL++ documentation
    url: https://llvm.org/docs/LibFuzzer.html
  - label: Clang Static Analyzer and clang-tidy checks; Cppcheck; CodeQL for C
    url: https://clang.llvm.org/docs/analyzer/checkers.html
  - label: "Böhme, Pham, Roychoudhury — Coverage-based Greybox Fuzzing as Markov Chain (CCS 2016)"
  - label: OSS-Fuzz — continuous fuzzing for open source
    url: https://google.github.io/oss-fuzz/
  - label: Beck — Test Driven Development; Google Testing Blog; Kerr — Unity / cmocka / Check frameworks
    url: https://cmocka.org/
---

No single technique finds all bugs. They complement each other: **static analysis** reasons about code without running it (finds shallow bugs on all paths), **tests** check intended behavior on chosen inputs, **fuzzing** explores unintended inputs automatically, and **sanitizers** make hidden corruption visible while the other two run.

## Static analysis

### The compiler is the first analyzer

`-Wall -Wextra` plus the extra warnings from the build note catch uninitialized variables, format mismatches, sign-compare, shadowing, missing returns, unused results and many API misuses; `-O2` enables more dataflow warnings (some only appear with optimization). **Treat warnings as defects**.

### `gcc -fanalyzer`

GCC 10+ includes an interprocedural analyzer that symbolically executes paths, finding double-free, use-after-free, leaks (memory, files), null dereference, uninitialized values, buffer overflows with known bounds, tainted-data uses. A file with four bugs:

{{include c/debug/analyze.c}}

{{run c/debug :: gcc -fanalyzer -O0 -c analyze.c -o /dev/null 2>&1 | grep -E 'warning:' | sed 's/analyze.c://' | cut -c1-120}}

It flagged the unchecked `malloc` result passed to `strcpy`, the use after `free`, and the `FILE *` leak on the early-return path (plus the unchecked `fopen` result). Analyzer output includes the full path through the code that triggers each warning (`-fdiagnostics-path-format=inline-events`). Expect false positives and slowness on big code bases; it is C-only in GCC (C++ support is limited).

### Other analyzers

| Tool | Notes |
|---|---|
| **clang-tidy / Clang Static Analyzer** (`scan-build make`) | path-sensitive checks (`clang-analyzer-*`), CERT (`cert-*`), bug-prone patterns (`bugprone-*`), modernization; configurable via `.clang-tidy`, driven by `compile_commands.json` |
| **Cppcheck** | fast, few false positives, no compile needed; MISRA add-on |
| **CodeQL** (GitHub), **Coverity**, **Infer**, **Semgrep** | queryable code databases, interprocedural taint analysis, custom rules |
| **Coccinelle** (`spatch`) | semantic patches for large-scale refactoring and bug patterns (heavily used in the Linux kernel) |
| **Sparse**, **smatch** (kernel) | annotation-driven checking (`__user`, `__rcu` address spaces, locking) |
| **Frama-C**, **CBMC** | formal verification / bounded model checking (prove absence of overflow for small functions) |
| **Include-what-you-use**, **clang-format** | hygiene and formatting enforcement |

Run analyzers in CI on every change; baseline existing findings and fail on *new* ones; fix or annotate with a justification rather than disabling checks globally.

Standards: **MISRA C** (automotive/embedded), **CERT C**, **CWE** lists, and **ISO 26262/DO-178C** rules require analyzers with documented coverage in regulated domains.

## Unit testing

C has no built-in test runner, but a test needs little: functions that call code and compare results, an exit status, and clear failure messages. Frameworks (Unity, cmocka, Check, CUnit, Criterion, greatest, µnit) add fixtures, mocks and reports; a 15-line macro harness is often enough to start:

{{include c/testing/mathutil.h}}

{{include c/testing/mathutil.c}}

{{include c/testing/test_mathutil.c}}

### Coverage with gcov

`--coverage` (= `-fprofile-arcs -ftest-coverage`) instruments the build; running the tests writes `.gcda` data and `gcov` annotates the source. Compile each file separately so the data files are named after the sources:

{{run c/testing :: gcc -std=c11 -Wall -Wextra --coverage -O0 -c mathutil.c && gcc -std=c11 -Wall -Wextra --coverage -O0 -c test_mathutil.c && gcc --coverage mathutil.o test_mathutil.o -o test_mathutil && ./test_mathutil && gcov -b mathutil.c 2>&1 | sed -n 2,5p; sed -n '/int isqrt/,/ 21:/p' mathutil.c.gcov | cut -c1-72; rm -f *.gcov *.gc* *.o test_mathutil}}

The tests executed **100 % of lines** but only **90 % of branches were taken**: `if (n < 0) return -1;` shows `branch 0 taken 0%`: no test passes a negative number. Line coverage reached 100 % because the line was executed (the condition evaluated), without ever exercising the true case. Hence:

- Prefer **branch (and condition/MC-DC)** coverage to line coverage; tools: `gcov -b`, `lcov`/`genhtml` HTML reports, `gcovr`.
- Coverage shows what is *not* tested; it doesn't prove what is tested well (assertions can be weak).
- Aim for risk-driven coverage: error paths, boundaries (`INT_MIN`, `INT_MAX`, 0, 1, empty, huge), and every `goto cleanup` path.

### Writing testable C

1. Keep logic in pure functions with explicit inputs/outputs; isolate I/O, time, randomness and globals behind small interfaces (function pointers / link-time substitution).
2. **Seams for mocking**: pass callbacks/ops tables; use the linker (`-Wl,--wrap=malloc` redirects `malloc` calls to `__wrap_malloc`, handy for failure injection); `LD_PRELOAD` shims; weak symbols.
3. **Fault injection**: make `malloc`, `fopen`, `read` fail on the N-th call and verify cleanup (tests the `goto out` paths and leak-free error handling under ASan). Loop N from 1 until no more failures occur.
4. **Property-based/table tests**: check invariants over many generated inputs (`sort` output is ordered and a permutation; `decode(encode(x)) == x`), plus golden tests for parsers.
5. **Run under sanitizers** (ASan/UBSan/TSan) and Valgrind in CI.
6. **Determinism**: no wall-clock, fixed seeds, temporary directories, parallel-safe tests.
7. **Integration/system tests** with real files, sockets and subprocesses; **ctest** (`add_test`) or a `make check` target orchestrates them; keep tests fast (seconds) so they run on every commit.

## Fuzzing

Fuzzing feeds a program huge numbers of mutated inputs looking for crashes, hangs and sanitizer reports. It is the most effective technique for code that **parses untrusted data** (file formats, network protocols, decompressors, regexes, XML/JSON, image codecs): the bugs it finds are exactly the ones attackers exploit.

### A working mutation fuzzer in 70 lines

A deliberately buggy parser for records `'R' <len> <payload>` ... `'E'`: the length is validated against the *input* but copied into a fixed 16-byte stack buffer.

{{include c/fuzz/parser.c}}

The fuzzer mutates a valid seed (bit flips, random bytes, "interesting" values, byte insertion, chunk duplication), runs the target in a forked child so crashes don't stop the loop, and saves the first input that kills it:

{{include c/fuzz/fuzz_main.c}}

{{run c/fuzz :: gcc -g -O1 -fsanitize=address -fno-omit-frame-pointer parser.c fuzz_main.c -o fuzz && rm -f crash.bin && ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 ./fuzz && od -An -tx1 crash.bin && ASAN_OPTIONS=detect_leaks=0 ./fuzz replay crash.bin 2>&1 | sed 's#/[^ ]*/##' | grep -E 'replaying|ERROR|WRITE of|#1 |SUMMARY' | cut -c1-110}}

In about ten thousand executions (seconds), random mutation grew a payload past 16 bytes and AddressSanitizer reported the `stack-buffer-overflow` in `memcpy` called from `parse_records` at the vulnerable line. `crash.bin` is the **reproducer**: replaying it under ASan gives a deterministic report ready to debug and to keep as a regression test. Without ASan the same input would corrupt the stack silently and might not even crash, which is why fuzzing is always run with sanitizers.

Even this naive strategy works because the target is small. Real targets have deep structure (magic numbers, checksums, nested lengths) that blind mutation rarely satisfies. **Coverage-guided fuzzers** add feedback:

1. Compile the target with instrumentation that records which basic blocks/edges each run executes (`-fsanitize=fuzzer`, `afl-clang-fast`, GCC `-fsanitize-coverage=trace-pc-guard`).
2. Keep any mutated input that reaches **new coverage** in the *corpus*; mutate those preferentially.
3. Repeat at 1,000s–100,000s of executions per second per core, using in-process execution (libFuzzer, honggfuzz) or a fork server (AFL++).
4. Minimize crashes (`-minimize_crash=1`, `afl-tmin`) and the corpus (`-merge=1`, `afl-cmin`).

That feedback loop lets the fuzzer discover `if (magic == 0xCAFEBABE)` branches step by step, solve checksums with comparison logging ("cmplog"), and uses **dictionaries** of tokens (`-dict=png.dict`).

### The libFuzzer harness (what you write for real projects)

```c
#include <stddef.h>
#include <stdint.h>
int parse_records(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {   /* called with each generated input */
    parse_records(data, size);
    return 0;                                                       /* must not leak or keep global state between calls */
}
```

```bash
clang -g -O1 -fsanitize=fuzzer,address,undefined parser.c harness.c -o fuzz_parser
mkdir corpus && echo -n 'R\x04abcdE' > corpus/seed1
./fuzz_parser corpus/ -max_total_time=300 -jobs=8 -dict=records.dict   # crashes saved as crash-<hash>
./fuzz_parser crash-3f2a...                                             # reproduce
```

(Clang isn't installed in the authoring environment, so this harness is shown, not run; AFL++ offers the same via `afl-clang-fast` or GCC plugin mode.) Harness design rules: exercise one entry point per fuzz target, make it deterministic and fast, reset state (no globals), bound memory/time, avoid `exit()`/`abort()` on normal errors, and initialize resources once. Add seeds from real files; use structure-aware mutators or grammar-based fuzzing (libprotobuf-mutator) for complex formats.

### Practical fuzzing

- Fuzz the **boundaries** of your system: every parser, deserializer, command interpreter, and protocol handler. Integrate into CI as a short "regression corpus replay" plus long-running jobs (ClusterFuzzLite, OSS-Fuzz for open source).
- Always run with ASan+UBSan (and sometimes MSan); the sanitizer is the bug oracle.
- Treat timeouts and OOMs as bugs (`-timeout=`, `-rss_limit_mb`).
- Stateful protocols: fuzz a sequence/model of operations; differential fuzzing compares two implementations; fuzz round-trips (`decode(encode(x))`).
- After fixing, add the crashing input to the corpus/tests.
- Related: **property testing**, **symbolic execution** (KLEE), **concolic** tools, **mutation testing** (Mull, `mutate_cpp`) to measure test strength, **fault injection** and **chaos testing** at system level.

## A quality pipeline for C

```text
pre-commit:  clang-format, compiler warnings -Werror, clang-tidy (changed files)
CI (every PR): build matrix (gcc/clang × Debug/Release × OS), unit tests, ASan+UBSan run, TSan run (threaded code),
               gcc -fanalyzer / scan-build / Cppcheck, coverage report with thresholds on new code
nightly:     long fuzz runs with corpus growth, Valgrind, Coverity/CodeQL, performance regression benchmarks
release:     reproducible hardened build, checksec audit, SBOM, signature
```

## Key takeaways

- Layer defenses: strict compiler warnings and `-fanalyzer` for static bugs; unit tests with branch coverage for behavior (line coverage hid an untested `n < 0` branch); fuzzing with sanitizers for hostile input.
- A fuzzer is just: mutate → run in isolation → detect crash → save the reproducer; coverage feedback makes it scale to deep formats.
- Structure code for testability (seams, pure functions, injectable failures) and verify error paths and cleanup.
- Fuzz every parser of untrusted data, keep crashers as regression tests, and automate everything in CI.
