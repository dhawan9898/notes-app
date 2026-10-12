---
title: Complexity Analysis and Amortized Analysis
track: dsa
module: Foundations
moduleOrder: 1
order: 1
level: intermediate
summary: How to reason about how algorithms scale — asymptotic notation, recurrences and the Master theorem, best/worst/average cases, amortized analysis, and the hidden O(n) traps in C — confirmed with operation counts rather than stopwatch noise.
sources:
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms (CLRS), 4th ed., Ch. 3 (Characterizing Running Times), Ch. 4 (Divide-and-Conquer), Ch. 16 (Amortized Analysis)
  - label: Knuth — The Art of Computer Programming, Vol. 1, §1.2.11 (Asymptotic representations)
  - label: Tarjan — Amortized Computational Complexity (SIAM J. Algebraic Discrete Methods, 1985)
  - label: Sleator & Tarjan — Amortized Efficiency of List Update and Paging Rules (CACM, 1985)
  - label: Bentley — Programming Pearls, 2nd ed., Ch. 8 (Algorithm Design Techniques)
  - label: Sedgewick & Wayne — Algorithms, 4th ed., Ch. 1.4 (Analysis of Algorithms)
---

**Complexity analysis** answers one question: *how does the cost of this algorithm grow as the input grows?* It lets you choose between designs on paper, predict whether a program will survive 100× more data, and spot accidental quadratic behavior before production does. Stopwatch measurements depend on the machine; counting operations does not — so this note validates every claim with **operation counts** from C programs, and uses timing only where the point is the real-world gap.

## What we measure

- **Input size `n`**: the quantity that drives cost — array length, number of nodes `V` and edges `E`, bits in a number, characters in a string. State it explicitly; for numbers, the size is `log n` bits, which is why trial-division primality (O(√n)) is *exponential* in the input size.
- **Cost model**: count a basic operation — comparisons for searching and sorting, memory accesses, edge relaxations, function calls — and assume it takes constant time (the word-RAM model: arithmetic on machine words and indexing are O(1)).
- **Which input**: the same algorithm has different costs on different inputs.

| Case | Meaning | Example (linear search) |
|---|---|---|
| **Best** | smallest cost over inputs of size `n` | key is first: 1 comparison |
| **Worst** | largest cost: a guarantee | key absent: `n` comparisons |
| **Average** | expected cost under a stated input distribution | key uniformly present: ≈ `(n+1)/2` |
| **Expected** | expectation over the algorithm's own random choices (randomized quicksort), for *any* input | O(n log n) |
| **Amortized** | average per operation over a worst-case *sequence* | dynamic array append: O(1) |

Worst case is the default for anything with deadlines or adversarial input (network packets, hash tables under attack); average/expected for batch work.

## Asymptotic notation

Let `f`, `g` be positive functions of `n`.

| Notation | Definition | Reads as |
|---|---|---|
| `f = O(g)` | ∃ c, n₀: `f(n) ≤ c·g(n)` for all n ≥ n₀ | `f` grows **no faster** than `g` (upper bound) |
| `f = Ω(g)` | ∃ c, n₀: `f(n) ≥ c·g(n)` | `f` grows **at least as fast** (lower bound) |
| `f = Θ(g)` | both O(g) and Ω(g) | **tight** bound |
| `f = o(g)` | `f/g → 0` | strictly slower |
| `f = ω(g)` | `f/g → ∞` | strictly faster |

Rules of thumb: drop constants and lower-order terms (`3n² + 50n + 7 = Θ(n²)`); `O` ignores the base of a logarithm (`log₂ n = Θ(log₁₀ n)`) but **not the base of an exponential** (`2ⁿ = O(3ⁿ)`, yet `3ⁿ` is not `O(2ⁿ)`); sums of costs take the max (sequence of an O(n) loop then an O(n²) loop = O(n²)); nested loops multiply; "O(n) time" says nothing about *which* input — say worst/average.

Common growth classes, with the time they take if one operation costs 1 ns (10⁹ ops/s):

