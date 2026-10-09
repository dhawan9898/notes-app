---
title: Virtual Memory, Paging and the Page Cache
track: linux
module: Memory
moduleOrder: 4
order: 1
level: advanced
summary: How every process gets a private address space, how page tables and the TLB translate addresses, what page faults really are, how the kernel caches files and reclaims memory, and how to read free, meminfo and smaps correctly.
sources:
  - label: "Linux kernel docs — Memory Management (Documentation/admin-guide/mm, Documentation/mm)"
    url: https://docs.kernel.org/admin-guide/mm/index.html
  - label: "Intel SDM Vol. 3A Ch. 4 / AMD64 APM Vol. 2 — Paging (4-level and 5-level)"
  - label: "man 5 proc (meminfo, smaps, maps), man 8 free, man 8 vmstat"
    url: https://man7.org/linux/man-pages/man5/proc_meminfo.5.html
  - label: "Gorman — Understanding the Linux Virtual Memory Manager"
  - label: "Drepper — What Every Programmer Should Know About Memory"
  - label: "Linux kernel docs — Concepts overview (mm)"
    url: https://docs.kernel.org/admin-guide/mm/concepts.html
---

## Virtual addresses

Every process sees a private, contiguous **virtual address space**; the CPU's **MMU** translates each access to a physical address using **page tables** maintained by the kernel. This gives:

- **Isolation**: a process cannot touch another's memory.
- **Overcommit and lazy allocation**: addresses can be reserved without backing memory.
- **Sharing**: libraries, `fork` copies and shared memory map the same physical pages in many address spaces.
- **Flexibility**: files can be mapped into memory; memory can be moved, swapped or compacted behind the program's back.

On x86-64 (48-bit virtual addresses, 4-level paging) user space occupies `0x0000000000000000–0x00007FFFFFFFFFFF` (128 TiB) and the kernel the upper half; 5-level paging extends to 57 bits (128 PiB). A typical process layout (low → high):

```text
text (code)  |  data  |  bss  |  heap (brk, grows up) ...      mmap region (libs, anon, files)  ...  stack (grows down)
```

`cat /proc/PID/maps` lists each mapping: range, permissions (`r-xp`), file offset, device, inode, path (`[heap]`, `[stack]`, `[vdso]`). `pmap -x PID` and `/proc/PID/smaps` add RSS/PSS per mapping. **ASLR** (`kernel.randomize_va_space = 2`) randomizes stack, mmap, heap and (for PIE binaries) code base addresses.

## Pages, page tables and the TLB

Memory is managed in fixed-size **pages**: 4 KiB on x86 (`getconf PAGESIZE` → `4096`), larger on some ARM/POWER configurations. Physical memory is an array of **page frames**.

A virtual address splits into indices into a multi-level tree (x86-64: 9 + 9 + 9 + 9 bits + 12-bit page offset):

```text
PGD -> PUD -> PMD -> PTE -> 4 KiB page frame
 9b     9b     9b     9b      + 12-bit offset
```

Each table has 512 8-byte entries. A **PTE** holds the frame number and flags: present, writable, user, accessed, dirty, no-execute, plus software bits. A PMD entry may map a **2 MiB huge page** directly, a PUD entry a **1 GiB** page.

Walking four levels per access would be ruinous, so the CPU caches translations in the **TLB** (Translation Lookaside Buffer, tens to thousands of entries). A **TLB miss** costs a page-table walk (tens of cycles, possibly several cache misses); a context switch between processes would flush it, so CPUs tag entries with **PCID/ASID**. Large working sets with random access suffer from TLB misses, which is the reason for **huge pages**. Changing mappings in multi-threaded programs requires **TLB shootdowns** (inter-processor interrupts), which makes `munmap`/`mprotect` heavy on many cores.

Page tables cost memory themselves (`PageTables` in `/proc/meminfo`); a huge sparse mapping or thousands of processes mapping GBs can consume significant memory.

