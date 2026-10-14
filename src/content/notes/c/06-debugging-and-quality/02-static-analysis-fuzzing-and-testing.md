---
title: Static Analysis, Fuzzing and Testing C Code
track: c
module: Debugging and Quality
moduleOrder: 6
order: 2
level: advanced
summary: Finding bugs before users do — compiler warnings and GCC's static analyzer, a unit-test harness with line and branch coverage, and coverage-guided fuzzing explained through a working mutation fuzzer that finds a stack overflow in seconds.
sources:
  - label: "GCC manual — Static Analyzer Options (-fanalyzer); Instrumentation Options (--coverage); gcov"
    url: https://gcc.gnu.org/onlinedocs/gcc/Static-Analyzer-Options.html
  - label: "LLVM libFuzzer documentation; AFL++ documentation"
    url: https://llvm.org/docs/LibFuzzer.html
  - label: "Clang Static Analyzer and clang-tidy checks; Cppcheck; CodeQL for C"
    url: https://clang.llvm.org/docs/analyzer/checkers.html
  - label: "Böhme, Pham, Roychoudhury — Coverage-based Greybox Fuzzing as Markov Chain (CCS 2016)"
  - label: "OSS-Fuzz — continuous fuzzing for open source"
    url: https://google.github.io/oss-fuzz/
  - label: "Beck — Test Driven Development; Google Testing Blog; Kerr — Unity / cmocka / Check frameworks"
    url: https://cmocka.org/
---

No single technique finds all bugs. They complement each other: **static analysis** reasons about code without running it (finds shallow bugs on all paths), **tests** check intended behavior on chosen inputs, **fuzzing** explores unintended inputs automatically, and **sanitizers** make hidden corruption visible while the other two run.

## Static analysis

### The compiler is the first analyzer

`-Wall -Wextra` plus the extra warnings from the build note catch uninitialized variables, format mismatches, sign-compare, shadowing, missing returns, unused results and many API misuses; `-O2` enables more dataflow warnings (some only appear with optimization). **Treat warnings as defects**.

### `gcc -fanalyzer`

GCC 10+ includes an interprocedural analyzer that symbolically executes paths, finding double-free, use-after-free, leaks (memory, files), null dereference, uninitialized values, buffer overflows with known bounds, tainted-data uses. A file with four bugs:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
char *make_copy(const char *s) {
    char *p = malloc(strlen(s) + 1);
    strcpy(p, s);                         /* p may be NULL here */
    return p;
}
void use_after_free(void) {
    char *q = malloc(10);
    if (!q) return;
    free(q);
    q[0] = 'x';
}
int leak(int flag) {
    FILE *f = fopen("/tmp/x", "r");
    if (flag) return -1;                  /* f leaked on this path */
    fclose(f);
    return 0;
}
```

```text
$ gcc -fanalyzer -O0 -c analyze.c -o /dev/null 2>&1 | grep -E 'warning:' | sed 's/analyze.c://' | cut -c1-120
6:5: warning: use of possibly-NULL ‘p’ where non-null expected [CWE-690] [-Wanalyzer-possible-null-argument]
13:10: warning: use after ‘free’ of ‘q’ [CWE-416] [-Wanalyzer-use-after-free]
17:22: warning: leak of FILE ‘f’ [CWE-775] [-Wanalyzer-file-leak]
17:22: warning: leak of ‘f’ [CWE-401] [-Wanalyzer-malloc-leak]
18:5: warning: use of possibly-NULL ‘f’ where non-null expected [CWE-690] [-Wanalyzer-possible-null-argument]
```

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

```c
#ifndef MATHUTIL_H
#define MATHUTIL_H
int clamp(int v, int lo, int hi);
int safe_add(int a, int b, int *out);
int isqrt(int n);
#endif
```

```c
#include <limits.h>
#include "mathutil.h"

int clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Returns 0 and stores a+b, or -1 on overflow. */
int safe_add(int a, int b, int *out) {
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) return -1;
    *out = a + b;
    return 0;
}

/* Integer square root by binary search; untested branch on purpose. */
int isqrt(int n) {
    if (n < 0) return -1;
    if (n < 2) return n;
    int lo = 1, hi = n < 46341 ? n : 46341;
    while (lo < hi) {
        int mid = lo + (hi - lo + 1) / 2;
        if ((long long)mid * mid <= n) lo = mid; else hi = mid - 1;
    }
    return lo;
}
```

```c
#include <limits.h>
#include <stdio.h>
#include "mathutil.h"

