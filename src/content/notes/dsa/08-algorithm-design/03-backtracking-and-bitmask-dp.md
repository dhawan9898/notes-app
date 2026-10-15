---
title: Backtracking, Branch and Bound, and Bitmask DP
track: dsa
module: Algorithm Design
moduleOrder: 8
order: 3
level: advanced
summary: "Systematic search with pruning — N-Queens with bitmasks, subset-sum bounds, a Sudoku solver with the fewest-candidates heuristic, permutation and subset enumeration (Gosper's hack), and the Held-Karp bitmask DP for the travelling salesman problem — with node counts and timings."
prerequisites:
  - Dynamic Programming
  - Bit Manipulation
sources:
  - label: "Knuth — The Art of Computer Programming, Vol. 4A, §7.2.2 (Backtrack programming) and §7.1.3 (Bitwise tricks and techniques)"
  - label: "Knuth — Dancing Links (in Millennial Perspectives in Computer Science, 2000)"
  - label: "Held & Karp — A Dynamic Programming Approach to Sequencing Problems (J. SIAM 10(1), 1962)"
  - label: "Bellman — Dynamic Programming Treatment of the Travelling Salesman Problem (J. ACM 9(1), 1962)"
  - label: "Land & Doig — An Automatic Method of Solving Discrete Programming Problems (Econometrica 28(3), 1960)"
  - label: "Russell & Norvig — Artificial Intelligence, A Modern Approach, Ch. 6 (Constraint Satisfaction Problems)"
  - label: "Warren — Hacker's Delight, 2nd ed., Ch. 2 (Basics) and Ch. 15 (Gosper's hack, next subset of fixed size)"
  - label: "OEIS A000170 — Number of ways of placing n nonattacking queens on an n X n board"
    url: https://oeis.org/A000170
---

