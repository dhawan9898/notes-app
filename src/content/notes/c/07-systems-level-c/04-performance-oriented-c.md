---
title: "Performance-Oriented C: Caches, Branches, Data Layout and Vectorization"
track: c
module: Systems-Level C
moduleOrder: 7
order: 4
level: expert
summary: How to reason about and measure C performance on modern CPUs — the memory hierarchy, branch prediction, data layout (AoS vs SoA), false sharing and vectorization — each demonstrated with a benchmark whose results span 5× to 50×.
sources:
  - label: "Drepper — What Every Programmer Should Know About Memory (2007)"
    url: https://people.freebsd.org/~lstewart/articles/cpumemory.pdf
  - label: "Fog — Optimizing software in C++; Instruction tables; Microarchitecture (agner.org)"
    url: https://www.agner.org/optimize/
  - label: "Intel 64 and IA-32 Architectures Optimization Reference Manual"
  - label: "Hennessy & Patterson — Computer Architecture, A Quantitative Approach (memory hierarchy, ILP)"
  - label: "Gregg — Systems Performance, 2nd ed.; Mytkowicz et al. — Producing Wrong Data Without Doing Anything Obviously Wrong! (ASPLOS 2009)"
  - label: "GCC manual — Optimize Options; auto-vectorization (-fopt-info-vec); __builtin_prefetch, __builtin_expect"
    url: https://gcc.gnu.org/projects/tree-ssa/vectorization.html
---

C gives control over memory layout and instructions, which is why it dominates performance-critical software. But modern CPUs execute billions of instructions per second only when the data is in cache, branches are predictable, and independent work is available. Most speedups come from **doing less work and touching less memory**, not from clever arithmetic. The experiments below were run on an 8-core AMD EPYC virtual machine with GCC 14; absolute numbers vary by hardware, ratios are what matter.

## Rule zero: measure

1. **Profile first** (`perf record`, flame graphs; see the Linux profiling note). Optimize the code that dominates, not the code you suspect.
2. **Algorithms and data structures first**: O(n²) → O(n log n) beats any micro-optimization.
3. Use realistic data and sizes; cache effects change with input size.
4. Prevent the compiler from deleting your benchmark: it removes unused computations, hoists loop-invariant work and folds constants. A `DO_NOT_OPTIMIZE(v)` barrier (`asm volatile("" : : "r"(v) : "memory")`) forces the value to be computed:

```c
#ifndef TIMING_H
#define TIMING_H
#include <time.h>
static inline double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
/* Prevent the optimizer from deleting a computed value or hoisting work out of a loop. */
#define DO_NOT_OPTIMIZE(v) __asm__ volatile("" : : "r,m"(v) : "memory")
#endif
```

5. Repeat runs, report min/median, pin the CPU (`taskset -c 2`), disable turbo/frequency scaling for stable numbers, warm caches/page tables, run long enough (≥ 100 ms), and beware of noisy neighbours in VMs. `perf stat` provides cycles, instructions, cache-misses and branch-misses (when exposed).
6. Compile as you ship: `-O2`/`-O3`, `-march`, LTO. Benchmarking `-O0` code is meaningless.
7. Re-measure after every change; keep a regression benchmark in CI.

## The memory hierarchy

A modern core can execute several instructions per nanosecond, but a trip to DRAM costs 60–100 ns (hundreds of instructions). Caches hide this when data has **locality**:

| Level | Typical size | Latency | Notes |
|---|---|---|---|
| Registers | ~16–32 | 0 | |
| L1D cache | 32–48 KiB / core | ~1 ns (4–5 cycles) | 64-byte lines |
| L2 | 256 KiB–2 MiB / core | ~3–5 ns (12–14 cycles) | |
| L3 | 8–256 MiB shared | ~10–30 ns (40–70 cycles) | shared, inclusive or victim |
| DRAM | GBs | 60–120 ns | NUMA remote: +50–100 % |
| NVMe | TBs | ~100,000 ns | |

Data moves in **cache lines (64 bytes)**; reading one byte loads 64. The hardware prefetcher detects sequential and strided access and fetches ahead, so streaming is nearly free while random access pays full latency.

### Experiment 1: traversal order

Summing a 4096×4096 `int` matrix by rows (consecutive addresses) versus by columns (16 KiB stride, one useful int per cache line), and chasing random pointers through arrays of growing size (`cache.c`):

