---
title: Debugging with gdb and the Sanitizers
track: c
module: Debugging and Quality
moduleOrder: 6
order: 1
level: intermediate
summary: A practical debugging workflow — compile for debuggability, read ASan/UBSan/TSan/LSan reports, use gdb (breakpoints, watchpoints, core dumps, reverse debugging) and complementary tools to turn crashes and corruption into root causes.
sources:
  - label: "GDB manual (Debugging with GDB)"
    url: https://sourceware.org/gdb/current/onlinedocs/gdb/
  - label: "AddressSanitizer, ThreadSanitizer, LeakSanitizer, UBSan documentation (Clang/GCC)"
    url: https://github.com/google/sanitizers/wiki
  - label: "man 5 core, man 1 addr2line, man 1 valgrind (memcheck, helgrind)"
    url: https://man7.org/linux/man-pages/man5/core.5.html
  - label: "rr — record and replay debugger"
    url: https://rr-project.org/
  - label: "Zeller — Why Programs Fail (A Guide to Systematic Debugging, 2nd ed.)"
  - label: "Agans — Debugging - The 9 Indispensable Rules"
---

Debugging is the scientific method applied to code: observe a failure, hypothesize a cause, test with the cheapest experiment, and repeat. In C the hard part is that bugs (memory corruption, data races, UB) often manifest far from their cause; the tools below move detection to the moment of the *first wrong action*.

## 1. Make the failure reproducible and small

1. Capture the exact input, command line, environment, seed, and build. Make a one-command reproduction.
2. Reduce: delete code and input until the failure remains (`creduce`, bisect the input, `git bisect run ./test.sh` to find the offending commit).
3. Prefer deterministic reproduction: fix random seeds, disable ASLR when debugging address-dependent bugs (`setarch -R ./prog`, or in gdb, which disables it by default), run single-threaded, pin to one CPU (`taskset -c 0`).
4. Record the symptom precisely (stack trace, error message, exit status) before changing anything.

## 2. Build for debugging

```bash
gcc -std=c11 -g3 -O0 -fno-omit-frame-pointer -Wall -Wextra -fsanitize=address,undefined prog.c -o prog
```

- `-g` (or `-g3` for macro info) emits DWARF debug info mapping machine code to lines, variables and types; it does not change generated code. `-O0` (or `-Og`) keeps variables alive and statements in order; at `-O2` variables are "optimized out" and lines jump around.
- If the bug appears only when optimized, debug with `-O2 -g` (expect imprecision), use sanitizers (they work with optimization), and suspect undefined behavior.
- Keep `.debug` files for release builds: `objcopy --only-keep-debug prog prog.debug; strip prog; objcopy --add-gnu-debuglink=prog.debug prog`, or fetch symbols with `debuginfod`.

## 3. The sanitizers: find the first bad access

### AddressSanitizer (memory errors)

Instruments loads and stores, surrounding objects with poisoned redzones; reports out-of-bounds (heap, stack, global), use-after-free, use-after-return, double/invalid free, with **allocation/free stacks**. A stack buffer overflow (`asan_stack.c`):

```c
#include <stdio.h>
#include <string.h>

int global_table[4] = {1, 2, 3, 4};

static int sum_table(int n) {           /* off-by-one on a global: reads global_table[4] */
    int s = 0;
    for (int i = 0; i <= n; i++) s += global_table[i];
    return s;
}

static void fill(const char *src) {
    char name[8];
    char tag[8] = "TAG";
    strcpy(name, src);                  /* stack buffer overflow when src is longer than 7 chars */
    printf("name=%s tag=%s\n", name, tag);
}

int main(int argc, char **argv) {
    if (argc > 1 && argv[1][0] == 'g') printf("sum=%d\n", sum_table(4));
    else fill(argc > 1 ? argv[1] : "ok");
    return 0;
}
```

