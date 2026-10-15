---
title: "Performance-Oriented C: Caches, Branches, Data Layout and Vectorization"
track: c
module: Systems-Level C
moduleOrder: 7
order: 4
level: expert
summary: How to reason about and measure C performance on modern CPUs — the memory hierarchy, branch prediction, data layout (AoS vs SoA), false sharing and vectorization — each demonstrated with a benchmark whose results span 5× to 50×.
prerequisites:
  - Structs, Alignment, Padding, Unions, Bit-fields and Endianness
  - Bit Manipulation
sources:
  - label: Drepper — What Every Programmer Should Know About Memory (2007)
    url: https://people.freebsd.org/~lstewart/articles/cpumemory.pdf
  - label: Fog — Optimizing software in C++; Instruction tables; Microarchitecture (agner.org)
    url: https://www.agner.org/optimize/
  - label: Intel 64 and IA-32 Architectures Optimization Reference Manual
  - label: Hennessy & Patterson — Computer Architecture, A Quantitative Approach (memory hierarchy, ILP)
  - label: Gregg — Systems Performance, 2nd ed.; Mytkowicz et al. — Producing Wrong Data Without Doing Anything Obviously Wrong! (ASPLOS 2009)
  - label: GCC manual — Optimize Options; auto-vectorization (-fopt-info-vec); __builtin_prefetch, __builtin_expect
    url: https://gcc.gnu.org/projects/tree-ssa/vectorization.html
---

C gives control over memory layout and instructions, which is why it dominates performance-critical software. But modern CPUs execute billions of instructions per second only when the data is in cache, branches are predictable, and independent work is available. Most speedups come from **doing less work and touching less memory**, not from clever arithmetic. The experiments below were run on an 8-core AMD EPYC virtual machine with GCC 14; absolute numbers vary by hardware, ratios are what matter.

## Rule zero: measure

1. **Profile first** (`perf record`, flame graphs; see the Linux profiling note). Optimize the code that dominates, not the code you suspect.
2. **Algorithms and data structures first**: O(n²) → O(n log n) beats any micro-optimization.
3. Use realistic data and sizes; cache effects change with input size.
4. Prevent the compiler from deleting your benchmark: it removes unused computations, hoists loop-invariant work and folds constants. A `DO_NOT_OPTIMIZE(v)` barrier (`asm volatile("" : : "r"(v) : "memory")`) forces the value to be computed:

{{include c/perf/timing.h}}

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

{{include c/perf/cache.c}}

{{run c/perf :: gcc -O2 cache.c -o cache && ./cache}}

- Same arithmetic, same result: the column walk is ~**50× slower** because every access misses cache and TLB, and the prefetcher can't help with the large stride. C arrays are row-major, so *the last index should vary fastest* in the inner loop. Loop interchange and **blocking/tiling** (process 64×64 tiles) are the standard fixes for matrix code.
- The pointer-chasing latencies reveal the hierarchy: ~**3 ns** while the set fits in L1 (4–32 KiB), ~8 ns in L2 (256 KiB), ~24 ns in L3 (2 MiB), ~**93–212 ns** once it spills out of cache into DRAM (plus TLB misses: 4 KiB pages mean large sets need **huge pages**, see the Linux memory notes). Random access to large data is two orders of magnitude slower than to small data: shrink the working set (smaller types, compact structures, indices instead of pointers, sorting/batching accesses).

### Experiment 2: data layout (AoS vs SoA)

A loop that needs only the `x` field of 4 million particles, stored as an **array of structs** (48-byte records) or a **struct of arrays**:

{{include c/perf/layout.c}}

{{run c/perf :: gcc -O2 layout.c -o layout && ./layout}}

The AoS loop drags 48 bytes per record through the cache to use 4 (12 % useful), so it is memory-bound; SoA reads contiguous floats (100 % useful) and the compiler can **vectorize** it. Guidelines: store hot and cold fields separately; order struct members for packing (previous structs note); prefer SoA or hybrid (AoSoA) for bulk numeric processing, AoS when you access whole records; use indices (`uint32_t`) instead of 8-byte pointers to halve memory; allocate objects of one kind contiguously (arenas/pools) instead of one `malloc` each; linked lists and pointer-rich trees are cache-hostile: prefer arrays, B-trees, open-addressing hash tables.

