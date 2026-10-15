---
title: Watching Virtual Memory Work
track: linux
module: Memory
moduleOrder: 4
order: 1
level: advanced
minutes: 40
env: machine
summary: "Five exercises on the memory module using a C program that reads /proc: demand paging (virtual size versus resident size), file-backed pages in the page cache, address-space limits with ulimit, overcommit and the OOM score, and reading /proc/meminfo correctly, with captured output."
prerequisites:
  - "Virtual Memory, Paging and the Page Cache"
  - "mmap, Overcommit, the OOM Killer, Huge Pages, NUMA and Swap"
sources:
  - label: "man 5 proc (/proc/PID/status, /proc/meminfo), man 2 mmap, man 2 setrlimit"
    url: https://man7.org/linux/man-pages/man5/proc.5.html
  - label: "Linux kernel documentation — Memory overcommit accounting and the vm sysctls"
    url: https://docs.kernel.org/mm/overcommit-accounting.html
  - label: "Drepper — What Every Programmer Should Know About Memory"
    url: https://people.freebsd.org/~lstewart/articles/cpumemory.pdf
---

Virtual memory is invisible by design, which makes it easy to misunderstand. The C program below makes the invisible visible by printing a process's **virtual size** (`VmSize`: the address space it has reserved) and **resident size** (`VmRSS`: the physical RAM it is actually using) at each step. Read the numbers before you read the explanations.

## The experiment

```c
/* Watch virtual memory work: reserved versus resident, anonymous versus file-backed pages. Linux only. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static long status_mib(const char *key) {                               /* VmSize or VmRSS from /proc/self/status, in MiB */
    FILE *f = fopen("/proc/self/status", "r"); char line[256]; long kb = -1; size_t n = strlen(key);
    while (f && fgets(line, sizeof line, f)) if (!strncmp(line, key, n)) { kb = atol(line + n + 1); break; }
    if (f) fclose(f);
    return kb / 1024;
}
static void show(const char *label) { printf("%-44s VmSize %5ld MiB   VmRSS %4ld MiB\n", label, status_mib("VmSize"), status_mib("VmRSS")); }

int main(void) {
    const size_t MiB = 1024 * 1024, page = (size_t)sysconf(_SC_PAGESIZE);
    show("1. at start");
    char *m = mmap(NULL, 512 * MiB, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);       /* ask for 512 MiB of anonymous memory */
    if (m == MAP_FAILED) { perror("mmap"); return 1; }
    show("2. after mmap of 512 MiB (not touched)");
    for (size_t off = 0; off < 128 * MiB; off += page) m[off] = 1;                                       /* touch 128 MiB, one byte per page */
    show("3. after touching 128 MiB of it");
    for (size_t off = 128 * MiB; off < 512 * MiB; off += page) m[off] = 1;
    show("4. after touching all 512 MiB");
    munmap(m, 512 * MiB);
    show("5. after munmap");

    /* file-backed mapping: pages come from the page cache, shared between processes */
    char path[] = "/tmp/pr_mem_demo.XXXXXX"; int fd = mkstemp(path);
    char *chunk = malloc(MiB); memset(chunk, 0xA5, MiB);
    for (int i = 0; i < 64; i++) if (write(fd, chunk, MiB) != (ssize_t)MiB) { perror("write"); return 1; }
    free(chunk);
    char *fm = mmap(NULL, 64 * MiB, PROT_READ, MAP_PRIVATE, fd, 0);
    show("6. file mapped (64 MiB), not read yet");
    volatile unsigned long sum = 0;
    for (size_t off = 0; off < 64 * MiB; off += page) sum += (unsigned char)fm[off];
    show("7. after reading one byte per page");
    munmap(fm, 64 * MiB); close(fd); unlink(path);
    printf("page size: %zu bytes\n", page);
    return 0;
}
```

Compile it with `cc -o vmem vmem.c` and run `./vmem` on a Linux machine with at least 1 GiB of free memory (the in-browser Linux has only 128 MiB, so the output below was captured in the same virtual machine given 1 GiB, with the program as the only thing running).