| Class | n = 10³ | n = 10⁶ | n = 10⁹ | Typical algorithm |
|---|---|---|---|---|
| O(1) | 1 ns | 1 ns | 1 ns | array index, hash lookup (expected) |
| O(log n) | 10 ns | 20 ns | 30 ns | binary search, balanced BST, heap push |
| O(n) | 1 µs | 1 ms | 1 s | scan, counting sort |
| O(n log n) | 10 µs | 20 ms | 30 s | merge sort, heap sort, FFT |
| O(n²) | 1 ms | 17 min | 32 years | bubble/insertion sort, naive all-pairs |
| O(n³) | 1 s | 32 years | – | naive matrix multiply, Floyd–Warshall |
| O(2ⁿ) | 10²⁸⁸ years | impossible | – | subset enumeration, naive recursive Fibonacci |
| O(n!) | – | – | – | brute-force permutations (TSP) |

The table's message: for n = 10⁶, O(n log n) is interactive and O(n²) is not. For n = 10, almost anything works.

## Counting operations: five algorithms, measured

`counts.c` instruments a global counter and runs an O(n) linear search (key absent), an O(log n) binary search, an O(n²) bubble sort (worst case, reversed input), an O(n log n) merge sort, and two Fibonacci implementations:

{{include dsa/complexity/counts.c}}

{{run dsa/complexity :: gcc -O2 -Wall -Wextra counts.c -o counts -lm && ./counts}}

Reading the columns as `n` doubles (1000 → 2000 → 4000 …):

- **linear** doubles exactly (n comparisons): Θ(n).
- **binary** gains **one** step per doubling (10, 11, 12, …): Θ(log n); searching 16,000 items took 14 comparisons — a million items would take 20.
- **bubble** quadruples each time (≈ n²/2: 499,500 → 1,999,000 → 7,998,000): Θ(n²). Doubling the input costs 4×.
- **mergesort** slightly more than doubles and tracks `n·log₂n` (the right-hand column): Θ(n log n); at n = 16,000 it uses 112,704 comparisons against bubble's 127,992,000 — about **1,100× fewer**.
- **Fibonacci**: the naive recursion makes 29.9 million calls for n = 35 (growing ≈ 1.618× per step — exponential, Θ(φⁿ)); memoization makes **69** (linear). Same function, same answer; the difference is whether subproblems are recomputed. This is the entire premise of dynamic programming.

The doubling test is a practical method: run at `n`, `2n`, `4n` and look at the ratio of costs: ×1 → O(1), +const → O(log n), ×2 → O(n), ×2.1 → O(n log n), ×4 → O(n²), ×8 → O(n³), squared → exponential.

## Recurrences and the Master theorem

Divide-and-conquer algorithms are described by recurrences `T(n) = a·T(n/b) + f(n)`: split into `a` subproblems of size `n/b`, then do `f(n)` work to divide/combine. Compare `f(n)` with `n^(log_b a)` (the work at the leaves):

| Case | Condition | Result |
|---|---|---|
| 1 | `f(n) = O(n^(log_b a − ε))` — leaves dominate | `T(n) = Θ(n^(log_b a))` |
| 2 | `f(n) = Θ(n^(log_b a))` — balanced | `T(n) = Θ(n^(log_b a) · log n)` |
| 3 | `f(n) = Ω(n^(log_b a + ε))` and regularity — root dominates | `T(n) = Θ(f(n))` |

| Algorithm | Recurrence | a, b, f | Result |
|---|---|---|---|
| Binary search | `T(n) = T(n/2) + 1` | 1, 2, 1 → case 2 | Θ(log n) |
| Merge sort | `T(n) = 2T(n/2) + n` | 2, 2, n → case 2 | Θ(n log n) |
| Tree traversal (balanced) | `T(n) = 2T(n/2) + 1` | 2, 2, 1 → case 1 | Θ(n) |
| Karatsuba multiplication | `T(n) = 3T(n/2) + n` | 3, 2, n → case 1 | Θ(n^1.585) |
| Strassen matrix multiply | `T(n) = 7T(n/2) + n²` | 7, 2, n² → case 1 | Θ(n^2.807) |
| Quickselect (average) | `T(n) = T(n/2) + n` | 1, 2, n → case 3 | Θ(n) |
| Naive Fibonacci | `T(n) = T(n−1) + T(n−2) + 1` | not of Master form | Θ(φⁿ) |