### Experiment 3: false sharing

Four threads each increment *their own* counter 50 million times. In one version the counters sit side by side (one 64-byte line); in the other each has its own line (`alignas(64)`):

{{include c/perf/falseshare.c}}

{{run c/perf :: gcc -O2 -pthread falseshare.c -o falseshare && ./falseshare}}

No data is shared, but cores must own a cache line exclusively to write it, so the line **ping-pongs** between cores via the coherence protocol: ~**18× slower**. Pad or align per-thread data to the cache-line size (`alignas(64)`, `_Alignas`; 128 on some ARM/Apple/POWER chips), use thread-local accumulators merged at the end, and measure with `perf c2c`. The same effect hits arrays of locks, per-CPU statistics, and adjacent fields written by different threads.

## Branches

CPUs predict branches and execute speculatively; a misprediction flushes the pipeline (~15–20 cycles). Predictable branches are free; **data-dependent unpredictable** branches are expensive. The classic demonstration sums elements ≥ 128 of 16 million random bytes, unsorted versus sorted (identical work). GCC at `-O1` and above often converts the branch into a conditional move on its own, hiding the effect, so the experiment disables **if-conversion** to show the raw branch behaviour:

{{include c/perf/branch.c}}

{{run c/perf :: gcc -O1 -fno-if-conversion -fno-if-conversion2 branch.c -o branch && ./branch}}

With the real branch, the unsorted data runs several times slower than the sorted data (the predictor guesses right ~50 % of the time on random data, ~100 % on sorted data); the branchless form is fast on both. Lessons:

- Let the compiler pick (it uses `cmov`/`setcc`/SIMD blends when profitable); check the assembly of hot loops.
- Make branches predictable: sort or partition data, process in batches by category, hoist invariant checks out of loops, separate fast and slow paths (`__builtin_expect`, `[[likely]]` in C++/GCC attributes; `likely()/unlikely()` macros in the kernel) so the slow path is laid out of line.
- Use table lookups, arithmetic masks (`x & -(cond)`), min/max, or SIMD selects for data-dependent decisions.
- Indirect calls and `switch` jump tables are predicted by target history; polymorphic call sites in hot loops can mispredict.
- Profile-guided optimization (`-fprofile-generate` / `-fprofile-use`) feeds real branch frequencies to the compiler for layout and inlining.

## Vectorization (SIMD)

CPUs have 128/256/512-bit registers processing 4–16 floats per instruction (SSE2 baseline on x86-64, AVX2, AVX-512, NEON/SVE on ARM). GCC auto-vectorizes simple loops at `-O3` (and `-O2` since GCC 12 with the very-cheap cost model) when it can prove it is safe: no loop-carried dependences, known aliasing (`restrict`), countable trip count. Floating-point addition is **not associative**, so a float reduction (`s += a[i]`) cannot be reordered into vector lanes unless you allow it (`-ffast-math` or `-fassociative-math -fno-signed-zeros -fno-trapping-math`):

{{include c/perf/vec.c}}

{{run c/perf :: for f in "-O3" "-O3 -ffast-math"; do printf "%-16s sum_float: %s packed addps instruction(s) in the loop\n" "gcc $f" "$(gcc $f -S vec.c -o - | awk '/^sum_float:/,/\.size\tsum_float/' | grep -c addps)"; done; echo; echo 'integer reduction is exact, so no flag is needed:'; gcc -O3 -S vec.c -o - | awk '/^sum_int:/,/\.size\tsum_int/' | grep -cE 'paddd'; echo 'with -march=native (AVX2 + FMA available on this CPU):'; gcc -O3 -march=native -S vec.c -o - | grep -oE 'vfmadd[0-9a-z]+|vpaddd' | sort | uniq -c}}

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