```text
$ ./vmem
1. at start                                  VmSize     0 MiB   VmRSS    0 MiB
2. after mmap of 512 MiB (not touched)       VmSize   512 MiB   VmRSS    0 MiB
3. after touching 128 MiB of it              VmSize   512 MiB   VmRSS  128 MiB
4. after touching all 512 MiB                VmSize   512 MiB   VmRSS  512 MiB
5. after munmap                              VmSize     0 MiB   VmRSS    0 MiB
6. file mapped (64 MiB), not read yet        VmSize    64 MiB   VmRSS    0 MiB
7. after reading one byte per page           VmSize    64 MiB   VmRSS   64 MiB
page size: 4096 bytes
```

(Your absolute numbers will differ slightly; the pattern is what matters.)

## Exercises

**1. Reserved versus used.** Between steps 1 and 2 the program asks for 512 MiB. By how much did `VmSize` and `VmRSS` change, and why is that not a contradiction? What do steps 3 and 4 show, and what happens at step 5?

**2. Why one byte per page?** The loops write one byte every `4096` bytes. Why is that enough to make the resident size grow by the full amount, and what is the kernel doing at each of those writes?

**3. Anonymous versus file-backed.** Steps 6 and 7 map a 64 MiB *file* and read one byte per page. What does `VmRSS` do, where do those pages physically live, and what difference does that make to a second process that maps the same file?

**4. Limits and overcommit.** Run `./bigalloc` (it asks for 600 MiB of address space; the source is in the *Exercise 4 program* listing below), then `(ulimit -v 400000; ./bigalloc)` and the two `cat` commands below. What does the limit do, what does `vm.overcommit_memory` control, and what happens when physical memory really runs out?

```c
/* Ask for 600 MiB of address space. Run it normally, then under a limit:  (ulimit -v 400000; ./bigalloc) */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    size_t n = 600u * 1024 * 1024;
    char *p = malloc(n);
    if (!p) { printf("malloc of 600 MiB failed: %s\n", strerror(errno)); return 1; }
    memset(p, 1, 4096);                     /* touch one page only: the rest stays virtual */
    printf("allocated 600 MiB of address space\n");
    free(p);
    return 0;
}
```

On a real Linux machine the limited run prints `malloc of 600 MiB failed: Cannot allocate memory`, and the unlimited run prints `allocated 600 MiB of address space`. (Not captured here: the in-browser Linux does not enforce address-space limits, so both runs print "allocated" there.) The two sysctl values below were captured on a real machine.

```text
$ cat /proc/sys/vm/overcommit_memory /proc/sys/vm/swappiness
0
60
```

**5. Read /proc/meminfo.** Which line tells you how much memory a new program can actually use: `MemFree` or `MemAvailable`? Why is a "full" `Cached` figure not a problem?

```text
$ grep -E "^(MemTotal|MemAvailable|Cached|SwapTotal|Dirty|HugePages_Total|Hugepagesize)" /proc/meminfo
MemTotal: 20486360 kB
MemAvailable: 9512508 kB
Cached: 8131732 kB
SwapTotal: 363516 kB
Dirty: 15828 kB
HugePages_Total: 0
Hugepagesize: 2048 kB
```

## Answers

Try before opening.

<details>
<summary>Show the explanations</summary>

**1. Reserved versus used.** After the 512 MiB `mmap`, **`VmSize` jumped by about 512 MiB (from 0 to 512) while `VmRSS` stayed at 0 MiB.** `mmap` only reserves a range of **virtual addresses**; the kernel records "this range is anonymous memory, zero-filled on demand" but allocates **no physical page** yet. Writing to 128 MiB (step 3) raised `VmRSS` by about 128 MiB (0 to 128), and touching everything (step 4) took it to 512: **physical memory is assigned page by page, on first touch**. At step 5 `munmap` removes the mapping and both numbers fall back. This is **demand paging**, and it is why a process can have a large virtual size and a small footprint, why `VmSize` is a poor measure of memory use, and why `malloc` of a huge block usually "succeeds" immediately.

