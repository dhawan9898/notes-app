---
title: Measuring Before Tuning
track: linux
module: Performance and Observability
moduleOrder: 9
order: 1
level: advanced
minutes: 45
summary: "Four exercises in performance method: reading vmstat and pressure-stall information while three busy loops share one CPU, showing a 20x slowdown caused only by memory access order, finding a hotspot with gprof, and applying the USE method (utilisation, saturation, errors) to decide what to look at first."
prerequisites:
  - "Performance Methodology: USE, Load, CPU and Memory Analysis"
  - "Profiling and Tracing — perf, ftrace, Flame Graphs and Friends"
  - "A Linux Troubleshooting Playbook"
sources:
  - label: "Gregg — Systems Performance, 2nd ed., Ch. 2 (Methodologies) and Ch. 6 (CPUs)"
  - label: "Linux kernel documentation — PSI: Pressure Stall Information"
    url: https://docs.kernel.org/accounting/psi.html
  - label: "GNU gprof manual"
    url: https://sourceware.org/binutils/docs/gprof/
  - label: "Gregg — The USE Method"
    url: https://www.brendangregg.com/usemethod.html
---

"Make it faster" is a bad starting point; "find the one resource that is saturated" is a good one. These exercises train the habit of **measuring first**, with tools that exist on almost every Linux machine (`perf` and `strace` are the more powerful options but often absent). All results are from real runs on an 8-CPU machine; yours will differ in size, not in pattern.

## Exercises

**1. Utilisation is not saturation.** Pin three busy loops to **one** CPU with `taskset -c 0` and watch `vmstat 1 3` and `/proc/pressure/cpu`. Which columns show that something is wrong, even though the machine as a whole is 84% idle?

```text
$ taskset -c 0 sh -c 'while :; do :; done' &      # run this three times
$ vmstat 1 3
procs -----------memory---------- ---swap-- -----io---- -system-- -------cpu----
 r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st
 3  1 242324 1970724 535920 8016856    0    0   244   176 6125    5  2  3 92  0
 3  0 242324 1972772 535920 8016904    0    0     0     4 4209 5733 14  2 84  0
 4  0 242324 1970884 535920 8016912    0    0     0    44 4305 5962 14  2 84  0
$ cat /proc/pressure/cpu | head -1
some avg10=18.59 avg60=3.48 avg300=0.99 total=3230916335
$ nproc
8
```

**2. Same work, 20 times slower.** The program below sums a 4096 × 4096 array of `int` (64 MiB) twice: row by row and column by column. Predict the ratio before running it, then explain it.

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 4096
static double secs(struct timespec a, struct timespec b) { return (double)(b.tv_sec - a.tv_sec) + (double)(b.tv_nsec - a.tv_nsec) / 1e9; }

int main(void) {
    int *m = malloc((size_t)N * N * sizeof *m);
    if (!m) return 1;
    for (size_t i = 0; i < (size_t)N * N; i++) m[i] = (int)(i & 7);
    struct timespec t0, t1;
    long s1 = 0, s2 = 0;
    double best_row = 1e9, best_col = 1e9;
    for (int run = 0; run < 3; run++) {
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) s1 += m[(size_t)r * N + c];     /* walks memory in order */
        clock_gettime(CLOCK_MONOTONIC, &t1);
        if (secs(t0, t1) < best_row) best_row = secs(t0, t1);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int c = 0; c < N; c++) for (int r = 0; r < N; r++) s2 += m[(size_t)r * N + c];     /* jumps 16 KiB each step */
        clock_gettime(CLOCK_MONOTONIC, &t1);
        if (secs(t0, t1) < best_col) best_col = secs(t0, t1);
    }
    printf("same sum (%s): %ld\n", s1 == s2 ? "equal" : "DIFFERENT", s1 / 3);
    printf("row by row:       %.0f ms\ncolumn by column: %.0f ms  (%.1fx slower)\n", best_row * 1e3, best_col * 1e3, best_col / best_row);
    free(m);
    return 0;
}
```

```text
$ gcc -O1 -std=gnu17 -o pfm pf_matrix.c && ./pfm
same sum (equal): 58720256
row by row:       16 ms
column by column: 362 ms  (22.0x slower: the same work, different memory access pattern)
```

**3. Find the hotspot.** Build a small program with `gcc -pg` and read the `gprof` flat profile. Which function takes the time, and what does that tell you about where optimisation effort belongs?

```c
static int is_prime(int n) { if (n < 2) return 0; for (int d = 2; d * d <= n; d++) if (n % d == 0) return 0; return 1; }
static int count_primes(int limit) { int c = 0; for (int i = 0; i < limit; i++) c += is_prime(i); return c; }
static unsigned checksum(const char *s, int rounds) { unsigned h = 0; for (int r = 0; r < rounds; r++) for (const char *p = s; *p; p++) h = h * 31 + (unsigned char)*p; return h; }
/* main: prints count_primes(600000) and checksum("the quick brown fox", 20000) */
```

```text
$ gcc -O0 -pg -std=gnu17 -o pfp pf_prof.c && ./pfp && gprof -b -p ./pfp gmon.out | head -6
primes below 600000: 49098
checksum: 2224902656
Flat profile:

Each sample counts as 0.01 seconds.
  %   cumulative   self              self     total           
 time   seconds   seconds    calls  ms/call  ms/call  name    