## Page faults

A **page fault** is the CPU asking the kernel to resolve an access it can't complete. Faults are normal and are how Linux implements demand paging:

| Kind | Cause | Cost |
|---|---|---|
| **Minor** | page is in memory but not mapped for this process: first touch of anonymous memory (allocate and zero a frame), copy-on-write, file page already in the page cache | ~1 µs |
| **Major** | the data must be read from storage (file page not cached, or swapped-out page) | 100 µs (NVMe) to ms (disk) |
| **Invalid** | address not mapped, or permission violation | kernel sends `SIGSEGV` |

Demand paging in action (Python mapping 256 MiB of anonymous memory):

```text
RSS start MiB: 8
after mmap 256 MiB (untouched) RSS MiB: 8          <- address space grew (VmSize), no memory used yet
after touching every page   RSS MiB: 264           <- 65,536 pages touched = 65,536 minor faults
minor faults: 66431  major faults: 2
```

- **Copy-on-write (COW)**: `fork` duplicates page tables, marks pages read-only; the first write by either process faults, and the kernel copies that page. This makes `fork` cheap and `fork+exec` practically free of copying, but a large process forking *and continuing* (e.g. Redis `BGSAVE`) can double memory use under heavy writes.
- **Zero page**: reading untouched anonymous memory maps a shared zero page; allocation happens on write.
- **Stack growth** and `brk`/`mmap` regions are extended on demand.

Measure: `/usr/bin/time -v cmd` (page faults), `ps -o min_flt,maj_flt`, `perf stat -e page-faults`, `pidstat -r`, `/proc/vmstat` (`pgfault`, `pgmajfault`).

## Kinds of memory and how to count it

| Type | Backing | If reclaimed |
|---|---|---|
| **Anonymous** (heap, stack, `MAP_ANONYMOUS`) | nothing on disk | must be **swapped** (or killed) |
| **File-backed, clean** (code, mapped files, page cache) | the file | simply dropped, re-read later |
| **File-backed, dirty** | the file | written back first |
| **Shared** (`shm`, tmpfs, `MAP_SHARED`) | RAM/tmpfs (+swap) | swap |
| **Kernel** (slab, page tables, stacks, network buffers) | RAM | partly reclaimable (slab) |

Per-process numbers (`ps`, `top`):

| Column | Meaning |
|---|---|
| **VSZ / VIRT** | address space size: mostly meaningless (reservations, shared libs, huge arenas) |
| **RSS / RES** | resident pages (includes shared pages, so summing RSS over processes double counts) |
| **USS** | unique set size: private pages only |
| **PSS** | proportional set size: shared pages divided among sharers: sums correctly (`/proc/PID/smaps_rollup`, `smem`) |
| `SHR` | resident shared |

## The page cache

Linux uses almost all otherwise idle RAM to cache file data: the **page cache**. File reads fill it; subsequent reads are served from memory; writes go to the cache first (**write-back**), making `write()` return quickly.

- **Read-ahead** detects sequential access and prefetches (`blockdev --getra`, `posix_fadvise(SEQUENTIAL)`, `readahead`).
- **Dirty pages** are written by kernel flusher threads. Default tunables: `vm.dirty_background_ratio=10` (start background writeback at 10 % of available memory), `vm.dirty_ratio=20` (**writers block** above 20 %), `vm.dirty_expire_centisecs=3000` (pages older than 30 s are flushed), `vm.dirty_writeback_centisecs=500`. On servers with fast writers and slow disks, large ratios create huge write bursts and stalls: use the absolute `vm.dirty_bytes`/`vm.dirty_background_bytes`. Durability requires `fsync` (see the filesystem notes), not merely `write`.
- **Page cache is shared** among all processes and is **counted in cgroup memory** accounting.
- `O_DIRECT` bypasses it (databases that manage their own cache); `posix_fadvise(DONTNEED)` and `madvise` hint; `echo 3 > /proc/sys/vm/drop_caches` empties the clean cache (for benchmarking only; never as a "fix").

