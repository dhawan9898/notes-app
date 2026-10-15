---
kind: code
title: Tower of Hanoi
module: Foundations
difficulty: Medium
tags:
  - recursion
  - recurrence
starter: dsa/starters/recursion/ex3-hanoi.c
examples:
  - input: hanoi_moves(5)
    output: '31'
  - input: hanoi_moves(63)
    output: '9223372036854775807'
  - input: hanoi(6, 0, 2, 1)
    output: 63 legal moves; all six disks end on peg 2
constraints:
  - Pegs are numbered 0, 1, 2.
  - '`1 <= n <= 63` for `hanoi_moves`; `hanoi` is tested with 6 disks.'
hints:
  - Move n-1 disks out of the way, move the largest one, then move the n-1 disks on top of it.
  - The closed form is `2^n - 1`; `(1LL << n) - 1` works up to 62, be careful with 63.
---

Moving `n` disks from one peg to another, never putting a larger disk on a smaller one, takes `T(n) = 2 T(n-1) + 1` moves. Implement `hanoi_moves(n)` from this recurrence, then `hanoi(n, from, to, via)`, which performs the moves by calling `move(from, to)` for each one. The tests check that every move is legal.

## Explanation

*Fibonacci.* The naive version recomputes the same values over and over: `fib(30)` makes **2,692,537 calls** to produce one number, and the call count itself grows like the Fibonacci numbers (about 1.618ⁿ, **exponential**), so `fib(50)` would need tens of billions. Memoisation stores each `fib(k)` the first time and reuses it: **59 calls** (2n − 1), so O(n) time and O(n) space. This is the core idea of dynamic programming: when subproblems overlap, remember them. (An iterative loop with two variables does the same in O(n) time and O(1) space.)

*Hanoi.* To move n disks: move n−1 disks to the spare peg, move the largest disk, move the n−1 disks back on top, so `T(n) = 2·T(n−1) + 1` with `T(0) = 0`, giving `T(n) = 2ⁿ − 1` (1, 31, 1,023 and 1,048,575 moves for 1, 5, 10 and 20 disks). At one move per second, 20 disks take about 12 days, and 64 disks take 2⁶⁴ − 1 ≈ 1.8 × 10¹⁹ seconds, about 585 billion years. Exponential growth means some problems are out of reach however fast the computer: here the *answer itself* is exponentially long, so no algorithm can do better.

*Fast power.* `b^e` equals `(b^(e/2))²` when `e` is even and `b · (b^(e/2))²` when `e` is odd, so each step halves the exponent: O(log e) multiplications (4 levels of recursion for `e` = 13, instead of a 13-step loop). Halving works because exponents add (`b^(2k) = b^k · b^k`). The same trick computes modular powers (used in cryptography), matrix powers and Fibonacci numbers in O(log n) steps. This is **divide and conquer** with a single subproblem of half the size, giving the recurrence `T(e) = T(e/2) + O(1) = O(log e)`.
