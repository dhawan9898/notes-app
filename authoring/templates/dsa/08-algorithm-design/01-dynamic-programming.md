---
title: Dynamic Programming
track: dsa
module: Algorithm Design
moduleOrder: 8
order: 1
level: advanced
summary: "A method for designing DP solutions — state, transition, order, answer — with knapsack, coin change, LIS, matrix chain, subset sum and tree DP, every one verified against brute force, plus measured space and time optimizations and the loop-direction bug."
sources:
  - label: Bellman — Dynamic Programming (Princeton University Press, 1957)
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 14 (Dynamic Programming)
  - label: Kleinberg & Tardos — Algorithm Design, Ch. 6 (Dynamic Programming)
  - label: Skiena — The Algorithm Design Manual, 3rd ed., Ch. 10 (Dynamic Programming)
  - label: Fredman — On Computing the Length of Longest Increasing Subsequences (Discrete Mathematics 11(1), 1975)
  - label: Hirschberg — A Linear Space Algorithm for Computing Maximal Common Subsequences (CACM 18(6), 1975)
  - label: Garey & Johnson — Computers and Intractability (pseudo-polynomial algorithms and strong NP-completeness)
---

**Dynamic programming** (DP) solves a problem by combining solutions of overlapping subproblems, computing each exactly once. It applies when two properties hold:

1. **Optimal substructure** — an optimal solution is built from optimal solutions of subproblems (the shortest path from `a` to `c` through `b` uses shortest paths `a→b` and `b→c`).
2. **Overlapping subproblems** — the same subproblems recur, so remembering answers saves work. (If subproblems do not overlap, you have *divide and conquer*: merge sort has optimal substructure and no overlap, so memoization buys nothing.)

The term, coined by Richard Bellman, predates computers as a word for "planning"; what it means in practice is: *turn an exponential recursion into a polynomial table fill*.

## A four-step method

1. **State.** What is the minimum information that identifies a subproblem? `f(i, c)` = "best value using the first `i` items with capacity `c`". The state is the most important decision; its size (number of distinct states) times the work per state is the running time.
2. **Transition.** Express a state through smaller states: `f(i, c) = max(f(i−1, c), v_i + f(i−1, c − w_i))`.
3. **Base cases and order.** Smallest states are known directly; evaluate in an order that guarantees dependencies are ready (increasing `i`; or recursion with memoization finds the order for you).
4. **Answer and reconstruction.** The answer is some state (here `f(n, cap)`); to recover *which* choices achieve it, store the decision (or recompute backwards from the table).

**Top-down** (recursion + memo table) writes the recurrence literally and computes only reachable states, at the cost of recursion depth and call overhead. **Bottom-up** (loops over a table) is usually faster, makes the evaluation order explicit, and allows **space optimization**: if row `i` depends only on row `i−1`, keep one row.

### Overlap, measured

Fibonacci is the smallest example of why it matters:

{{include dsa/dp/demo_dp.c 10-14}}

The complete demonstration program (listing below) runs the numbers: the naive recursion makes **331 million calls** for `fib(40)` while the memoized version makes **79** — a state space of 40 entries, each computed once.

## The library

{{include dsa/dp/dp.h}}

Notes on the code:

- **0/1 vs unbounded knapsack differ only in loop direction.** In 0/1 knapsack the inner loop runs capacity *downward*, so `dp[c − w]` still holds the value from *before* this item was considered; running it *upward* lets the same item be added repeatedly — which is exactly the unbounded problem. Picking the wrong direction produces plausible but wrong answers (shown below).
- **Counting combinations vs sequences in coin change** is the order of the loops: coins in the *outer* loop counts each multiset once (`1+2` and `2+1` are the same); amount in the outer loop would count ordered sequences.
- **LIS in O(n log n)** keeps `tails[k]`, the smallest last element of any increasing subsequence of length `k+1`. It is always sorted, so each new element replaces the first tail ≥ it by binary search — "patience sorting". The array does not hold a subsequence itself, only its length is the answer.
- **Matrix-chain multiplication** is an *interval DP*: `dp[i][j]` for the best parenthesization of matrices `i..j`, trying every split point `k`. States are `O(n²)`, each does `O(n)` work → `O(n³)`.
- **Subset sum** uses a bitset: bit `s` records "some subset sums to `s`", and adding an item `x` is `reach |= reach << x` applied across machine words.

## Verification

Every DP is checked against an exhaustive or exponential-recursion oracle on 3000 random instances: all subsets for knapsack and LIS and subset sum, enumeration of copy counts for unbounded knapsack, plain recursion for coin change and matrix chain.

{{include dsa/dp/test_dp.c}}

{{run dsa/dp :: gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined test_dp.c -o test_dp && ./test_dp}}

## Demonstrations

{{include dsa/dp/demo_dp.c}}