100.16      0.34     0.34   600000     0.00     0.00  is_prime
```

**4. The USE method.** For each of CPU, memory, disk and network, what are the *utilisation*, *saturation* and *errors* measurements you would check first on Linux, and which exercise above showed a saturation signal?

## Answers

Try before opening.

<details>
<summary>Show the explanations</summary>

**1.** Look at **`r`** (processes that are *runnable*, running or waiting for a CPU) and the **pressure** file, not at the idle percentage. `r` is 3 to 4 while the machine has 8 CPUs: more runnable tasks than the CPU they are all pinned to can serve. `id` (idle) is still 84% because the other seven CPUs have nothing to do, so a system-wide average hides a **saturated** CPU: `us` of 14% is one CPU of eight (12.5%) at 100%. The **PSI** line `some avg10=18.59` is the clearest signal: for **18.6% of the last 10 seconds at least one task was stalled waiting for CPU time**, and it takes no knowledge of the topology to read (`/proc/pressure/memory` and `io` work the same way, and a `full` line means *all* non-idle tasks were stalled). Load average would also rise (load counts runnable tasks, so three loops on one CPU contribute about 3). Utilisation answers "how busy?", saturation answers "is work waiting?": performance problems show as saturation first. Always look **per CPU** (`mpstat -P ALL` or `top` then `1`) before concluding that a machine is idle.

**2.** The array is stored row after row, so the first loop reads **consecutive addresses**: each 64-byte cache line brought in from RAM serves 16 integers, and the hardware **prefetcher** recognises the pattern and loads lines ahead of time. The second loop jumps **16 KiB** (4096 ints) between accesses, so every access lands in a different cache line and a different page; each line is used once for one 4-byte value and evicted before its neighbours are needed, and the prefetcher cannot help. The CPU spends its time waiting for memory. The arithmetic is identical (the sums are equal), yet the runtime differs by about **20 times** (16 ms against 300 to 360 ms; the exact ratio varies by machine and run). Lessons: loop order matters for multi-dimensional arrays (keep the *last* index in the inner loop in C), data layout often matters more than instruction counts, and **a profile that says "the loop is slow" does not say why**: that needs hardware counters (`perf stat -e cache-misses`) or a hypothesis like this one. This is also why linked lists lose to arrays in practice (see *Linked Lists, Stacks and Queues for Beginners*).

**3.** `gprof` samples the program counter every 10 ms and counts calls: **`is_prime` accounts for essentially all of the time** (0.34 s over 600,000 calls; the percentage can exceed 100 slightly because of rounding in the sampling), while `checksum` did not register at all. Optimising `checksum` would gain nothing; speeding up `is_prime` (a sieve, or checking only odd divisors) is where all the benefit is. **Amdahl's law** says the same thing: speeding up a part that takes 1% of the time cannot gain more than 1%. The method is always: **measure, find the dominant cost, change that, measure again**. Two cautions: `-pg` instruments the code and works best with `-O0`, so the profile of an optimised build can differ (inlining hides functions); and sampling profilers see only where time is *spent*, not why. On production systems `perf record -g` with a flame graph (see *Profiling and Tracing*) gives the same answer without recompiling, across the whole stack including the kernel.

**4.** **USE** (Brendan Gregg's method) checks every resource for **U**tilisation (the fraction of time it was busy), **S**aturation (extra work queued that it cannot service) and **E**rrors. Starting points on Linux:

| Resource | Utilisation | Saturation | Errors |
|---|---|---|---|
| CPU | `mpstat -P ALL`, `top` (per CPU `us+sy`) | `vmstat` column `r` above the CPU count, load average, `/proc/pressure/cpu` | machine-check events (`dmesg`) |
| Memory | `free`, `MemAvailable` in `/proc/meminfo` | swapping (`si`, `so` in `vmstat`), `/proc/pressure/memory`, OOM kills | `dmesg` out-of-memory messages, ECC errors |
| Disk | `iostat -x` `%util` | queue size and `await`, `/proc/pressure/io`, `b` in `vmstat` | I/O errors in `dmesg`, SMART data |
| Network | interface throughput against link speed (`sar -n DEV`, `ip -s link`) | drops and overruns (`ip -s link`, `ss -s`, `nstat`), qdisc backlog (`tc -s qdisc`) | `ip -s link` error counters, retransmits (`nstat -az TcpRetransSegs`) |

The method turns a vague "it is slow" into a short checklist that finds **bottlenecks you would not have guessed**. In the exercises, exercise 1 showed **saturation without high utilisation** (run queue and CPU pressure on one CPU), exercise 2 a memory-bound program that looks 100% busy on the CPU but is really waiting for RAM, and exercise 3 where the time goes inside a program. Only after identifying the saturated resource do you tune: a faster CPU will not fix a memory-bound loop, and more RAM will not fix a single pinned CPU.

</details>

## Key takeaways

1. Utilisation is not saturation: look at the run queue (`vmstat` `r`), per-CPU usage and PSI pressure files, not just the idle percentage.
2. Memory access pattern can change runtime by 20x with identical arithmetic; walk arrays in the order they are stored.
3. Profile before optimising: the hotspot is usually one function, and effort elsewhere is wasted (Amdahl's law).
4. Apply the USE method to every resource, and tune only the one that is saturated or erroring.
