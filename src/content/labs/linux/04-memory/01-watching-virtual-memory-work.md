---
title: Watching Virtual Memory Work
track: linux
module: Memory
moduleOrder: 4
order: 1
level: advanced
minutes: 40
summary: "Five exercises on the memory module using a Python script that reads /proc: demand paging (virtual size versus resident size), file-backed pages in the page cache, address-space limits with ulimit, overcommit and the OOM score, and reading /proc/meminfo correctly, with captured output."
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

Virtual memory is invisible by design, which makes it easy to misunderstand. The script below makes the invisible visible by printing a process's **virtual size** (`VmSize`: the address space it has reserved) and **resident size** (`VmRSS`: the physical RAM it is actually using) at each step. Read the numbers before you read the explanations.

## The experiment

```python
import mmap, os, re

def status(key):
    for line in open("/proc/self/status"):
        if line.startswith(key):
            return int(line.split()[1]) // 1024            # kB -> MiB
def show(label):
    print(f"{label:<44} VmSize {status('VmSize'):>5} MiB   VmRSS {status('VmRSS'):>4} MiB")

show("1. at start")
m = mmap.mmap(-1, 512 * 1024 * 1024)                       # ask for 512 MiB of anonymous memory
show("2. after mmap of 512 MiB (not touched)")
page = mmap.PAGESIZE
for off in range(0, 128 * 1024 * 1024, page): m[off] = 1  # touch 128 MiB, one byte per page
show("3. after touching 128 MiB of it")
for off in range(128 * 1024 * 1024, 512 * 1024 * 1024, page): m[off] = 1
show("4. after touching all 512 MiB")
m.close()
show("5. after munmap")

# file-backed mapping: pages come from the page cache, shared between processes
path = "/tmp/pr_mem_demo.bin"
with open(path, "wb") as f: f.write(os.urandom(64 * 1024 * 1024))
with open(path, "rb") as f:
    fm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    show("6. file mapped (64 MiB), not read yet")
    s = 0
    for off in range(0, len(fm), page): s += fm[off]
    show("7. after reading one byte per page")
    fm.close()
os.remove(path)
print("page size:", page, "bytes")
```

```text
$ python3 mem.py
1. at start                                  VmSize    18 MiB   VmRSS   10 MiB
2. after mmap of 512 MiB (not touched)       VmSize   530 MiB   VmRSS   10 MiB
3. after touching 128 MiB of it              VmSize   530 MiB   VmRSS  138 MiB
4. after touching all 512 MiB                VmSize   530 MiB   VmRSS  522 MiB
5. after munmap                              VmSize    18 MiB   VmRSS   10 MiB
6. file mapped (64 MiB), not read yet        VmSize    82 MiB   VmRSS   10 MiB
7. after reading one byte per page           VmSize    82 MiB   VmRSS   74 MiB
page size: 4096 bytes
```

(Your absolute numbers will differ slightly; the pattern is what matters.)

## Exercises

**1. Reserved versus used.** Between steps 1 and 2 the program asks for 512 MiB. By how much did `VmSize` and `VmRSS` change, and why is that not a contradiction? What do steps 3 and 4 show, and what happens at step 5?

**2. Why one byte per page?** The loops write one byte every `4096` bytes. Why is that enough to make the resident size grow by the full amount, and what is the kernel doing at each of those writes?

**3. Anonymous versus file-backed.** Steps 6 and 7 map a 64 MiB *file* and read one byte per page. What does `VmRSS` do, where do those pages physically live, and what difference does that make to a second process that maps the same file?

**4. Limits and overcommit.** Run `(ulimit -v 400000; python3 -c 'b = bytearray(600*1024*1024)')` and the two `cat` commands below. What does the limit do, what does `vm.overcommit_memory` control, and what happens when physical memory really runs out?