When the Master theorem doesn't apply (unequal splits like quicksort's `T(n) = T(k) + T(n−k−1) + n`), draw the recursion tree: sum the work per level times the number of levels (balanced split → log n levels × n = n log n; worst split → n levels × n = n²), or use substitution and induction.

Merge sort's measured comparison count lies between `½·n·log₂n` and `n·log₂n − n + 1` for every input, matching the Θ(n log n) bound exactly.

## Lower bounds and optimality

Upper bounds come from algorithms; **lower bounds** come from arguments about *every* possible algorithm in a model. The decision-tree argument: a comparison sort must distinguish `n!` permutations with binary comparisons, so it needs at least `⌈log₂ n!⌉ = Ω(n log n)` comparisons. Merge sort and heap sort are therefore asymptotically **optimal** comparison sorts. Counting sort and radix sort beat this bound only by not comparing (they use key structure). Other classic bounds: searching a sorted array Ω(log n), finding the maximum Ω(n), element distinctness Ω(n log n) in the comparison model.

## Amortized analysis

Some operations are usually cheap but occasionally expensive (a dynamic array append that triggers a resize). The **worst case** for one append is O(n), yet any *sequence* of `n` appends costs O(n) total, i.e. **O(1) amortized** per append. Amortized analysis is *not* average-case over random inputs — it is a **worst-case guarantee over a sequence** with no probability involved.

Three standard proof techniques, shown on the doubling array (capacity doubles when full; copying `k` elements costs `k`):

