---
title: "mmap, Overcommit, the OOM Killer, Huge Pages, NUMA and Swap"
track: linux
module: Memory
moduleOrder: 4
order: 2
level: expert
summary: Allocation behind malloc, why the kernel promises more memory than exists, how the OOM killer chooses victims and how to control it, transparent and explicit huge pages, NUMA placement, and swap design including zram.
sources:
  - label: "man 2 mmap, man 2 madvise, man 3 malloc, man 3 mallopt, man 5 proc (overcommit_memory, oom_score_adj)"
    url: https://man7.org/linux/man-pages/man2/mmap.2.html
  - label: "Linux kernel docs — Overcommit Accounting"
    url: https://docs.kernel.org/mm/overcommit-accounting.html
  - label: "Linux kernel docs — Transparent Hugepage Support; HugeTLB Pages"
    url: https://docs.kernel.org/admin-guide/mm/transhuge.html
  - label: "Linux kernel docs — cgroup v2 memory controller; PSI"
    url: https://docs.kernel.org/admin-guide/cgroup-v2.html
  - label: "Linux kernel docs — NUMA memory policy; Automatic NUMA balancing"
    url: https://docs.kernel.org/admin-guide/mm/numa_memory_policy.html
  - label: "Lameter — NUMA (Non-Uniform Memory Access) - An Overview, ACM Queue 2013"
---

## mmap and how malloc gets memory

`mmap(addr, len, prot, flags, fd, off)` creates a mapping in the address space:

| Flags | Meaning |
|---|---|
| `MAP_PRIVATE` | copy-on-write mapping: writes are not visible to others or written back |
| `MAP_SHARED` | writes are visible to other mappers and (for files) written to the file |
| `MAP_ANONYMOUS` | not backed by a file: zero-filled memory (`malloc`, thread stacks, shared anonymous memory across `fork`) |
| `MAP_FIXED`, `MAP_FIXED_NOREPLACE` | exact address (the latter fails instead of clobbering) |
| `MAP_POPULATE` | prefault all pages now (avoid later faults) |
| `MAP_HUGETLB`, `MAP_NORESERVE`, `MAP_STACK`, `MAP_GROWSDOWN` | special cases |

`mmap` of a **file** gives page-cache-backed access with pointer syntax, which is fast for random reads and shared by processes, but I/O errors appear as `SIGBUS`, truncation of the file under you also yields `SIGBUS`, and there is no explicit control over write timing (`msync`). `madvise(addr, len, advice)` gives hints: `MADV_WILLNEED` (prefetch), `MADV_DONTNEED` (drop now: anonymous pages are zeroed on next touch), `MADV_FREE` (lazily free), `MADV_HUGEPAGE`/`NOHUGEPAGE`, `MADV_DONTDUMP`. `mlock`/`mlockall` pin pages in RAM (real-time, secrets; needs `RLIMIT_MEMLOCK`).

### glibc malloc

`malloc` is a user-space allocator layered on two kernel interfaces:

- **`brk`/`sbrk`** grows the main **heap** for small requests.
- **`mmap`** for large requests: above `M_MMAP_THRESHOLD` (128 KiB by default, adjusted dynamically up to 32 MiB), which `free` returns to the OS immediately via `munmap`.
- Per-thread **arenas** (up to 8 × cores) reduce lock contention but increase fragmentation and memory footprint: `MALLOC_ARENA_MAX=2` often cuts RSS of multithreaded services dramatically.
- `free` rarely returns small chunks to the OS (heap fragmentation, top-of-heap trimming only): "my process doesn't shrink after freeing" is normal. `malloc_trim(0)` helps; alternatives **jemalloc**, **tcmalloc**, **mimalloc** have better fragmentation and scalability and profiling (`MALLOC_CONF=prof:true`).
- A successful `malloc` does not mean memory exists: pages are allocated at first touch (minor fault).

## Overcommit

Linux deliberately lets processes reserve more virtual memory than there is RAM + swap, betting that most of it is never touched (sparse arrays, `fork` COW, large `malloc`s, JVM heaps). Policy `vm.overcommit_memory`:

| Value | Behavior |
|---|---|
| **0** (default) | heuristic: refuse **obvious** overcommits (a single allocation larger than total RAM + swap) |
| **1** | always succeed (needed by some scientific apps, Redis recommends it for `fork`) |
| **2** | strict: total committed address space may not exceed `CommitLimit = swap + RAM × overcommit_ratio/100` (ratio default 50; or `overcommit_kbytes`) |

On an example machine:

```text
CommitLimit:    10606696 kB
Committed_AS:   31689828 kB     <- promised ~3x what the strict limit would allow; mode 0 permits it
```

With overcommit, failure is deferred from `malloc` time to **touch time**: when real memory runs out the kernel cannot return `ENOMEM` to a page fault, so it invokes the **OOM killer**. With mode 2 (strict) allocations fail early and predictably (`ENOMEM`), but many programs reserve far more than they use (large `fork` of a big process counts the whole child), so strict mode needs careful sizing: appropriate for some embedded and safety-critical systems, rarely for general servers.

## The OOM killer

When reclaim cannot free enough pages, the kernel selects a victim to kill so the system can proceed.

### Victim selection

Each process has an **`oom_score`** (`/proc/PID/oom_score`): approximately the share (0–1000) of total RAM + swap it uses (RSS + swap + page tables), adjusted by **`oom_score_adj`** (−1000…+1000, `/proc/PID/oom_score_adj`). `-1000` exempts a process; `+1000` makes it the first target. The highest-scoring task (children's memory is not summed) is killed with `SIGKILL`. Root privilege no longer protects by default.

```bash
echo -1000 > /proc/$(pidof sshd)/oom_score_adj       # protect sshd
# systemd:  OOMScoreAdjust=-900   (and  MemoryMax=…  to bound the service instead)
sort -k1 -n <(for p in /proc/[0-9]*; do echo "$(cat $p/oom_score 2>/dev/null) ${p##*/} $(cat $p/comm 2>/dev/null)"; done) | tail -5
```

### Reading the log

```text
$ dmesg -T | grep -i -A3 'out of memory'
Out of memory: Killed process 4242 (java) total-vm:12582912kB, anon-rss:7340032kB, file-rss:1024kB, shmem-rss:0kB, UID:1000 pgtables:15200kB oom_score_adj:0
oom_reaper: reaped process 4242 (java), now anon-rss:0kB, file-rss:0kB, shmem-rss:0kB
```

Before this line the kernel prints the allocation that failed (`gfp_mask`, `order`), per-zone free memory, a table of all tasks with their memory and `oom_score_adj`: use it to see who held memory and whether it was a cgroup OOM (`oom-kill:constraint=CONSTRAINT_MEMCG` and the cgroup path) or global (`CONSTRAINT_NONE`). `oom_kill` counter in `/proc/vmstat`; `journalctl -k | grep -i oom`.

### Better than the global OOM killer

- **cgroup limits**: `memory.max` is a hard limit inside which the OOM killer acts (killing only in that group; `memory.oom.group=1` kills the whole group); `memory.high` throttles allocation via reclaim before the limit (soft); `memory.low`/`min` protect from reclaim; `memory.events` counts `high`, `max`, `oom`, `oom_kill`. Containers/Kubernetes use these (`OOMKilled`, exit code 137).
- **Userspace OOM daemons** act earlier using PSI and pressure, killing deliberately before the kernel thrashes: `systemd-oomd`, `earlyoom`, Facebook's `oomd`. Global OOM can take minutes of thrashing to trigger on a swapping box; PSI-based killers react in seconds.
- Right-size memory limits, avoid unbounded caches, set JVM `-Xmx` below the container limit (heap + metaspace + threads + direct buffers).

## Huge pages

A 2 MiB page reduces page-table depth and TLB entries 512×. Gains: 5–20 % on memory-intensive, random-access workloads (databases, JVM heaps, in-memory analytics, virtualization).

| Mechanism | How | Characteristics |
|---|---|---|
| **THP** (Transparent Huge Pages) | kernel promotes/aligns anonymous regions automatically (`khugepaged`, fault-time) | `enabled`: `always` / `madvise` / `never` (commonly `always` or `madvise`). Zero config, but allocation may stall for **compaction**, and memory bloat when sparse touches pull in 2 MiB |
| **HugeTLB** (`hugetlbfs`) | pre-reserved pool: `vm.nr_hugepages=1024` or boot `hugepages=`; `mmap` with `MAP_HUGETLB`, `shmget(SHM_HUGETLB)`, or mount hugetlbfs; 2 MiB or 1 GiB sizes | guaranteed, never swapped, not counted as normal memory; must be reserved ahead |

Databases such as Oracle, PostgreSQL (`huge_pages=try/on`), DPDK and KVM prefer **explicit hugepages**; Redis, MongoDB and others advise setting THP to `madvise`/`never` because defragmentation causes latency spikes: `echo madvise > /sys/kernel/mm/transparent_hugepage/enabled`. Check use: `grep -i huge /proc/meminfo` (`AnonHugePages`, `HugePages_*`), `/proc/PID/smaps` `AnonHugePages`. Fragmentation prevents allocation, and `echo 1 > /proc/sys/vm/compact_memory` can help.

## NUMA

On multi-socket (and some multi-chiplet) machines memory is attached to specific CPU packages. A CPU accesses **local** memory faster than **remote** memory (typically 1.3–2× the latency, lower bandwidth over the interconnect).

```bash
numactl --hardware        # nodes, sizes, distances (10 = local, 21 = one hop)
lscpu | grep NUMA ; numastat -m ; numastat -p PID
numactl --cpunodebind=0 --membind=0 ./app      # CPU and memory on node 0
numactl --interleave=all ./db                  # spread pages evenly (big shared caches)
```

Policies (`set_mempolicy`, `mbind`): **local** (default, "first-touch": a page is placed on the node of the CPU that first touches it), bind, preferred, interleave. Consequences:

- Initializing a big array from one thread places it all on that node; later parallel access from other sockets is remote. Initialize in parallel with the same thread layout as use (first-touch awareness).
- A node can run out of memory while others are free (local allocation fails → fallback or reclaim); symptom: swapping with free memory elsewhere. `vm.zone_reclaim_mode=0` (default) prefers using remote memory over reclaiming.
- **Automatic NUMA balancing** (`kernel.numa_balancing`) samples access via page faults and migrates tasks/pages toward each other: helps general workloads, harms latency-sensitive ones.
- VMs and containers should be sized within a node when possible; Kubernetes' topology manager / CPU manager provide alignment.

## Swap

Swap is storage used to evict **anonymous** pages (and tmpfs/shm), keeping RAM for hot data. Without swap, cold anonymous memory can never be reclaimed, so file cache gets squeezed instead.

- **Swap is not "emergency memory"**: a little swap use (cold pages of idle daemons) is healthy; the signal of trouble is **swap activity** (`si`/`so` in `vmstat`, `pswpin/pswpout` in `/proc/vmstat`) together with high PSI memory and major faults.
- `vm.swappiness`: 0–200 (default 60) tilts reclaim between anon and file pages (low values avoid swapping anonymous memory but never fully disable it; 1 is common for databases, ~100 for zram setups).
- Devices: partition, file (`mkswap`, `swapon`; not on Btrfs CoW without NOCOW), multiple with priorities.
- **zram** (compressed swap in RAM, 2–3× compression, microsecond latency) and **zswap** (compressed cache in front of a disk swap) give "free" extra capacity at some CPU cost; common on desktops, containers' hosts and Android/ChromeOS.
- Hibernation needs swap ≥ RAM image size. Sizing otherwise: a few GiB is plenty for most servers; huge swap just prolongs thrashing before OOM. With cgroup v2, `memory.swap.max` limits per workload (Kubernetes disables swap for pods by default).
- Encrypted swap (dm-crypt with random key) protects secrets; `mlock` keeps keys out of swap.

## Diagnosing memory problems

| Symptom | Check |
|---|---|
| "Out of memory" kills | `dmesg`, which process, which cgroup; leak vs undersized limit; heap settings |
| Slow, disk busy, `si/so` > 0 | working set > RAM: PSI memory, `vmstat 1`, reduce footprint or add RAM |
| High `sys` CPU, `kswapd` busy | reclaim/compaction pressure; THP `defrag`; `min_free_kbytes` |
| RSS grows forever | leak (heap profilers: `valgrind --tool=massif`, `heaptrack`, `jemalloc prof`, `bpftrace`/`memleak`) vs allocator fragmentation (`malloc_stats`, arenas) |
| `free` shows little free | usually fine: read `available` |
| Container killed at limit though app "uses less" | page cache and kernel memory count; check `memory.stat` |
| Latency spikes under load | THP compaction, direct reclaim (`allocstall`), NUMA remote access, swap |
| Many small processes use lots of RAM | page tables, per-process arenas; use PSS (`smem`) |

## Key takeaways

- `malloc` uses `brk` and `mmap` and succeeds lazily; real allocation happens at first touch.
- Overcommit (mode 0) allows `Committed_AS` far above `CommitLimit`; the price is the OOM killer, which kills by `oom_score` (RSS-based, adjustable) and is best preempted with cgroup limits and PSI-based userspace killers.
- Use huge pages for TLB-heavy workloads, explicit (HugeTLB) for predictability and `madvise` THP for safety.
- On NUMA machines keep CPU and memory local: first-touch placement, `numactl`, parallel initialization.
- Swap is for cold anonymous pages: judge it by activity, not by usage; zram/zswap are cheap capacity.