```text
$ (ulimit -v 400000; python3 -c "b = bytearray(600*1024*1024); print('allocated')")
MemoryError
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

**1. Reserved versus used.** After the 512 MiB `mmap`, **`VmSize` jumped by about 512 MiB (from 18 to 530) while `VmRSS` stayed at 10 MiB.** `mmap` only reserves a range of **virtual addresses**; the kernel records "this range is anonymous memory, zero-filled on demand" but allocates **no physical page** yet. Writing to 128 MiB (step 3) raised `VmRSS` by about 128 MiB (10 to 138), and touching everything (step 4) took it to 522: **physical memory is assigned page by page, on first touch**. At step 5 `munmap` removes the mapping and both numbers fall back. This is **demand paging**, and it is why a process can have a large virtual size and a small footprint, why `VmSize` is a poor measure of memory use, and why `malloc` of a huge block usually "succeeds" immediately.

**2. Why one byte per page?** Memory is managed in **pages** (4,096 bytes here, the last line of the output). The first write to any byte of an untouched page causes a **page fault**: the CPU traps into the kernel, which finds a free physical frame, fills it with zeros, maps it into the process and resumes the instruction, all invisibly. One write per page therefore faults every page exactly once, and a page is either resident or not, so 131,072 faults of 4 KiB give 512 MiB. (It also shows the cost: each first touch is a few microseconds; programs that allocate and use memory in a burst pay this as a start-up delay, and tools such as `MAP_POPULATE` or transparent huge pages reduce it.)

**3. Anonymous versus file-backed.** Mapping the file raised `VmSize` by 64 MiB but not `VmRSS`; reading one byte per page raised `VmRSS` by about 64 MiB (10 to 74). The difference from anonymous memory is the **backing store**: these pages are copies of the file's blocks held in the **page cache**, the kernel's shared cache of file contents, and they are *not* private. A second process that maps the same file gets the **same physical pages** mapped into its own address space, with no extra RAM, and clean (unmodified) pages can be dropped at any time and re-read from disk when needed; anonymous memory can only be reclaimed by writing it to swap. That sharing is how shared libraries and executables stay cheap: dozens of processes use one cached copy of `libc`.

**4. Limits and overcommit.** `ulimit -v 400000` caps the address space at about 390 MiB, so the 600 MiB request fails immediately with `MemoryError` (the allocator's `mmap` returned an error). This is a *virtual* limit set per process, good for containing runaway programs. **`vm.overcommit_memory = 0`** means the default **heuristic overcommit**: the kernel grants a request unless it is obviously larger than the whole system could ever provide, because most programs never touch everything they reserve. Value `1` always allows, and `2` enforces a strict commit limit (`CommitLimit` in `/proc/meminfo`) for systems that must never kill anything. Overcommit is a bet: if processes then touch more pages than there is RAM plus swap, the kernel cannot honour them, and the **OOM killer** chooses a victim by its **oom score** (roughly the process's share of memory, adjusted by `/proc/PID/oom_score_adj`, from −1000 "never kill" to +1000 "kill first") and terminates it with `SIGKILL`; the kernel log shows "Out of memory: Killed process". `swappiness` (60) sets how willingly the kernel swaps out anonymous pages versus dropping file cache.

**5. Read /proc/meminfo.** **`MemAvailable`** (about 9.5 GB here) is the kernel's estimate of what a new program can use without swapping, because it counts free memory *plus* the part of the page cache and reclaimable slabs that can be dropped. **`MemFree`** is only the memory doing literally nothing, and is normally small on a healthy machine: Linux deliberately uses spare RAM as cache. So a large `Cached` (8 GB here) is good news, not a leak: it makes file access fast and is given back on demand. Watch instead for `MemAvailable` falling towards zero, heavy swap activity (`vmstat 1` columns `si` and `so`), and a large `Dirty` value that is not draining (data waiting to be written to disk). `HugePages_Total: 0` means no static huge pages are reserved (the default size is 2 MiB); transparent huge pages are configured separately.

</details>

## Key takeaways

1. `mmap` and `malloc` reserve virtual address space; physical pages are assigned on first touch, so `VmSize` says little and `VmRSS` is the real footprint.
2. Each first touch of a page is a page fault serviced by the kernel (4 KiB at a time); page size explains the one-byte-per-page trick.
3. File-backed pages come from the shared page cache and can be dropped and re-read; anonymous pages can only go to swap.
4. Overcommit (`vm.overcommit_memory`) lets the kernel grant more than it has; when the bet fails, the OOM killer uses `oom_score` to pick a victim.
5. Judge memory with `MemAvailable`, not `MemFree`; a big page cache is healthy.