```text
$ gcc -g -O0 -fsanitize=address -fno-omit-frame-pointer asan_stack.c -o asan_stack && ./asan_stack LongNameTooBig 2>&1 | sed -e 's#/[^ ]*/##' | grep -E 'ERROR|WRITE|#[0-2] |located|This frame|<==|\[[0-9]+, [0-9]+\)|SUMMARY' | cut -c1-120
==2428122==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7fc6b5800028 at pc 0x7fc6b7aa2681 bp 0x7ffeac71c7
WRITE of size 15 at 0x7fc6b5800028 thread T0
    #0 0x7fc6b7aa2680 in strcpy ..asan_interceptors.cpp:563
    #1 0x56229652f32e in fill asan_stack.c:15
    #2 0x56229652f48e in main asan_stack.c:21
Address 0x7fc6b5800028 is located in stack of thread T0 at offset 40 in frame
    #0 0x56229652f278 in fill asan_stack.c:12
  This frame has 2 object(s):
    [32, 40) 'name' (line 13) <== Memory access at offset 40 overflows this variable
    [64, 72) 'tag' (line 14)
SUMMARY: AddressSanitizer: stack-buffer-overflow ..asan_interceptors.cpp:563 in strcpy
```

Read the report from the top: the error kind (`stack-buffer-overflow`), the access (`WRITE of size 15`), the call stack that did it (`#1 fill asan_stack.c:15`, the `strcpy`), then **which variable was overrun**: it printed the frame layout, `[32, 40) 'name'` flagged with `<== Memory access at offset 40 overflows this variable`. Global overflows get the same treatment:

```text
$ ./asan_stack g 2>&1 | sed -e 's#/[^ ]*/##' | grep -E 'ERROR|READ|is located|SUMMARY' | cut -c1-140
==2428127==ERROR: AddressSanitizer: global-buffer-overflow on address 0x55f54bba9090 at pc 0x55f54bba623e bp 0x7ffc2a639540 sp 0x7ffc2a63953
READ of size 4 at 0x55f54bba9090 thread T0
0x55f54bba9090 is located 0 bytes after global variable 'global_table' defined in 'asan_stack.c:4:5' (0x55f54bba9080) of size 16
SUMMARY: AddressSanitizer: global-buffer-overflow asan_stack.c:8 in sum_table
```

Tips: `ASAN_OPTIONS=detect_stack_use_after_return=1:halt_on_error=0:abort_on_error=1:symbolize=1`; set `ASAN_SYMBOLIZER_PATH` (e.g. `llvm-symbolizer`) for line numbers; ASan intercepts `strcpy`, `memcpy`, `malloc`; it is incompatible with Valgrind and with TSan in the same binary; ~2× CPU, 2–3× memory.

### LeakSanitizer (leaks)

Enabled by default with ASan on Linux; reports at exit the allocation stack of unreachable blocks:

```c
#include <stdlib.h>
#include <string.h>
static char *dup_name(const char *s) { char *p = malloc(strlen(s) + 1); strcpy(p, s); return p; }
int main(void) {
    char *a = dup_name("alpha");
    char *b = dup_name("beta");      /* leaked */
    (void)b;
    free(a);
    return 0;
}
```

```text
$ gcc -g -fsanitize=address leak.c -o leak && ./leak 2>&1 | sed -e 's#/[^ ]*/##' | grep -E 'ERROR|Direct leak|#[0-2] |SUMMARY' | cut -c1-120
==2428137==ERROR: LeakSanitizer: detected memory leaks
Direct leak of 5 byte(s) in 1 object(s) allocated from:
    #0 0x7fe191af4c57 in malloc ..asan_malloc_linux.cpp:69
    #1 0x556ec95661cc in dup_name leak.c:3
    #2 0x556ec9566213 in main leak.c:6
SUMMARY: AddressSanitizer: 5 byte(s) leaked in 1 allocation(s).
```

`b` was allocated in `dup_name` (called from `main` line 6) and never freed: 5 bytes in 1 object. ("Indirect leaks" are blocks reachable only from leaked blocks.) For long-running servers use heap profilers instead.