{{run dsa/dp :: gcc -std=gnu11 -O2 -Wall -Wextra demo_dp.c -o demo_dp && ./demo_dp}}

What the output teaches:

- **Memoization collapses exponential recursion**: 331,160,281 calls vs 79 for the same answer.
- **Loop direction is semantics**: with items (3, 4) and (4, 5) and capacity 10, the downward loop gives 9 (each item once) and the upward loop gives 13 (the weight-3 item twice plus the weight-4 item).
- **Greedy is not DP.** For US coins greedy and DP agree, but for coins `{4, 3, 1}` and amount 6 greedy picks 4+1+1 (3 coins) while the optimum is 3+3 (2 coins). Whether greedy works depends on the structure of the problem — the next note's subject.
- **Space optimization** is dramatic: the textbook `n × cap` table for 2000 items and capacity 50,000 needs 800 MB (and a pass through memory), the single row needs 0.4 MB and runs several times faster because it stays in cache.
- **Bitset subset sum** is an order of magnitude faster than the byte array here even though the compiler vectorises the byte version; it also needs one eighth of the memory. (Bit-parallel DP is a general trick: edit distance, LCS and string matching all have bit-parallel versions.)
- **LIS**: the quadratic DP takes seconds on 40,000 elements; the patience-sorting version takes under 2 ms — a thousand-fold difference from replacing "scan all earlier elements" with "binary search a sorted summary".
- **Tree DP**: for each vertex keep two numbers (best total if this vertex is taken, best if skipped). Because children are numbered after parents, a loop from the last vertex to the first is a post-order traversal with no recursion, so a path of two million vertices (which would overflow a recursive solution's stack) is fine.

## Where DP appears

| Problem family | State | Complexity |
|---|---|---|
| 0/1 and unbounded knapsack | (items used, capacity) | `O(n·W)` — *pseudo-polynomial* |
| Coin change (min coins / ways) | amount | `O(amount·k)` |
| Longest common subsequence, edit distance | (i, j) prefixes of two strings | `O(nm)`; see the string-matching note |
| LIS | tail value per length | `O(n log n)` |
| Matrix chain, optimal BST, polygon triangulation | interval (i, j) | `O(n³)` |
| Shortest paths on DAGs | vertex | `O(V + E)` — this *is* a DP; Bellman–Ford is a DP over path length |
| Tree DP (independent set, diameter, rerooting) | (vertex, state) | `O(n)` |
| Bitmask DP (TSP, assignment) | (visited set, last vertex) | `O(2^n · n²)`; next-but-one note |
| Digit DP, probability and expected value DP | position + tight/loose flag | polynomial in digits |
| Parsing (CYK), Viterbi, sequence alignment | (position, state) | `O(n³)` and `O(nK²)` |

**Pseudo-polynomial** means polynomial in the *numeric value* `W` of an input rather than its bit length: the knapsack DP runs in `O(nW)`, which explodes for `W = 2^60`. Knapsack and subset sum are NP-hard in general; the DP is efficient only when the numbers are small.

## Pitfalls

- **Wrong state.** If the transition needs information the state does not carry (for example, "which items are used", or "was the previous character a match"), either enlarge the state or the recurrence is simply wrong.
- **Loop direction and order** (above): downward for 0/1, upward for unbounded; coins-outer for combinations. Test with a brute-force oracle on tiny inputs, as the verification program does.
- **Sentinel confusion in memo tables**: using 0 as "not computed" when 0 is a legal answer causes recomputation (as in `fib_memo`, where `memo[n]` is never 0 for `n ≥ 2` — in general use a separate flag or −1).
- **Overflow**: counting DPs grow quickly (use 64-bit or modular arithmetic); with an `INF` sentinel never add to it (the same bug as in Floyd–Warshall).
- **Recursion depth** in top-down DP: depth equals the longest dependency chain, which can be `n`; convert to bottom-up or use an explicit stack for large inputs.
- **Table size**: `O(n²)` memory is the usual limit (10⁵ × 10⁵ is impossible); use rolling rows, Hirschberg's divide-and-conquer to recover solutions in linear space, or banded DP.
- **Reconstruction**: keeping a choice array alongside the table is easy; recomputing the path backwards from the table avoids that memory but must follow the same tie-breaking.

## Key takeaways

1. DP = optimal substructure + overlapping subproblems; the work is *state design*, then transition, order and answer.
2. Memoization (top-down) is easiest to write; bottom-up tables are faster and enable rolling-row space optimization.
3. Loop direction and loop nesting order encode the problem's semantics (0/1 vs unbounded; combinations vs sequences).
4. Replace "scan all previous states" with a data structure — binary search, a bitset, a monotone deque — to cut a polynomial factor (LIS: `n²` → `n log n`).
5. Knapsack-type DPs are pseudo-polynomial; always check the magnitude of the numeric parameter.
6. Verify every DP against brute force on tiny random inputs before trusting it.