Reading `free -m` on an example host:

```text
              total  used  free  shared  buff/cache  available
Mem:          20006 17260  1971     181        1419       2745
Swap:           354   354     0
```

**`free` is not "available".** `MemAvailable` (2.7 GB here) estimates how much memory could be allocated without swapping: free plus reclaimable cache/slab. A system with little *free* but large *available* is healthy. On this example, most memory (`AnonPages` ≈ 16.5 GB) is anonymous and swap is full, so this example machine is genuinely under pressure, unlike one with a big `buff/cache` line.

## Reclaim: how the kernel frees memory

Memory is organized in **zones** (DMA, DMA32, Normal, Movable) and allocated by the **buddy allocator**, which hands out blocks of 2^order pages (order 0–10, up to 4 MiB); `cat /proc/buddyinfo` shows free blocks per order (fragmentation = no large free blocks). Small kernel objects come from **slab/SLUB** caches (`slabtop`): dentries and inodes are reclaimable; `vm.vfs_cache_pressure` (default 100) biases that.

Each zone has **watermarks** (min, low, high) scaled from `vm.min_free_kbytes`:

1. When free pages fall below *low*, **kswapd** (per node) reclaims in the background until *high*.
2. If allocations outpace kswapd and free memory approaches *min*, the allocating task does **direct reclaim** itself: this is what causes latency spikes (`allocstall_*` in `/proc/vmstat`; PSI memory `some`/`full`).
3. If reclaim fails, **compaction** defragments for large allocations, and as a last resort the **OOM killer** runs (next note).

Reclaim works on **LRU lists** (active/inactive × anon/file): pages that haven't been touched recently are evicted. Clean file pages are cheapest; dirty ones need writeback; anonymous pages require swap space. `vm.swappiness` (0–200; default 60) sets the relative preference between reclaiming anonymous (swap) vs file pages; it is *not* a percentage threshold. A "thrashing" system continually evicts and refaults its working set: high `si/so` in `vmstat 1`, rising major faults, high iowait, PSI `full` > 0.

The **working set** concept: if the hot pages of all processes plus needed cache exceed RAM, performance collapses (superlinearly). Capacity planning should be based on working set, not on free memory.

## Memory cgroups

Per-cgroup accounting includes anonymous memory, page cache, kernel memory and swap; limits (`memory.max`, `memory.high`) trigger reclaim and OOM *within the cgroup*. A container may be OOM-killed while the host has plenty of free RAM. Details in the cgroups note.

## Tools

```bash
free -m ; vmstat 1 ; cat /proc/meminfo
cat /proc/PID/status | grep -E 'Vm|Rss'       # VmPeak VmSize VmRSS VmData VmStk VmSwap
cat /proc/PID/smaps_rollup                    # Pss, Private_Dirty, Swap...
pmap -x PID ; smem -tk                        # per-mapping, per-process PSS
slabtop -o | head ; cat /proc/buddyinfo
sar -B 1 ; sar -r 1                           # paging and memory utilization history
cat /proc/pressure/memory                     # PSI: share of time stalled on memory
perf stat -e page-faults,dTLB-load-misses ./prog
```

## Key takeaways

- Virtual memory gives isolation, lazy allocation and sharing; a 4-level page table (512 entries per level) plus the TLB translates addresses.
- Minor faults are cheap and normal (demand paging, COW); major faults hit storage; invalid ones raise SIGSEGV.
- Use PSS (not RSS or VSZ) to account for shared memory.
- The page cache uses spare RAM, is written back in the background (dirty ratios), and is reclaimed first; judge health by `MemAvailable`, reclaim stalls (PSI) and swap activity, not by "free".
- Anonymous memory cannot be dropped, only swapped or killed; plan for the working set.