Some problems have no known polynomial algorithm: place `n` non-attacking queens, solve a Sudoku, pick a subset with a given sum, visit every city once. The honest approach is **search**, and the art is cutting the search tree down. This note covers the two ways of doing that: *prune* (backtracking, branch and bound — abandon a partial solution as soon as it can't succeed) and *share* (bitmask DP — recognise that different partial solutions lead to the same sub-problem and compute it once).

## The backtracking template

Backtracking builds a solution one decision at a time and **undoes** a decision when it leads nowhere:

```text
solve(partial):
    if partial is complete:  record it; return
    for each choice that is consistent with partial:     # <- pruning happens here
        apply choice
        solve(partial)
        undo choice                                       # <- "backtrack"
```

Three things decide whether it is fast enough:

1. **How early you detect failure** — check constraints incrementally and reject a branch the moment it is infeasible, not at the leaves.
2. **Order of decisions and choices** — decide the most constrained thing first ("fail-first"), and try the most promising value first.
3. **Cost per node** — the search tree is the same however you implement it; the per-node cost is where bit tricks pay off.

## N-Queens: same tree, 4–20× cheaper nodes

Place `n` queens on an `n×n` board with no two sharing a row, column or diagonal. Place one queen per row; the only choice is the column. The textbook version stores the columns placed so far and checks each candidate against every earlier queen: `O(n)` per placement. The bitmask version keeps three bitsets — occupied columns, and the two diagonal directions shifted one step per row — so the set of *safe* squares in the current row is a single expression, `~(cols | d1 | d2) & full`, and iterating over its set bits (`bit = avail & -avail`) visits exactly the legal placements.

The two programs visit the **identical search tree** (same node counts, verified by the assertion that the solution counts match, and printed side by side), so the speed difference is purely per-node cost. The known solution counts (OEIS A000170) are the check.

```c
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

/* ===== N-Queens #1: array of column positions, O(n) safety test per placement ===== */
static unsigned long long nodes_a;
static int safe(const int *q, int row, int col) { for (int r = 0; r < row; r++) if (q[r] == col || abs(q[r] - col) == row - r) return 0; return 1; }
static long queens_array(int *q, int row, int n) {
    nodes_a++;
    if (row == n) return 1;
    long c = 0; for (int col = 0; col < n; col++) if (safe(q, row, col)) { q[row] = col; c += queens_array(q, row + 1, n); }
    return c;
}
/* ===== N-Queens #2: three bitmasks (columns, / diagonals, \ diagonals); the set of safe squares in a row is ONE expression ===== */
static unsigned long long nodes_b;
static long queens_bits(unsigned cols, unsigned d1, unsigned d2, unsigned full) {
    nodes_b++;
    if (cols == full) return 1;
    long c = 0; unsigned avail = full & ~(cols | d1 | d2);
    while (avail) { unsigned bit = avail & -avail; avail ^= bit; c += queens_bits(cols | bit, ((d1 | bit) << 1) & full, (d2 | bit) >> 1, full); }
    return c;
}

/* ===== subset-sum with and without pruning: count subsets of a[] (positive ints) summing to target ===== */
static unsigned long long nodes_plain, nodes_pruned;
static long ss_plain(const int *a, int n, int i, int sum, int target) { nodes_plain++; if (i == n) return sum == target; return ss_plain(a, n, i + 1, sum, target) + ss_plain(a, n, i + 1, sum + a[i], target); }
static long ss_pruned(const int *a, const long *suffix, int n, int i, int sum, int target) {
    nodes_pruned++;
    if (sum == target) return 1;                                              /* items are positive, so the only way to extend a hit is to exclude everything left */
    if (i == n || sum > target || sum + suffix[i] < target) return 0;         /* overshoot, or even taking everything left cannot reach the target */
    return ss_pruned(a, suffix, n, i + 1, sum, target) + ss_pruned(a, suffix, n, i + 1, sum + a[i], target);
}

/* ===== enumerations ===== */
static void next_perm_demo(void) {                                            /* lexicographic next permutation: the standard in-place algorithm */
    int p[4] = { 1, 2, 3, 4 }; int count = 0;
    for (;;) {
        count++;
        int i = 2; while (i >= 0 && p[i] >= p[i + 1]) i--;
        if (i < 0) break;
        int j = 3; while (p[j] <= p[i]) j--;
        int t = p[i]; p[i] = p[j]; p[j] = t;
        for (int a = i + 1, b = 3; a < b; a++, b--) { t = p[a]; p[a] = p[b]; p[b] = t; }
    }
    printf("next_permutation visited %d permutations of 4 elements (4! = 24)\n", count);
}
static void gosper_demo(void) {                                               /* all 3-element subsets of {0..4} as bitmasks, in increasing order, via Gosper's hack */
    unsigned x = 0x7, limit = 1u << 5; printf("Gosper's hack, 3-subsets of 5 elements:");
    int count = 0; while (x < limit) { printf(" %02x", x); count++; unsigned c = x & -x, r = x + c; x = (((r ^ x) >> 2) / c) | r; }
    printf("  (%d = C(5,3))\n", count);
}

int main(void) {
    printf("%3s %12s | %14s %9s | %14s %9s | %6s\n", "n", "solutions", "array: nodes", "ms", "bitmask: nodes", "ms", "speedup");
    for (int n = 4; n <= 13; n++) {
        int q[16]; nodes_a = 0; double t0 = now(); long a = queens_array(q, 0, n); double ta = now() - t0;
        nodes_b = 0; t0 = now(); long b = queens_bits(0, 0, 0, (1u << n) - 1); double tb = now() - t0;
        assert(a == b);
        printf("%3d %12ld | %14llu %9.1f | %14llu %9.1f | %5.0fx\n", n, a, nodes_a, ta * 1e3, nodes_b, tb * 1e3, tb > 0 ? ta / tb : 0.0);
    }
    printf("(known solution counts: 4->2, 5->10, 6->4, 7->40, 8->92, 9->352, 10->724, 11->2680, 12->14200, 13->73712)\n\n");

    /* pruning: 28 positive numbers, target near the middle */
    int a[28]; uint64_t s = 88172645463325252ULL; for (int i = 0; i < 28; i++) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; a[i] = 1 + (int)((s >> 11) % 100); }
    /* sort descending: large items first make the bounds bite sooner */
    for (int i = 0; i < 28; i++) for (int j = i + 1; j < 28; j++) if (a[j] > a[i]) { int t = a[i]; a[i] = a[j]; a[j] = t; }
    long suffix[29]; suffix[28] = 0; for (int i = 27; i >= 0; i--) suffix[i] = suffix[i + 1] + a[i];
    int target = (int)(suffix[0] / 2);
    nodes_plain = 0; long p1 = ss_plain(a, 28, 0, 0, target);
    nodes_pruned = 0; long p2 = ss_pruned(a, suffix, 28, 0, 0, target);
    assert(p1 == p2);
    printf("subset sum, 28 positive numbers, target = half the total (%d):\n", target);
    printf("  no pruning : %ld subsets found, %llu search nodes\n", p1, nodes_plain);
    printf("  with bounds: %ld subsets found, %llu search nodes  (%.0fx fewer)\n\n", p2, nodes_pruned, (double)nodes_plain / (double)nodes_pruned);
    next_perm_demo(); gosper_demo();
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra queens.c -o queens && ./queens
  n    solutions |   array: nodes        ms | bitmask: nodes        ms | speedup
  4            2 |             17       0.0 |             17       0.0 |     4x
  5           10 |             54       0.0 |             54       0.0 |     6x
  6            4 |            153       0.0 |            153       0.0 |     8x
  7           40 |            552       0.1 |            552       0.0 |     9x
  8           92 |           2057       0.3 |           2057       0.1 |     6x
  9          352 |           8394       1.5 |           8394       0.1 |    12x
 10          724 |          35539       7.4 |          35539       0.6 |    13x
 11         2680 |         166926      38.0 |         166926       2.6 |    14x
 12        14200 |         856189     211.7 |         856189      13.7 |    15x
 13        73712 |        4674890    1239.8 |        4674890      69.0 |    18x
(known solution counts: 4->2, 5->10, 6->4, 7->40, 8->92, 9->352, 10->724, 11->2680, 12->14200, 13->73712)

subset sum, 28 positive numbers, target = half the total (706):
  no pruning : 686541 subsets found, 536870911 search nodes
  with bounds: 686541 subsets found, 16210383 search nodes  (33x fewer)

next_permutation visited 24 permutations of 4 elements (4! = 24)
Gosper's hack, 3-subsets of 5 elements: 07 0b 0d 0e 13 15 16 19 1a 1c  (10 = C(5,3))
```

- Solution counts match OEIS A000170 for every `n` (2, 10, 4, 40, 92, … 73,712 at `n = 13`).
- The node counts of the two implementations are **equal in every row**; the bitmask version is roughly an order of magnitude faster per row and the gap widens with `n`, because the array version's `O(n)` safety test grows while the bitmask test stays one machine instruction. This is the whole point of the C track's *Bit Manipulation* note applied to search.
- The tree itself grows by about 5× per added row (17 nodes at `n = 4` → 4.7 million at `n = 13`); no implementation trick changes that growth rate, which is why `n` beyond ~15 needs symmetry reduction (halve by reflection) or different algorithms.

## Branch and bound: pruning with a bound

For *optimisation* problems, keep the best solution found so far and prune any branch whose **optimistic estimate** cannot beat it. The estimate must never be pessimistic (otherwise you'd discard the optimum). Two instances are in the programs here:

- **Subset sum with positive numbers.** Sorting the numbers in decreasing order and keeping suffix sums gives two cut-offs: stop when the running sum already exceeds the target, or when even taking *everything remaining* cannot reach it. On 28 numbers with the target at half the total, the plain recursion visits all `2^29 − 1` ≈ 537 million nodes; the bounded one visits about 33× fewer in the run below, while finding the same number of subsets — asserted equal.
- **Travelling salesman with a length bound.** The brute-force tour enumeration below abandons a partial tour as soon as its length reaches the best complete tour found so far.

### Constraint satisfaction: Sudoku

Sudoku is a constraint-satisfaction problem: each empty cell must take a digit not used in its row, column or box. With three 9-bit masks per unit, the candidate set of any cell is `~(row | col | box)` — one expression — and the solver recurses over the candidates of one chosen cell. The key decision is *which cell*. The naive solver takes the next empty cell in reading order; the **MRV** ("minimum remaining values", fail-first) heuristic takes the cell with the fewest candidates, which both detects dead ends sooner (zero candidates → backtrack at once) and makes forced moves (one candidate) free.

```c
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../c/perf/timing.h"

/* Sudoku as constraint satisfaction: bitmasks of used digits per row/column/box; choose the empty cell with the FEWEST candidates (MRV heuristic). */
static int grid[81], row_m[9], col_m[9], box_m[9];
static unsigned long long nodes;
static int use_mrv;
#define BOX(i) (((i) / 27) * 3 + ((i) % 9) / 3)

static void place(int i, int d) { int bit = 1 << d; row_m[i / 9] |= bit; col_m[i % 9] |= bit; box_m[BOX(i)] |= bit; grid[i] = d; }
static void unplace(int i, int d) { int bit = ~(1 << d); row_m[i / 9] &= bit; col_m[i % 9] &= bit; box_m[BOX(i)] &= bit; grid[i] = 0; }
static int cands(int i) { return 0x3FE & ~(row_m[i / 9] | col_m[i % 9] | box_m[BOX(i)]); }          /* bits 1..9 */

static int solve(int empties) {
    nodes++;
    if (!empties) return 1;
    int best = -1, bestc = 10;
    for (int i = 0; i < 81; i++) if (!grid[i]) {
        int c = __builtin_popcount(cands(i));
        if (!use_mrv) { best = i; bestc = c; break; }                       /* naive: first empty cell in reading order */
        if (c < bestc) { best = i; bestc = c; if (c <= 1) break; }
    }
    if (bestc == 0) return 0;                                                /* dead end: some cell has no candidate */
    for (int m = cands(best); m; m &= m - 1) {
        int d = __builtin_ctz(m); place(best, d);
        if (solve(empties - 1)) return 1;
        unplace(best, d);
    }
    return 0;
}
static int load(const char *s) {
    memset(grid, 0, sizeof grid); memset(row_m, 0, sizeof row_m); memset(col_m, 0, sizeof col_m); memset(box_m, 0, sizeof box_m); int empties = 0;
    for (int i = 0; i < 81; i++) { if (s[i] >= '1' && s[i] <= '9') place(i, s[i] - '0'); else empties++; }
    return empties;
}
static int valid_solution(void) {
    for (int u = 0; u < 9; u++) { int r = 0, c = 0, b = 0; for (int k = 0; k < 9; k++) { r |= 1 << grid[u * 9 + k]; c |= 1 << grid[k * 9 + u]; b |= 1 << grid[(u / 3) * 27 + (u % 3) * 3 + (k / 3) * 9 + k % 3]; } if (r != 0x3FE || c != 0x3FE || b != 0x3FE) return 0; }
    return 1;
}

int main(void) {
    static const struct { const char *name, *puzzle; } tests[] = {
        { "easy",                    "53..7....6..195....98....6.8...6...34..8.3..17...2...6.6....28....419..5....8..79" },
        { "hard (Inkala, 2012)",     "8..........36......7..9.2...5...7.......457.....1...3...1....68..85...1..9....4.." },
        { "17 clues (sparse)",      "...............3.85..1.2.......5.7.....4...1...9.......5......73..2.1........4...9" },
    };
    printf("%-22s %-16s %14s %10s %s\n", "puzzle", "cell choice", "search nodes", "ms", "result");
    for (int t = 0; t < 3; t++) for (int mrv = 0; mrv < 2; mrv++) {
        int e = load(tests[t].puzzle); use_mrv = mrv; nodes = 0; double t0 = now(); int ok = solve(e); double dt = now() - t0;
        printf("%-22s %-16s %14llu %10.2f %s\n", tests[t].name, mrv ? "fewest candidates" : "reading order", nodes, dt * 1e3, ok && valid_solution() ? "solved, verified" : "FAILED");
    }
    load(tests[1].puzzle); use_mrv = 1; nodes = 0; solve(81 - 21);
    printf("\nsolution of the hard puzzle:\n"); for (int r = 0; r < 9; r++) { for (int c = 0; c < 9; c++) printf("%d%s", grid[r * 9 + c], c % 3 == 2 && c < 8 ? " | " : " "); printf("\n"); if (r % 3 == 2 && r < 8) printf("------+-------+------\n"); }
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -Wall -Wextra sudoku.c -o sudoku && ./sudoku
puzzle                 cell choice        search nodes         ms result
easy                   reading order              4209       0.23 solved, verified
easy                   fewest candidates             52       0.01 solved, verified
hard (Inkala, 2012)    reading order             49559       3.11 solved, verified
hard (Inkala, 2012)    fewest candidates          13811       3.05 solved, verified
17 clues (sparse)      reading order            521082      26.54 solved, verified
17 clues (sparse)      fewest candidates            355       0.09 solved, verified

solution of the hard puzzle:
8 1 2 | 7 5 3 | 6 4 9 
9 4 3 | 6 8 2 | 1 7 5 
6 7 5 | 4 9 1 | 2 8 3 
------+-------+------
1 5 4 | 2 3 7 | 8 9 6 
3 6 9 | 8 4 5 | 7 2 1 
2 8 7 | 1 6 9 | 5 3 4 
------+-------+------
5 2 1 | 9 7 4 | 3 6 8 
4 3 8 | 5 2 6 | 9 1 7 
7 9 6 | 3 1 8 | 4 5 2 
```

The same solver with only a change in cell ordering visits 4× to 1,500× fewer nodes: for the sparse 17-clue grid, reading order needs hundreds of thousands of nodes and MRV a few hundred. Every result is checked by an independent validity test (each row, column and box contains 1–9 exactly once). For harder constraint problems, **constraint propagation** (after placing a digit, eliminate it from the peers' candidates and apply "naked/hidden single" rules) shrinks the tree further, and **Dancing Links** (Knuth's Algorithm X on the exact-cover formulation) is the classic general-purpose engine for Sudoku, polyomino tiling and similar puzzles.

## Enumerating combinatorial objects

When you must visit *every* object — all permutations, all subsets, all `k`-subsets — use a direct generator rather than a recursive filter.

- **Permutations**: the in-place *next permutation* algorithm (as in C++'s `std::next_permutation`) steps through all `n!` arrangements in lexicographic order with `O(1)` amortised work per step: find the rightmost ascent, swap with the next larger suffix element, reverse the suffix. Heap's algorithm generates them with one swap per step.
- **All subsets**: count a mask from 0 to `2^n − 1`; bit `i` says whether element `i` is chosen. Iterate the *submasks* of a mask `m` with `s = (s − 1) & m`, which visits all `2^|m|` submasks — the engine of "subset DP" and the `3^n` loop over all (set, subset) pairs.
- **All `k`-element subsets**: **Gosper's hack** computes the next integer with the same number of set bits, from `0b0111…` upward, in a handful of instructions: `c = x & −x; r = x + c; x = (((r ^ x) >> 2) / c) | r`. The program prints the ten 3-subsets of a 5-element set it generates (`C(5,3) = 10`).

## Bitmask DP: share instead of prune

Backtracking explores the same sub-problem repeatedly from different paths. When the sub-problem depends only on a *set* of choices already made, memoise it keyed by a bitmask of that set — classic bitmask DP. For the **travelling salesman problem** (shortest tour visiting each of `n` cities once), the state is *(set of cities visited, current city)*: `dp[mask][j]` is the shortest path from city 0 through exactly the cities in `mask`, ending at `j`. The transition extends to an unvisited `k`: `dp[mask ∪ {k}][k] = min(dp[mask][j] + d(j,k))`. That is `2^n · n` states with `O(n)` work each: **`O(2^n n²)`** time and `O(2^n n)` memory — the Held–Karp algorithm (1962) — against `(n−1)!` for brute force. For `n = 20`, `2^20·400 ≈ 4·10^8` operations versus `1.2·10^17` tours.

```c
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

/* ---------- brute force: try every permutation of cities 1..n-1 (fix city 0 as start): (n-1)! tours ---------- */
static int best_len, nn; static int *dm;
static void permute(int *p, int k, int len_so_far) {
    if (len_so_far >= best_len) return;                                          /* branch and bound: this partial tour is already too long */
    if (k == nn) { int total = len_so_far + dm[p[nn - 1] * nn + p[0]]; if (total < best_len) best_len = total; return; }
    for (int i = k; i < nn; i++) { int t = p[k]; p[k] = p[i]; p[i] = t; permute(p, k + 1, len_so_far + dm[p[k - 1] * nn + p[k]]); t = p[k]; p[k] = p[i]; p[i] = t; }
}
static int tsp_brute(int n, int *d) {
    nn = n; dm = d; best_len = 1 << 30;
    int p[16];
    for (int i = 0; i < n; i++) p[i] = i;
    permute(p, 1, 0);
    return best_len;
}

/* ---------- Held-Karp: dp[mask][j] = shortest path starting at city 0, visiting exactly the cities in `mask`, ending at j.  O(2^n n^2) time, O(2^n n) memory ---------- */
static int held_karp(int n, const int *d) {
    size_t full = (size_t)1 << n; int *dp = malloc(full * (size_t)n * sizeof(int));
    for (size_t i = 0; i < full * (size_t)n; i++) dp[i] = 1 << 29;
    dp[1 * n + 0] = 0;                                                          /* only city 0 visited, standing at city 0 */
    for (size_t mask = 1; mask < full; mask++) {
        if (!(mask & 1)) continue;
        for (int j = 0; j < n; j++) {
            int cur = dp[mask * n + j]; if (cur >= 1 << 29 || !(mask >> j & 1)) continue;
            for (int k = 1; k < n; k++) if (!(mask >> k & 1)) { size_t nm = mask | ((size_t)1 << k); int v = cur + d[j * n + k]; if (v < dp[nm * n + k]) dp[nm * n + k] = v; }
        }
    }
    int best = 1 << 29; for (int j = 1; j < n; j++) if (dp[(full - 1) * n + j] + d[j * n + 0] < best) best = dp[(full - 1) * n + j] + d[j * n + 0];
    free(dp); return best;
}

int main(void) {
    /* correctness: Held-Karp equals brute force on random complete graphs (asymmetric distances allowed) */
    for (int round = 0; round < 300; round++) {
        int n = 2 + (int)(rnd() % 7); int d[64]; for (int i = 0; i < n * n; i++) d[i] = 1 + (int)(rnd() % 50);
        for (int i = 0; i < n; i++) d[i * n + i] = 0;
        int hk = held_karp(n, d), bf = tsp_brute(n, d); assert(hk == bf);
    }
    printf("300 random instances with 2-8 cities: Held-Karp bitmask DP == exhaustive search over all tours\n\n");
    printf("%4s %16s %12s %14s %12s\n", "n", "(n-1)! tours", "brute+bound ms", "Held-Karp ms", "table memory");
    for (int n = 8; n <= 21; n += (n < 12 ? 1 : (n < 16 ? 2 : 1))) {
        int *d = malloc((size_t)n * n * sizeof *d); for (int i = 0; i < n * n; i++) d[i] = 10 + (int)(rnd() % 90); for (int i = 0; i < n; i++) d[i * n + i] = 0;
        double fact = 1; for (int k = 2; k < n; k++) fact *= k;
        double tb = -1; if (n <= 12) { double t0 = now(); int b = tsp_brute(n, d); tb = now() - t0; (void)b; }
        double t0 = now(); int hk = held_karp(n, d); double th = now() - t0; (void)hk;
        if (tb >= 0) printf("%4d %16.3g %12.2f %14.2f %9.1f MB\n", n, fact, tb * 1e3, th * 1e3, (double)((size_t)1 << n) * n * 4 / 1e6);
        else printf("%4d %16.3g %12s %14.2f %9.1f MB\n", n, fact, "(not run)", th * 1e3, (double)((size_t)1 << n) * n * 4 / 1e6);
        free(d); if (th > 20) break;
    }
    return 0;
}
```

```text
$ gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined tsp.c -o tsp_asan && ./tsp_asan | head -1 && gcc -std=gnu11 -O2 -Wall -Wextra tsp.c -o tsp && ./tsp
300 random instances with 2-8 cities: Held-Karp bitmask DP == exhaustive search over all tours
300 random instances with 2-8 cities: Held-Karp bitmask DP == exhaustive search over all tours

   n     (n-1)! tours brute+bound ms   Held-Karp ms table memory
   8         5.04e+03         0.09           0.04       0.0 MB
   9         4.03e+04         0.18           0.11       0.0 MB
  10         3.63e+05         1.53           0.30       0.0 MB
  11         3.63e+06         2.63           0.66       0.1 MB
  12         3.99e+07         5.99           1.65       0.2 MB
  14         6.23e+09    (not run)           7.68       0.9 MB
  16         1.31e+12    (not run)          34.39       4.2 MB
  17         2.09e+13    (not run)          76.62       8.9 MB
  18         3.56e+14    (not run)         169.24      18.9 MB
  19          6.4e+15    (not run)         357.19      39.8 MB
  20         1.22e+17    (not run)         730.60      83.9 MB
  21         2.43e+18    (not run)        1509.35     176.2 MB
```

- The first line, from the sanitizer build, is the correctness check: Held–Karp equals exhaustive search over all tours on 300 random instances (asymmetric distances allowed).
- Branch-and-bound brute force is fast up to about 12 cities on this *random* instance (the bound prunes aggressively when distances are random; it is far less effective on structured instances), and was not run beyond that: the number of tours reaches 6·10^9 at 14 cities.
- Held–Karp's time roughly **doubles per added city** (a factor of 2 from `2^n`, a little more from `n²`) — about 1.5 s at `n = 21` — which is exponential but a different world from `(n−1)!`; its **memory** doubling is the practical limit (176 MB at 21 cities; about 7 GB at 26).

Bitmask DP is the standard tool whenever `n ≤ ~20–25` and the problem has a "which subset have I used" state: assignment of tasks to workers, minimum cost to cover a set, partitioning into compatible groups, counting Hamiltonian paths, and game states such as tic-tac-toe boards.

## Choosing between them

| Technique | Use when | Cost |
|---|---|---|
| Plain backtracking | Few solutions wanted, strong early constraints (puzzles, N-Queens, parsing) | exponential, tamed by pruning |
| Branch and bound | Optimisation with a cheap, valid bound | exponential worst case, often far better |
| Memoisation / DP on subsets | Sub-problems depend only on the set chosen so far, `n ≲ 25` | `O(2^n · poly)` |
| Meet in the middle | `n ≲ 40` subset problems: split in half, enumerate `2^(n/2)` each, combine | `O(2^(n/2))` |
| Heuristics / approximation | `n` large; exactness not required | polynomial |
| SAT/SMT/ILP solvers | Large structured instances | solver-dependent, often excellent in practice |

## Pitfalls

- **Forgetting to undo.** State mutated on the way down must be restored on the way up; with bitmask-by-value recursion (as in `queens_bits`) undo is automatic because the masks are copies.
- **Pruning that discards the optimum.** A bound must be *optimistic*; an estimate that can underestimate a branch's best outcome silently returns wrong answers. Always compare with brute force on small cases, as these programs do.
- **Deep recursion.** Search depth equals the number of decisions, usually small; if not, convert to an explicit stack.
- **Combinatorial explosion in memory**: bitmask DP over `n = 30` needs `2^30·n` states — plan memory before choosing it.
- **Shift width**: `1 << 31` overflows `int`, and `1 << n` with `n ≥ 32` on a 32-bit type is undefined behavior; use `1ULL << n` for sets that may exceed 31 elements.
- **Counting vs finding**: finding one solution can be fast with a good heuristic while counting all solutions needs the whole tree — decide which question you are answering.
- **Symmetry**: N-Queens solutions come in rotations and reflections; exploit that (fix the first queen's column in the left half) rather than searching duplicates.

## Key takeaways

1. Backtracking is depth-first search over partial solutions with early rejection; its performance is set by how early it detects infeasibility and in what order it makes decisions.
2. Bitmasks make constraint tests and candidate iteration single instructions; the N-Queens tree is identical but about an order of magnitude cheaper per node.
3. The fail-first (fewest candidates) heuristic cut Sudoku search by up to three orders of magnitude without changing the solver otherwise.
4. Branch and bound needs an optimistic bound; subset-sum bounds cut the search space by orders of magnitude while returning identical counts.
5. Held–Karp turns the `(n−1)!` TSP search into `O(2^n n²)` by memoising on (visited set, last city); memory, not time, becomes the limit near `n = 25`.
6. Verify pruned and shared searches against exhaustive search on small inputs.
