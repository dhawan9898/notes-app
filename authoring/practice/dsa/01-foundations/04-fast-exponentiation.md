---
kind: code
title: Fast exponentiation
module: Foundations
difficulty: Medium
tags:
  - recursion
  - divide and conquer
  - math
starter: dsa/starters/recursion/ex4-fast-power.c
examples:
  - input: power_fast(3, 13)
    output: '1594323'
  - input: power_fast(-2, 5)
    output: '-32'
  - input: power_fast(1, 1000)
    output: 1, using at most 20 multiplications
constraints:
  - '`e >= 0`; the result fits in a `long long`.'
  - 'Do not square the base once more than needed: the intermediate value can overflow.'
hints:
  - '`b^e = (b^(e/2))^2`, times `b` when `e` is odd.'
  - 'Iterative form: square the base each step and multiply into the result when the low bit of `e` is 1.'
---

Compute `b^e` with **O(log e)** multiplications instead of `e`. The helper `mul(x, y)` is the multiplication the tests count. The loop in the editor is correct but makes `e` multiplications.

## Explanation

*Fibonacci.* The naive version recomputes the same values over and over: `fib(30)` makes **2,692,537 calls** to produce one number, and the call count itself grows like the Fibonacci numbers (about 1.618ⁿ, **exponential**), so `fib(50)` would need tens of billions. Memoisation stores each `fib(k)` the first time and reuses it: **59 calls** (2n − 1), so O(n) time and O(n) space. This is the core idea of dynamic programming: when subproblems overlap, remember them. (An iterative loop with two variables does the same in O(n) time and O(1) space.)

*Hanoi.* To move n disks: move n−1 disks to the spare peg, move the largest disk, move the n−1 disks back on top, so `T(n) = 2·T(n−1) + 1` with `T(0) = 0`, giving `T(n) = 2ⁿ − 1` (1, 31, 1,023 and 1,048,575 moves for 1, 5, 10 and 20 disks). At one move per second, 20 disks take about 12 days, and 64 disks take 2⁶⁴ − 1 ≈ 1.8 × 10¹⁹ seconds, about 585 billion years. Exponential growth means some problems are out of reach however fast the computer: here the *answer itself* is exponentially long, so no algorithm can do better.

*Fast power.* `b^e` equals `(b^(e/2))²` when `e` is even and `b · (b^(e/2))²` when `e` is odd, so each step halves the exponent: O(log e) multiplications (4 levels of recursion for `e` = 13, instead of a 13-step loop). Halving works because exponents add (`b^(2k) = b^k · b^k`). The same trick computes modular powers (used in cryptography), matrix powers and Fibonacci numbers in O(log n) steps. This is **divide and conquer** with a single subproblem of half the size, giving the recurrence `T(e) = T(e/2) + O(1) = O(log e)`.
