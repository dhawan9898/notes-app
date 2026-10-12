---
title: Backtracking, Branch and Bound, and Bitmask DP
track: dsa
module: Algorithm Design
moduleOrder: 8
order: 3
level: advanced
summary: "Systematic search with pruning — N-Queens with bitmasks, subset-sum bounds, a Sudoku solver with the fewest-candidates heuristic, permutation and subset enumeration (Gosper's hack), and the Held-Karp bitmask DP for the travelling salesman problem — with node counts and timings."
sources:
  - label: Knuth — The Art of Computer Programming, Vol. 4A, §7.2.2 (Backtrack programming) and §7.1.3 (Bitwise tricks and techniques)
  - label: Knuth — Dancing Links (in Millennial Perspectives in Computer Science, 2000)
  - label: Held & Karp — A Dynamic Programming Approach to Sequencing Problems (J. SIAM 10(1), 1962)
  - label: Bellman — Dynamic Programming Treatment of the Travelling Salesman Problem (J. ACM 9(1), 1962)
  - label: Land & Doig — An Automatic Method of Solving Discrete Programming Problems (Econometrica 28(3), 1960)
  - label: Russell & Norvig — Artificial Intelligence, A Modern Approach, Ch. 6 (Constraint Satisfaction Problems)
  - label: Warren — Hacker's Delight, 2nd ed., Ch. 2 (Basics) and Ch. 15 (Gosper's hack, next subset of fixed size)
  - label: OEIS A000170 — Number of ways of placing n nonattacking queens on an n X n board
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

{{include dsa/backtrack/queens.c}}

{{run dsa/backtrack :: gcc -std=gnu11 -O2 -Wall -Wextra queens.c -o queens && ./queens}}

- Solution counts match OEIS A000170 for every `n` (2, 10, 4, 40, 92, … 73,712 at `n = 13`).
- The node counts of the two implementations are **equal in every row**; the bitmask version is roughly an order of magnitude faster per row and the gap widens with `n`, because the array version's `O(n)` safety test grows while the bitmask test stays one machine instruction. This is the whole point of the C track's *Bit Manipulation* note applied to search.
- The tree itself grows by about 5× per added row (17 nodes at `n = 4` → 4.7 million at `n = 13`); no implementation trick changes that growth rate, which is why `n` beyond ~15 needs symmetry reduction (halve by reflection) or different algorithms.

## Branch and bound: pruning with a bound

For *optimisation* problems, keep the best solution found so far and prune any branch whose **optimistic estimate** cannot beat it. The estimate must never be pessimistic (otherwise you'd discard the optimum). Two instances are in the programs here:

- **Subset sum with positive numbers.** Sorting the numbers in decreasing order and keeping suffix sums gives two cut-offs: stop when the running sum already exceeds the target, or when even taking *everything remaining* cannot reach it. On 28 numbers with the target at half the total, the plain recursion visits all `2^29 − 1` ≈ 537 million nodes; the bounded one visits about 33× fewer in the run below, while finding the same number of subsets — asserted equal.
- **Travelling salesman with a length bound.** The brute-force tour enumeration below abandons a partial tour as soon as its length reaches the best complete tour found so far.

### Constraint satisfaction: Sudoku

Sudoku is a constraint-satisfaction problem: each empty cell must take a digit not used in its row, column or box. With three 9-bit masks per unit, the candidate set of any cell is `~(row | col | box)` — one expression — and the solver recurses over the candidates of one chosen cell. The key decision is *which cell*. The naive solver takes the next empty cell in reading order; the **MRV** ("minimum remaining values", fail-first) heuristic takes the cell with the fewest candidates, which both detects dead ends sooner (zero candidates → backtrack at once) and makes forced moves (one candidate) free.

{{include dsa/backtrack/sudoku.c}}

{{run dsa/backtrack :: gcc -std=gnu11 -O2 -Wall -Wextra sudoku.c -o sudoku && ./sudoku}}

The same solver with only a change in cell ordering visits 4× to 1,500× fewer nodes: for the sparse 17-clue grid, reading order needs hundreds of thousands of nodes and MRV a few hundred. Every result is checked by an independent validity test (each row, column and box contains 1–9 exactly once). For harder constraint problems, **constraint propagation** (after placing a digit, eliminate it from the peers' candidates and apply "naked/hidden single" rules) shrinks the tree further, and **Dancing Links** (Knuth's Algorithm X on the exact-cover formulation) is the classic general-purpose engine for Sudoku, polyomino tiling and similar puzzles.

## Enumerating combinatorial objects

When you must visit *every* object — all permutations, all subsets, all `k`-subsets — use a direct generator rather than a recursive filter.

- **Permutations**: the in-place *next permutation* algorithm (as in C++'s `std::next_permutation`) steps through all `n!` arrangements in lexicographic order with `O(1)` amortised work per step: find the rightmost ascent, swap with the next larger suffix element, reverse the suffix. Heap's algorithm generates them with one swap per step.
- **All subsets**: count a mask from 0 to `2^n − 1`; bit `i` says whether element `i` is chosen. Iterate the *submasks* of a mask `m` with `s = (s − 1) & m`, which visits all `2^|m|` submasks — the engine of "subset DP" and the `3^n` loop over all (set, subset) pairs.
- **All `k`-element subsets**: **Gosper's hack** computes the next integer with the same number of set bits, from `0b0111…` upward, in a handful of instructions: `c = x & −x; r = x + c; x = (((r ^ x) >> 2) / c) | r`. The program prints the ten 3-subsets of a 5-element set it generates (`C(5,3) = 10`).

## Bitmask DP: share instead of prune

Backtracking explores the same sub-problem repeatedly from different paths. When the sub-problem depends only on a *set* of choices already made, memoise it keyed by a bitmask of that set — classic bitmask DP. For the **travelling salesman problem** (shortest tour visiting each of `n` cities once), the state is *(set of cities visited, current city)*: `dp[mask][j]` is the shortest path from city 0 through exactly the cities in `mask`, ending at `j`. The transition extends to an unvisited `k`: `dp[mask ∪ {k}][k] = min(dp[mask][j] + d(j,k))`. That is `2^n · n` states with `O(n)` work each: **`O(2^n n²)`** time and `O(2^n n)` memory — the Held–Karp algorithm (1962) — against `(n−1)!` for brute force. For `n = 20`, `2^20·400 ≈ 4·10^8` operations versus `1.2·10^17` tours.

{{include dsa/backtrack/tsp.c}}

{{run dsa/backtrack :: gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined tsp.c -o tsp_asan && ./tsp_asan | head -1 && gcc -std=gnu11 -O2 -Wall -Wextra tsp.c -o tsp && ./tsp}}

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