```c
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "timing.h"

#define N 4096                              /* 4096 x 4096 ints = 64 MiB */
static int a[N][N];

int main(void) {
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) a[i][j] = i ^ j;

    double t0 = now(); long s1 = 0;
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) s1 += a[i][j];      /* row-major: sequential addresses */
    double t1 = now(); long s2 = 0;
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) s2 += a[i][j];      /* column-major: stride of 16 KiB */
    double t2 = now();
    DO_NOT_OPTIMIZE(s1); DO_NOT_OPTIMIZE(s2);
    printf("sum by rows    : %7.1f ms\nsum by columns : %7.1f ms  (%.1fx slower, same result: %s)\n", (t1 - t0) * 1e3, (t2 - t1) * 1e3, (t2 - t1) / (t1 - t0), s1 == s2 ? "yes" : "NO");

    /* working-set size vs time per access: random pointer chasing through arrays of growing size */
    printf("\nrandom access latency by working-set size (pointer chasing):\n");
    for (size_t kb = 4; kb <= 262144; kb *= 8) {
        size_t n = kb * 1024 / sizeof(size_t);
        size_t *next = malloc(n * sizeof *next), *perm = malloc(n * sizeof *perm);
        for (size_t i = 0; i < n; i++) perm[i] = i;
        uint64_t r = 88172645463325252ULL;
        for (size_t i = n - 1; i > 0; i--) { r ^= r << 13; r ^= r >> 7; r ^= r << 17; size_t j = r % (i + 1); size_t t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
        for (size_t i = 0; i < n; i++) next[perm[i]] = perm[(i + 1) % n];       /* one big cycle in random order */
        size_t p = 0; size_t steps = 20000000;
        double s = now();
        for (size_t i = 0; i < steps; i++) p = next[p];
        double dt = now() - s;
        DO_NOT_OPTIMIZE(p);
        printf("  %7zu KiB : %6.2f ns per access\n", kb, dt / steps * 1e9);
        free(next); free(perm);
    }
    return 0;
}
```

```text
$ gcc -O2 cache.c -o cache && ./cache
sum by rows    :     7.2 ms
sum by columns :   586.8 ms  (81.9x slower, same result: yes)

random access latency by working-set size (pointer chasing):
        4 KiB :   2.83 ns per access
       32 KiB :   2.86 ns per access
      256 KiB :   7.05 ns per access
     2048 KiB :  22.48 ns per access
    16384 KiB :  81.07 ns per access
   131072 KiB : 164.15 ns per access
```

- Same arithmetic, same result: the column walk is ~**50× slower** because every access misses cache and TLB, and the prefetcher can't help with the large stride. C arrays are row-major, so *the last index should vary fastest* in the inner loop. Loop interchange and **blocking/tiling** (process 64×64 tiles) are the standard fixes for matrix code.
- The pointer-chasing latencies reveal the hierarchy: ~**3 ns** while the set fits in L1 (4–32 KiB), ~8 ns in L2 (256 KiB), ~24 ns in L3 (2 MiB), ~**93–212 ns** once it spills out of cache into DRAM (plus TLB misses: 4 KiB pages mean large sets need **huge pages**, see the Linux memory notes). Random access to large data is two orders of magnitude slower than to small data: shrink the working set (smaller types, compact structures, indices instead of pointers, sorting/batching accesses).

### Experiment 2: data layout (AoS vs SoA)

A loop that needs only the `x` field of 4 million particles, stored as an **array of structs** (48-byte records) or a **struct of arrays**:

```c
#include <stdio.h>
#include <stdlib.h>
#include "timing.h"

#define N 4000000
struct particle_aos { float x, y, z, vx, vy, vz; int id; char name[20]; };      /* 48 bytes; we only need x */
struct particles_soa { float *x, *y, *z, *vx, *vy, *vz; };

int main(void) {
    struct particle_aos *aos = malloc(N * sizeof *aos);
    struct particles_soa soa = { malloc(N * 4), malloc(N * 4), malloc(N * 4), malloc(N * 4), malloc(N * 4), malloc(N * 4) };
    for (int i = 0; i < N; i++) { aos[i].x = (float)i; soa.x[i] = (float)i; }
    float s1 = 0, s2 = 0;
    double t0 = now();
    for (int rep = 0; rep < 20; rep++) for (int i = 0; i < N; i++) s1 += aos[i].x;           /* touches one float per 48-byte record */
    double t1 = now();
    for (int rep = 0; rep < 20; rep++) for (int i = 0; i < N; i++) s2 += soa.x[i];           /* contiguous floats */
    double t2 = now();
    DO_NOT_OPTIMIZE(s1); DO_NOT_OPTIMIZE(s2);
    printf("array of structs : %7.1f ms (%zu bytes/record, 4 of them used)\nstruct of arrays : %7.1f ms  (%.1fx faster)\n", (t1 - t0) * 1e3, sizeof(struct particle_aos), (t2 - t1) * 1e3, (t1 - t0) / (t2 - t1));
    return 0;
}
```