**2. Why one byte per page?** Memory is managed in **pages** (4,096 bytes here, the last line of the output). The first write to any byte of an untouched page causes a **page fault**: the CPU traps into the kernel, which finds a free physical frame, fills it with zeros, maps it into the process and resumes the instruction, all invisibly. One write per page therefore faults every page exactly once, and a page is either resident or not, so 131,072 faults of 4 KiB give 512 MiB. (It also shows the cost: each first touch is a few microseconds; programs that allocate and use memory in a burst pay this as a start-up delay, and tools such as `MAP_POPULATE` or transparent huge pages reduce it.)

**3. Anonymous versus file-backed.** Mapping the file raised `VmSize` by 64 MiB but not `VmRSS`; reading one byte per page raised `VmRSS` by about 64 MiB (0 to 64). The difference from anonymous memory is the **backing store**: these pages are copies of the file's blocks held in the **page cache**, the kernel's shared cache of file contents, and they are *not* private. A second process that maps the same file gets the **same physical pages** mapped into its own address space, with no extra RAM, and clean (unmodified) pages can be dropped at any time and re-read from disk when needed; anonymous memory can only be reclaimed by writing it to swap. That sharing is how shared libraries and executables stay cheap: dozens of processes use one cached copy of `libc`.

**4. Limits and overcommit.** `ulimit -v 400000` caps the address space at about 390 MiB, so the 600 MiB request fails immediately (`malloc` returns `NULL` with `ENOMEM`, "Cannot allocate memory", because its `mmap` was refused). This is a *virtual* limit set per process, good for containing runaway programs. **`vm.overcommit_memory = 0`** means the default **heuristic overcommit**: the kernel grants a request unless it is obviously larger than the whole system could ever provide, because most programs never touch everything they reserve. Value `1` always allows, and `2` enforces a strict commit limit (`CommitLimit` in `/proc/meminfo`) for systems that must never kill anything. Overcommit is a bet: if processes then touch more pages than there is RAM plus swap, the kernel cannot honour them, and the **OOM killer** chooses a victim by its **oom score** (roughly the process's share of memory, adjusted by `/proc/PID/oom_score_adj`, from −1000 "never kill" to +1000 "kill first") and terminates it with `SIGKILL`; the kernel log shows "Out of memory: Killed process". `swappiness` (60) sets how willingly the kernel swaps out anonymous pages versus dropping file cache.

**5. Read /proc/meminfo.** **`MemAvailable`** (about 9.5 GB here) is the kernel's estimate of what a new program can use without swapping, because it counts free memory *plus* the part of the page cache and reclaimable slabs that can be dropped. **`MemFree`** is only the memory doing literally nothing, and is normally small on a healthy machine: Linux deliberately uses spare RAM as cache. So a large `Cached` (8 GB here) is good news, not a leak: it makes file access fast and is given back on demand. Watch instead for `MemAvailable` falling towards zero, heavy swap activity (`vmstat 1` columns `si` and `so`), and a large `Dirty` value that is not draining (data waiting to be written to disk). `HugePages_Total: 0` means no static huge pages are reserved (the default size is 2 MiB); transparent huge pages are configured separately.

</details>

## Key takeaways

1. `mmap` and `malloc` reserve virtual address space; physical pages are assigned on first touch, so `VmSize` says little and `VmRSS` is the real footprint.
2. Each first touch of a page is a page fault serviced by the kernel (4 KiB at a time); page size explains the one-byte-per-page trick.
3. File-backed pages come from the shared page cache and can be dropped and re-read; anonymous pages can only go to swap.
4. Overcommit (`vm.overcommit_memory`) lets the kernel grant more than it has; when the bet fails, the OOM killer uses `oom_score` to pick a victim.
5. Judge memory with `MemAvailable`, not `MemFree`; a big page cache is healthy.