1. **Aggregate**: resizes happen at sizes 1, 2, 4, …, so the total copying over `n` appends is `1 + 2 + 4 + … + n/2 < n`; total work `< n + n = 2n`, so the amortized cost is `< 2` per append.
2. **Accounting (banker's)**: charge each append 3 "coins": 1 pays for storing it, 2 are saved on the element. When a resize copies `k` elements, the `k/2` newest elements each saved 2 coins = `k` coins pay for the copy. The balance never goes negative, so 3 coins per append suffice.
3. **Potential**: define `Φ = 2·size − capacity ≥ 0`; an ordinary append raises Φ by 2 (amortized cost 1 + 2 = 3); a resizing append drops Φ by `size` (cancelling the real cost `size`). Amortized cost ≤ 3.

The growth factor matters. Counting element copies for one million appends (`amortized.c`):

{{include dsa/complexity/amortized.c}}

{{run dsa/complexity :: gcc -O2 -Wall -Wextra amortized.c -o amortized && ./amortized}}

- **Doubling** copies 1.05 elements per append (the geometric sum), a single resize can still copy 524,287 elements (a **latency spike**, important for real-time systems — it is amortized O(1), not worst-case O(1)).
- A factor of **1.5** costs 2.1 copies per append (but frees old blocks for reuse); **1.1** costs 10.
- **Adding a constant** (+16 or +1024) is O(n) amortized: copies per append *grow* with n (4.6 → 48.7 → 488 → 4,883 per append as n grows 10×); +16 needed 31 billion copies for a million appends.
- The doubling rows show the bound is flat as `n` scales from a thousand to 10 million (1.01 → 1.68, converging toward 2).

Other amortized structures you will meet: hash table rehashing (amortized O(1) insert), splay trees (amortized O(log n)), union-find with path compression (inverse-Ackermann α(n) amortized), binary counters (increment is amortized O(1): the total number of bit flips over `n` increments is `< 2n`), Fibonacci heaps, queues built from two stacks (each element is moved at most twice), and garbage collectors.

## Space complexity

Count the **extra** memory beyond the input: O(1) for in-place algorithms (heap sort, two-pointer loops), O(n) for merge sort's buffer, O(log n) for quicksort's recursion stack, O(depth) for DFS. **Stack depth counts**: recursive code with depth `n` is O(n) space and, in C, a crash risk at ~30,000–100,000 frames on an 8 MiB stack (see the stack-frames note). Time/space trade-offs are everywhere: memoization, hash indexes, lookup tables, precomputed prefix sums trade memory for speed.

## When asymptotics mislead

1. **Constants and lower-order terms**: an O(n log n) algorithm with a large constant can lose to an O(n²) one for small n — which is why production sorts switch to insertion sort for runs of ~16–32 elements (introsort, Timsort).
2. **Memory hierarchy**: the RAM model treats every access as O(1), but a cache miss costs 100× a hit. A linked list and an array both traverse in O(n); the array is typically 5–50× faster (see the C performance note). Cache-oblivious and cache-aware algorithms (B-trees, blocking) win by minimizing misses.
3. **Big inputs but small exponents**: Θ(n log n) vs Θ(n): the log factor is ~20 for a million items; sometimes the Θ(n) algorithm with a worse constant loses.
4. **Hidden costs**: operations the model calls O(1) may not be (see below), and `malloc`, system calls, locks and page faults are much more expensive than arithmetic.
5. **Randomized/average-case behavior** matters when worst-case input is rare (quicksort) — and when it is *not* rare, as with adversarial inputs to hash tables (hash flooding).
6. **Parallelism**: work vs depth (span); Amdahl's law bounds speedup by the serial fraction.

## Hidden O(n) in everyday C

Two classic accidental quadratics, measured (`hidden.c`):

{{include dsa/complexity/hidden.c}}

{{run dsa/complexity :: gcc -O2 -Wall -Wextra hidden.c -o hidden && ./hidden}}

- `for (i = 0; i < strlen(s); i++) s[i] = toupper(s[i]);` calls `strlen` — which is O(n) — on **every iteration**. The compiler cannot hoist it because the loop writes through `s` and might change the terminator. Result: doubling the input costs ~4–5×; hoisting `n = strlen(s)` makes it linear (thousands of times faster at 400,000 characters).
- Repeated `strcat(dst, piece)` re-scans the growing destination to find its end on every call: building a string from `p` pieces is O(p²). Tracking the length (or using `memcpy` at a known offset, or a string-builder) is O(p).

Other traps: `memmove`/array insertion at the front in a loop (O(n²)); `list.length()` on a singly linked list inside a loop; `realloc` by constant increments; looking up in a list or array repeatedly instead of building a hash set (O(n·m) vs O(n+m)); string comparison cost in sorts of long strings (O(L) per comparison); recomputing a value that could be hoisted or cached; `printf` in a hot loop; reading a file one byte at a time (syscalls; see the I/O note).

## A practical method

1. Write the dominant loop(s) and count how many times the innermost statement runs as a function of `n` (and `V`, `E`, `m`... when several inputs exist; state complexities like `O(V + E)`, `O(m·n)`).
2. Check the hidden costs of library calls (`strlen`, `strcat`, `qsort`'s comparisons, `malloc`, hash operations).
3. Sanity-check with the doubling test using operation counters, then with real timings on realistic data and **large** `n` (cache effects appear at sizes you did not test).
4. Prefer the simplest algorithm that meets the constraint; document the complexity and the assumption (input distribution, load factor, key hashing) next to the code.
5. For security-sensitive code, analyze the worst case against an adversary, not the average.

## Key takeaways

- Asymptotic notation (O, Ω, Θ) describes growth, dropping constants; always say which case (worst, average, expected, amortized) and what `n` is.
- Operation counts confirmed the classes: linear search ×2 per doubling, binary search +1, bubble sort ×4, merge sort ≈ n log₂ n (1,100× fewer comparisons than bubble sort at n = 16,000), naive Fibonacci exponential vs memoized linear.
- The Master theorem solves `T(n) = aT(n/b) + f(n)`; the comparison-sort lower bound Ω(n log n) comes from the `n!`-leaf decision tree.
- Amortized analysis (aggregate, accounting, potential) bounds a *sequence*: doubling a dynamic array costs ~1–2 copies per append, while growing by a constant is quadratic overall — but a single resize is still a latency spike.
- Watch for hidden linear work in loop conditions and string building; the memory hierarchy can override asymptotics at practical sizes.