```text
$ gcc -O2 layout.c -o layout && ./layout
array of structs :   290.7 ms (48 bytes/record, 4 of them used)
struct of arrays :   136.7 ms  (2.1x faster)
```

The AoS loop drags 48 bytes per record through the cache to use 4 (12 % useful), so it is memory-bound; SoA reads contiguous floats (100 % useful) and the compiler can **vectorize** it. Guidelines: store hot and cold fields separately; order struct members for packing (previous structs note); prefer SoA or hybrid (AoSoA) for bulk numeric processing, AoS when you access whole records; use indices (`uint32_t`) instead of 8-byte pointers to halve memory; allocate objects of one kind contiguously (arenas/pools) instead of one `malloc` each; linked lists and pointer-rich trees are cache-hostile: prefer arrays, B-trees, open-addressing hash tables.

### Experiment 3: false sharing

Four threads each increment *their own* counter 50 million times. In one version the counters sit side by side (one 64-byte line); in the other each has its own line (`alignas(64)`):

```c
#define _GNU_SOURCE
#include <pthread.h>
#include <stdalign.h>
#include <stdio.h>
#include "timing.h"

#define THREADS 4
#define ITERS 50000000L
struct packed_counters { volatile long v[THREADS]; };                                    /* all in one 64-byte cache line */
struct padded_counter  { alignas(64) volatile long v; };                                 /* one cache line each */
static struct packed_counters packed;
static struct padded_counter padded[THREADS];

static void *work_packed(void *arg) { long id = (long)arg; for (long i = 0; i < ITERS; i++) packed.v[id]++; return NULL; }
static void *work_padded(void *arg) { long id = (long)arg; for (long i = 0; i < ITERS; i++) padded[id].v++; return NULL; }

static double run(void *(*fn)(void *)) {
    pthread_t t[THREADS];
    double t0 = now();
    for (long i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, fn, (void *)i);
    for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
    return now() - t0;
}

int main(void) {
    double a = run(work_packed), b = run(work_padded);
    printf("4 threads, counters adjacent in memory : %6.0f ms\n4 threads, one cache line per counter: %6.0f ms  (%.1fx faster, same work)\n", a * 1e3, b * 1e3, a / b);
    return 0;
}
```

```text
$ gcc -O2 -pthread falseshare.c -o falseshare && ./falseshare
4 threads, counters adjacent in memory :   1051 ms
4 threads, one cache line per counter:     93 ms  (11.3x faster, same work)
```

No data is shared, but cores must own a cache line exclusively to write it, so the line **ping-pongs** between cores via the coherence protocol: ~**18× slower**. Pad or align per-thread data to the cache-line size (`alignas(64)`, `_Alignas`; 128 on some ARM/Apple/POWER chips), use thread-local accumulators merged at the end, and measure with `perf c2c`. The same effect hits arrays of locks, per-CPU statistics, and adjacent fields written by different threads.

## Branches

CPUs predict branches and execute speculatively; a misprediction flushes the pipeline (~15–20 cycles). Predictable branches are free; **data-dependent unpredictable** branches are expensive. The classic demonstration sums elements ≥ 128 of 16 million random bytes, unsorted versus sorted (identical work). GCC at `-O1` and above often converts the branch into a conditional move on its own, hiding the effect, so the experiment disables **if-conversion** to show the raw branch behaviour:

```c
#include <stdio.h>
#include <stdlib.h>
#include "timing.h"

static int cmp(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

int main(void) {
    enum { N = 1 << 24 };
    int *v = malloc(N * sizeof *v);
    srand(1);
    for (int i = 0; i < N; i++) v[i] = rand() % 256;

    long s; double t0, t1;
    t0 = now(); s = 0;
    for (int rep = 0; rep < 10; rep++) for (int i = 0; i < N; i++) if (v[i] >= 128) s += v[i];     /* data-dependent branch */
    t1 = now(); DO_NOT_OPTIMIZE(s);
    printf("unsorted, branch   : %7.1f ms\n", (t1 - t0) * 1e3);

    qsort(v, N, sizeof *v, cmp);
    t0 = now(); s = 0;
    for (int rep = 0; rep < 10; rep++) for (int i = 0; i < N; i++) if (v[i] >= 128) s += v[i];     /* same code, predictable branch */
    t1 = now(); DO_NOT_OPTIMIZE(s);
    printf("sorted,   branch   : %7.1f ms   <- identical work, the branch is now predictable\n", (t1 - t0) * 1e3);

    t0 = now(); s = 0;
    for (int rep = 0; rep < 10; rep++) for (int i = 0; i < N; i++) { int m = -(v[i] >= 128); s += v[i] & m; }   /* branchless */
    t1 = now(); DO_NOT_OPTIMIZE(s);
    printf("sorted,   branchless: %6.1f ms\n", (t1 - t0) * 1e3);
    free(v); return 0;
}
```

```text
$ gcc -O1 -fno-if-conversion -fno-if-conversion2 branch.c -o branch && ./branch
unsorted, branch   :  1270.1 ms
sorted,   branch   :    80.3 ms   <- identical work, the branch is now predictable
sorted,   branchless:  112.4 ms
```

With the real branch, the unsorted data runs several times slower than the sorted data (the predictor guesses right ~50 % of the time on random data, ~100 % on sorted data); the branchless form is fast on both. Lessons:

- Let the compiler pick (it uses `cmov`/`setcc`/SIMD blends when profitable); check the assembly of hot loops.
- Make branches predictable: sort or partition data, process in batches by category, hoist invariant checks out of loops, separate fast and slow paths (`__builtin_expect`, `[[likely]]` in C++/GCC attributes; `likely()/unlikely()` macros in the kernel) so the slow path is laid out of line.
- Use table lookups, arithmetic masks (`x & -(cond)`), min/max, or SIMD selects for data-dependent decisions.
- Indirect calls and `switch` jump tables are predicted by target history; polymorphic call sites in hot loops can mispredict.
- Profile-guided optimization (`-fprofile-generate` / `-fprofile-use`) feeds real branch frequencies to the compiler for layout and inlining.

## Vectorization (SIMD)

CPUs have 128/256/512-bit registers processing 4–16 floats per instruction (SSE2 baseline on x86-64, AVX2, AVX-512, NEON/SVE on ARM). GCC auto-vectorizes simple loops at `-O3` (and `-O2` since GCC 12 with the very-cheap cost model) when it can prove it is safe: no loop-carried dependences, known aliasing (`restrict`), countable trip count. Floating-point addition is **not associative**, so a float reduction (`s += a[i]`) cannot be reordered into vector lanes unless you allow it (`-ffast-math` or `-fassociative-math -fno-signed-zeros -fno-trapping-math`):

```c
#include <stddef.h>
float sum_float(const float *a, size_t n) { float s = 0; for (size_t i = 0; i < n; i++) s += a[i]; return s; }
int   sum_int(const int *a, size_t n)     { int s = 0;   for (size_t i = 0; i < n; i++) s += a[i]; return s; }
void  saxpy(float *restrict y, const float *restrict x, float a, size_t n) { for (size_t i = 0; i < n; i++) y[i] += a * x[i]; }
```

```text
$ for f in "-O3" "-O3 -ffast-math"; do printf "%-16s sum_float: %s packed addps instruction(s) in the loop\n" "gcc $f" "$(gcc $f -S vec.c -o - | awk '/^sum_float:/,/\.size\tsum_float/' | grep -c addps)"; done; echo; echo 'integer reduction is exact, so no flag is needed:'; gcc -O3 -S vec.c -o - | awk '/^sum_int:/,/\.size\tsum_int/' | grep -cE 'paddd'; echo 'with -march=native (AVX2 + FMA available on this CPU):'; gcc -O3 -march=native -S vec.c -o - | grep -oE 'vfmadd[0-9a-z]+|vpaddd' | sort | uniq -c
gcc -O3          sum_float: 0 packed addps instruction(s) in the loop
gcc -O3 -ffast-math sum_float: 3 packed addps instruction(s) in the loop

integer reduction is exact, so no flag is needed:
3
with -march=native (AVX2 + FMA available on this CPU):
      1 vfmadd132ps
      1 vfmadd132ss
      1 vfmadd213ps
      2 vfmadd213ss
     10 vpaddd
```