### UndefinedBehaviorSanitizer

`-fsanitize=undefined` (signed overflow, shifts, null, misaligned, bounds with static sizes, invalid enum/bool, `unreachable`, etc.); add `-fno-sanitize-recover=all` to stop at the first report. Shown in the UB note.

### ThreadSanitizer (data races, lock-order inversions)

```c
static int counter;
static void *inc(void *arg) { for (int i = 0; i < 100000; i++) counter++; return NULL; }   /* two threads run this */
```

```text
$ gcc -g -O1 -fsanitize=thread -pthread race.c -o race && ./race 2>&1 | sed -e 's#/[^ ]*/##' | grep -E 'WARNING|Previous write|SUMMARY|counter=' | cut -c1-120
WARNING: ThreadSanitizer: data race (pid=2428148)
  Previous write of size 4 at 0x55af5c6f305c by thread T1:
SUMMARY: ThreadSanitizer: data race race.c:4 in inc
counter=200000
```

TSan reported the race even though this particular run printed the correct total (`200000`): races are *latent*, and the sanitizer detects the missing happens-before relation, not the wrong result. Fix with atomics or locks. TSan needs all code instrumented (including libraries you built), has ~5–15× overhead.

### MemorySanitizer (Clang only)

Detects reads of uninitialized memory (`-fsanitize=memory -fPIE -pie`); on GCC use Valgrind `memcheck --track-origins=yes`.

## 4. gdb essentials

`gdb` is the standard debugger (LLDB on macOS/LLVM). It needs `-g` symbols. (The authoring environment for these notes has no `gdb`, so the following is a command reference, not captured output.)

```bash
gdb ./prog                      # start;  gdb --args ./prog arg1 arg2;  gdb -p PID (attach);  gdb ./prog core
(gdb) run < input.txt           # or: start (break at main)
(gdb) bt                        # backtrace;  bt full (with locals);  thread apply all bt
(gdb) frame 2 ; up ; down       # select a frame
(gdb) info locals ; info args ; info registers
(gdb) print x ; print *ptr ; print arr[1]@5 ; print/x flags ; ptype struct node ; whatis expr
(gdb) x/16xb ptr ; x/4gx $rsp ; x/s str ; x/5i $pc      # examine memory as bytes/giants/string/instructions
(gdb) break file.c:42 ; break func ; break func if n > 100 ; tbreak ; delete 2 ; disable 3
(gdb) watch counter ; rwatch x ; awatch x ; watch -l *ptr   # hardware watchpoints: stop when memory changes
(gdb) next ; step ; finish ; until ; continue ; stepi ; nexti
(gdb) display var ; set var x = 5 ; call func(3) ; return                # tweak state
(gdb) catch syscall write ; catch signal SIGSEGV ; handle SIGPIPE nostop
(gdb) set pagination off ; set print pretty on ; set disassembly-flavor intel ; layout src (TUI) ; tui enable
(gdb) set follow-fork-mode child ; set detach-on-fork off ; info threads ; thread 3
(gdb) disassemble /s main ; info symbol 0x401136 ; info line *0x401136 ; list func
```

Workflow for a segfault: `gdb ./prog core` (or run under gdb) → `bt` shows the faulting frame → `frame N`, `info locals`, `print *ptr` to find the bad pointer → find where it was set (use a **watchpoint** on the variable and rerun, or `rr` to go backwards).

Power features:

- **Conditional breakpoints and commands**: `break parse if len > 4096` then `commands ... silent; bt 3; continue; end` automates logging.
- **Hardware watchpoints** (a few per CPU) stop at the exact instruction that corrupts a variable: the fastest way to catch a buffer overflow or a stray write: `watch -l buf[0]` after the buffer is allocated.
- **Pretty printers and Python scripting** (`python print(gdb.parse_and_eval("x"))`); `.gdbinit` for settings; `source script.gdb`.
- **Remote debugging**: `gdbserver :1234 ./prog` on the target, `target remote host:1234`; embedded cores via OpenOCD.
- **Core dumps**: `ulimit -c unlimited`; the pattern is in `/proc/sys/kernel/core_pattern` (`core`, `|/usr/lib/systemd/systemd-coredump ...` → `coredumpctl list`, `coredumpctl gdb`). Always keep the exact binary and libraries (build-id) that produced the dump. Without gdb, resolve addresses with `addr2line -f -e prog 0x1139` or ASan's symbolized stacks.
- **Reverse debugging**: `rr record ./prog` then `rr replay`, `reverse-continue`, `reverse-step`, `watch` + `reverse-continue` to find who wrote a value: deterministic replay of a failing run (needs performance counters; not in many VMs). gdb's built-in `record full` is slower.
- **Multithreaded bugs**: `thread apply all bt`, `set scheduler-locking on`, `info threads`; for deadlocks, attach and list who waits on which mutex (`pthread_mutex_t` owner field).

## 5. Valgrind and other tools

| Tool | Finds | Cost |
|---|---|---|
| `valgrind --tool=memcheck --leak-check=full --track-origins=yes ./prog` | invalid reads/writes, uninitialized use (with origin), leaks, bad frees | ~20× slower; no recompilation |
| `valgrind --tool=helgrind` / `drd` | data races, lock-order errors | slow |
| `valgrind --tool=callgrind` + `kcachegrind` | instruction-level profile, call graph | slow, deterministic |
| `valgrind --tool=massif` | heap usage over time | |
| `strace -f -tt -T`, `ltrace` | syscalls / library calls as they happen | see syscalls note |
| `ldd`, `LD_DEBUG`, `nm`, `objdump -d`, `readelf` | loader and linking problems | |
| `perf`, `gprof`, `gcov` | where time goes / which lines ran | profiling notes |
| `lsof`, `/proc/PID/*`, `ss` | open files, memory maps, sockets | |
| `MALLOC_CHECK_=3`, `MALLOC_PERTURB_=165`, `electric-fence` | glibc heap corruption checks / poisoning | |

## 6. Debugging strategy

1. **Read the evidence first**: the first error line, the last log lines, the exit code (139 = SIGSEGV, 134 = SIGABRT, 137 = SIGKILL/OOM, 124 timeout).
2. **Look for the earliest wrong state**, not the crash site: memory corruption crashes later. Sanitizers and watchpoints move you to the cause.
3. **Bisect**: input, code (`git bisect`), configuration, compiler flags (`-O0` vs `-O2`, `-fno-inline`), library versions.
4. **Binary search with prints/asserts**: add invariant checks (`assert`), print values with `%zu`/`%p`, log to `stderr` (unbuffered), and use `fprintf(stderr, ...)` or `setvbuf` so output survives a crash. Remove the instrumentation afterward.
5. **Change one thing at a time** and keep notes; if you can't explain why a fix works, you haven't found the bug.
6. **Heisenbugs** (disappear when observed): suspect uninitialized variables, data races, timing, stack/heap layout dependence (UB); use TSan/ASan/UBSan/Valgrind, vary `ulimit`, stack size, thread counts, and CPU affinity.
7. **Add a regression test** and, if the class of bug is common, a lint rule or sanitizer CI job.

Rubber-duck it: explaining the code path aloud, line by line, solves a large fraction of bugs.

## Key takeaways

- Compile with `-g -O0` (or `-Og`) plus `-fsanitize=address,undefined` for development; use TSan separately for threads and LSan/Valgrind for leaks.
- Read sanitizer reports from the top: error kind, offending access, allocation/free stacks, and the overrun variable's location in the frame.
- gdb: backtrace, frame/locals, hardware watchpoints for corruption, conditional breakpoints, core dumps with the exact binary; `rr` gives deterministic reverse debugging.
- Debug by shrinking the reproduction, finding the *earliest* wrong state, bisecting changes, and turning every fix into a test.