/* A minimal xUnit-style harness: no dependencies, one process, clear output. */
static int failures, checks;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { long long _a = (a), _b = (b); checks++; if (_a != _b) { failures++; printf("FAIL %s:%d: %s == %s (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)

static void test_clamp(void) {
    CHECK_EQ(clamp(5, 0, 10), 5);
    CHECK_EQ(clamp(-3, 0, 10), 0);
    CHECK_EQ(clamp(99, 0, 10), 10);
    CHECK_EQ(clamp(0, 0, 0), 0);
}

static void test_safe_add(void) {
    int r = 0;
    CHECK_EQ(safe_add(2, 3, &r), 0);  CHECK_EQ(r, 5);
    CHECK_EQ(safe_add(INT_MAX, 1, &r), -1);
    CHECK_EQ(safe_add(INT_MIN, -1, &r), -1);
    CHECK_EQ(safe_add(INT_MAX, 0, &r), 0);
}

static void test_isqrt(void) {
    CHECK_EQ(isqrt(0), 0);
    CHECK_EQ(isqrt(15), 3);
    CHECK_EQ(isqrt(16), 4);
    /* no test for negative input or INT_MAX: coverage will show the gap */
}

int main(void) {
    test_clamp(); test_safe_add(); test_isqrt();
    printf("%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
```

### Coverage with gcov

`--coverage` (= `-fprofile-arcs -ftest-coverage`) instruments the build; running the tests writes `.gcda` data and `gcov` annotates the source. Compile each file separately so the data files are named after the sources:

```text
$ gcc -std=c11 -Wall -Wextra --coverage -O0 -c mathutil.c && gcc -std=c11 -Wall -Wextra --coverage -O0 -c test_mathutil.c && gcc --coverage mathutil.o test_mathutil.o -o test_mathutil && ./test_mathutil && gcov -b mathutil.c 2>&1 | sed -n 2,5p; sed -n '/int isqrt/,/ 21:/p' mathutil.c.gcov | cut -c1-72; rm -f *.gcov *.gc* *.o test_mathutil
12 checks, 0 failures
Lines executed:100.00% of 16
Branches executed:100.00% of 20
Taken at least once:90.00% of 20
No calls
        3:   18:int isqrt(int n) {
       3*:   19:    if (n < 0) return -1;
branch  0 taken 0% (fallthrough)
branch  1 taken 100%
        3:   20:    if (n < 2) return n;
branch  0 taken 33% (fallthrough)
branch  1 taken 67%
        2:   21:    int lo = 1, hi = n < 46341 ? n : 46341;
```

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

```c
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A toy record format:  'R' <len:1> <payload:len>  ...  repeated; 'E' ends.
   It has a deliberate bug: the copy into the 16-byte stack buffer is bounded by len, not by the buffer. */
int parse_records(const uint8_t *data, size_t size) {
    size_t pos = 0;
    int records = 0;
    while (pos < size) {
        uint8_t tag = data[pos++];
        if (tag == 'E') break;
        if (tag != 'R') return -1;
        if (pos >= size) return -1;
        uint8_t len = data[pos++];
        if (len > size - pos) return -1;                /* length is validated against the INPUT size... */
        char buf[16];
        memcpy(buf, data + pos, len);                    /* ...but not against the 16-byte destination */
        buf[len < sizeof buf ? len : sizeof buf - 1] = 0;
        pos += len;
        records++;
    }
    return records;
}
```

The fuzzer mutates a valid seed (bit flips, random bytes, "interesting" values, byte insertion, chunk duplication), runs the target in a forked child so crashes don't stop the loop, and saves the first input that kills it:

```c
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int parse_records(const uint8_t *data, size_t size);

/* xorshift64: tiny deterministic PRNG so the run is reproducible */
static uint64_t rng = 88172645463325252ULL;
static uint64_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

#define MAXLEN 128

/* Apply 1-4 random mutations: bit flips, interesting values, insertions and chunk duplication. */
static size_t mutate(uint8_t *buf, size_t n) {
    int muts = 1 + (int)(rnd() % 4);
    for (int m = 0; m < muts; m++) {
        size_t i = (size_t)(rnd() % n);
        switch (rnd() % 5) {
        case 0: buf[i] ^= (uint8_t)(1u << (rnd() % 8)); break;
        case 1: buf[i] = (uint8_t)rnd(); break;
        case 2: buf[i] = (uint8_t[]){0, 1, 0x7f, 0x80, 0xff, 16, 17, 32}[rnd() % 8]; break;
        case 3:                                                       /* insert a random byte */
            if (n < MAXLEN) { memmove(buf + i + 1, buf + i, n - i); buf[i] = (uint8_t)rnd(); n++; }
            break;
        case 4: {                                                     /* duplicate a chunk */
            size_t len = 1 + (size_t)(rnd() % 8);
            if (i + len <= n && n + len <= MAXLEN) { memmove(buf + i + len, buf + i, n - i); n += len; }
            break;
        }
        }
    }
    return n;
}

static int run_one(const uint8_t *buf, size_t n) {       /* run the target in a child so a crash doesn't stop the fuzzer */
    pid_t pid = fork();
    if (pid == 0) { parse_records(buf, n); _exit(0); }
    int st; waitpid(pid, &st, 0);
    return WIFSIGNALED(st) || (WIFEXITED(st) && WEXITSTATUS(st) != 0);
}

int main(int argc, char **argv) {
    if (argc > 2 && !strcmp(argv[1], "replay")) {            /* ./fuzz replay crash.bin */
        uint8_t buf[MAXLEN];
        FILE *f = fopen(argv[2], "rb");
        size_t n = f ? fread(buf, 1, sizeof buf, f) : 0;
        printf("replaying %zu bytes\n", n);
        printf("result=%d\n", parse_records(buf, n));
        return 0;
    }
    const uint8_t seed[] = { 'R', 4, 'a', 'b', 'c', 'd', 'R', 2, 'x', 'y', 'E' };   /* a valid seed input */
    uint8_t cur[MAXLEN];
    for (unsigned long iters = 1; iters <= 300000; iters++) {
        size_t n = sizeof seed;
        memcpy(cur, seed, n);
        n = mutate(cur, n);
        if (run_one(cur, n)) {
            FILE *f = fopen("crash.bin", "wb"); fwrite(cur, 1, n, f); fclose(f);
            printf("crash after %lu inputs; %zu-byte reproducer saved to crash.bin\n", iters, n);
            return 0;
        }
    }
    puts("no crash found");
    return 0;
}
```

```text
$ gcc -g -O1 -fsanitize=address -fno-omit-frame-pointer parser.c fuzz_main.c -o fuzz && rm -f crash.bin && ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 ./fuzz && od -An -tx1 crash.bin && ASAN_OPTIONS=detect_leaks=0 ./fuzz replay crash.bin 2>&1 | sed 's#/[^ ]*/##' | grep -E 'replaying|ERROR|WRITE of|#1 |SUMMARY' | cut -c1-110
crash after 9069 inputs; 22-byte reproducer saved to crash.bin
 52 12 04 61 62 63 64 52 12 04 61 62 63 64 52 02
 7a 79 02 78 79 45
==2437275==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7fe1d2300030 at pc 0x7fe1d46f27ef bp 0x
WRITE of size 18 at 0x7fe1d2300030 thread T0
    #1 0x55a8e2039453 in parse_records parser.c:19
SUMMARY: AddressSanitizer: stack-buffer-overflow ..sanitizer_common_interceptors_memintrinsics.inc:115 in memc
=================================================================
==2437273==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7f2731b00030 at pc 0x7f2733ef27ef bp 0x7ffd823509c0 sp 0x7ffd82350180
WRITE of size 18 at 0x7f2731b00030 thread T0
    #0 0x7f2733ef27ee in memcpy ../../../../src/libsanitizer/sanitizer_common/sanitizer_common_interceptors_memintrinsics.inc:115
    #1 0x55f962e64453 in parse_records authoring/examples/c/fuzz/parser.c:19
    #2 0x55f962e64c9e in run_one authoring/examples/c/fuzz/fuzz_main.c:41
    #3 0x55f962e64c9e in main authoring/examples/c/fuzz/fuzz_main.c:61
    #4 0x7f2733c35ca7  (/lib/x86_64-linux-gnu/libc.so.6+0x29ca7) (BuildId: c495b62edadd6c356265942ec1282d98058a7b41)
    #5 0x7f2733c35d64 in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29d64) (BuildId: c495b62edadd6c356265942ec1282d98058a7b41)
    #6 0x55f962e641f0 in _start (authoring/examples/c/fuzz/fuzz+0x21f0) (BuildId: aa75eed696d97dafa916c033cea2eb5dbc790213)

Address 0x7f2731b00030 is located in stack of thread T0 at offset 48 in frame
    #0 0x55f962e642c8 in parse_records authoring/examples/c/fuzz/parser.c:8

  This frame has 1 object(s):
    [32, 48) 'buf' (line 18) <== Memory access at offset 48 overflows this variable
HINT: this may be a false positive if your program uses some custom stack unwind mechanism, swapcontext or vfork
      (longjmp and C++ exceptions *are* supported)
SUMMARY: AddressSanitizer: stack-buffer-overflow ../../../../src/libsanitizer/sanitizer_common/sanitizer_common_interceptors_memintrinsics.inc:115 in memcpy
Shadow bytes around the buggy address:
  0x7f2731affd80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x7f2731affe00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x7f2731affe80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x7f2731afff00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x7f2731afff80: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x7f2731b00000: f1 f1 f1 f1 00 00[f3]f3 00 00 00 00 00 00 00 00
  0x7f2731b00080: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x7f2731b00100: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x7f2731b00180: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x7f2731b00200: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x7f2731b00280: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07 
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==2437273==ABORTING
```

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