Without `-ffast-math` the float sum compiled to scalar adds; with it, the loop uses packed `addps`. Integer sums vectorize freely, and `saxpy` with `restrict` compiled to FMA (fused multiply-add) vector instructions under `-march=native`. Practical advice:

- Check what happened: `-fopt-info-vec-optimized` / `-fopt-info-vec-missed`, or read the assembly (`objdump -d`, Compiler Explorer).
- Help the compiler: `restrict`, simple countable loops, no function calls or `break`s in the body, contiguous (SoA) data, aligned data (`alignas(32)`, `__builtin_assume_aligned`), `#pragma GCC ivdep`, `#pragma omp simd`.
- Explicit SIMD with intrinsics (`<immintrin.h>`: `_mm256_add_ps`) or portable vector extensions (`typedef float v8f __attribute__((vector_size(32)))`) when the compiler can't; guard by CPU feature (`__builtin_cpu_supports("avx2")`, `target_clones`) or compile separate variants and dispatch at runtime. Don't ship `-march=native` binaries to other machines (illegal-instruction crashes); target a baseline such as `x86-64-v2/v3`.
- Libraries (BLAS, `memcpy`, `strlen`, `simdjson`, Highway) already contain tuned kernels.

## Other levers

| Lever | What to do |
|---|---|
| **Inlining** | small hot functions: `static inline`; `-O2` decides, LTO inlines across files; `__attribute__((always_inline))`/`noinline` for control; avoid huge inlines that blow the instruction cache |
| **Allocation** | `malloc` costs ~20–100 ns and fragments; reuse buffers, arenas, pools, stack buffers for small sizes; avoid per-element allocation |
| **System calls** | batch (`readv`, big reads, `sendmmsg`), avoid per-byte syscalls (600× in the I/O note) |
| **Prefetching** | `__builtin_prefetch(&a[i + 16])` for irregular but predictable access (graph/tree traversals); measure, the hardware often wins |
| **Alignment** | align hot arrays to cache lines/pages; avoid cache-line splits |
| **Integer division and modulo** | ~20–40 cycles; use power-of-two sizes and masks, multiply by reciprocal, hoist invariants |
| **Floating-point** | prefer `float` for throughput (2× SIMD width), beware denormals (slow), `-fno-math-errno` to allow inlining `sqrt` |
| **Multithreading** | partition data to avoid sharing, use per-thread buffers, reduce locking, mind NUMA and false sharing; Amdahl's law bounds speedup by the serial fraction |
| **Compile-time work** | `constexpr`/macros/tables computed at build time, `static const` tables in `.rodata` |
| **Link/load** | `-fvisibility=hidden`, `-fno-plt`, static linking or prelinking for startup-sensitive programs |
| **PGO + LTO + BOLT** | typically 5–30 % free speedup for large programs (`-fprofile-use`, `-flto`, `llvm-bolt` for layout) |

## A short checklist

1. Is it algorithmically optimal? Can work be skipped, batched or cached?
2. What's the working set? Does it fit in L2/L3? Can types shrink (`uint32_t`, 16-bit indices, bitfields, quantization)?
3. Are accesses sequential and the fields used together stored together (SoA/hot-cold split)?
4. Are branches in the hot loop predictable? Is the loop vectorized? Check the compiler report.
5. Are threads sharing cache lines or locks? Is work evenly partitioned?
6. Did you measure with realistic data, optimized builds, repeated runs, and verify results are unchanged?

## Key takeaways

- Memory dominates: row-vs-column traversal differed ~50×, AoS vs SoA ~5×, false sharing ~18×; keep working sets small, access sequentially, and group data by use.
- Pointer chasing exposes the hierarchy (~3 ns in L1 to >200 ns in DRAM): avoid pointer-rich structures in hot paths.
- Unpredictable data-dependent branches cost a pipeline flush; the compiler's branchless conversions, sorting/batching and `likely` hints help.
- Auto-vectorization needs simple loops, `restrict`, SoA data, and for float reductions `-ffast-math`; check assembly or `-fopt-info`; target the right ISA without breaking portability.
- Always measure with a methodology that defeats compiler tricks and noise; optimize what the profiler says matters.
