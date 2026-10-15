---
title: "Practice: Complexity and Recursion"
track: dsa
module: Foundations
moduleOrder: 1
order: 3
level: intermediate
summary: "Four exercises on the foundations module: classifying seven loops by counting their steps and reading the doubling ratio, explaining why naive Fibonacci explodes and memoisation fixes it, counting Tower of Hanoi moves, and computing powers in O(log n) with square-and-multiply, with verified C answers."
prerequisites:
  - "Complexity Analysis and Amortized Analysis"
  - "Recursion, Divide and Conquer, and Loop Invariants"
sources:
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 3 (Characterizing Running Times), Ch. 4 (Divide-and-Conquer)
  - label: Sedgewick & Wayne — Algorithms, 4th ed., §1.4 (Analysis of Algorithms)
  - label: Knuth — The Art of Computer Programming, Vol. 1, §1.2 (Mathematical Preliminaries)
---

Complexity analysis is learned by classifying many small loops until it is automatic. First do each one on paper (write the exact count for `n` as a formula if you can), then check with the programs.

## Exercises

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Classify the loops.** For each loop shape, give the Big-O of the number of times the innermost statement runs, then predict the ratio when `n` doubles from 64 to 128.

- A: `for i in 0..n` (single loop)
- B: `for (i = 1; i < n; i *= 2)` (the counter doubles)
- C: `for i in 0..n { for j in 0..n }`
- D: `for i in 0..n { for j in i..n }` (the inner loop starts at `i`)
- E: `for (i = 1; i < n; i *= 2) { for j in 0..i }`
- F: `for i in 0..n { for (j = 1; j < n; j *= 2) }`
- G: two separate loops of `n` iterations, one after the other

**2. Why is naive Fibonacci slow?** `fib(n) = fib(n-1) + fib(n-2)`. How many calls does `fib(30)` make? Fix it with memoisation and count again. What is the complexity of each version?

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/recursion/ex2-fib-memo.c}}

</details>

**3. Tower of Hanoi.** Moving *n* disks from one peg to another (larger disks never on smaller ones) takes how many moves? Write the recurrence and the closed form. If one move takes one second, how long do 20 and 64 disks take?

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/recursion/ex3-hanoi.c}}

</details>

**4. Fast power.** Compute `b^e` with O(log e) multiplications instead of `e`. Why does halving the exponent work?

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/recursion/ex4-fast-power.c}}

</details>

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

{{include dsa/practice/p1_loops.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -o p1_loops p1_loops.c && ./p1_loops; rm -f p1_loops}}

- **A** is **O(n)**: exactly n steps; doubling n doubles the work (ratio 2.00).
- **B** is **O(log n)**: the counter doubles each time, so about log₂ n steps (6, then 7: one extra step when n doubles; ratio 1.17).
- **C** is **O(n²)**: n × n steps; doubling n quadruples the work (4.00).
- **D** is **O(n²)** too: the inner loop runs n, n−1, ..., 1 times, a sum of n(n+1)/2 steps (2080 for n = 64: 64 × 65 / 2). It is half of C, but the constant does not change the class; the ratio approaches 4 (3.97).
- **E** is **O(n)**, a classic surprise: the inner loop runs 1 + 2 + 4 + ... + n/2 times, a geometric series that sums to about n (63 for n = 64). Two nested loops are not automatically quadratic; look at how many times the inner one runs *in total*. Ratio 2.02.
- **F** is **O(n log n)**: an outer loop of n and an inner loop of log n (n × 6 = 384 for n = 64). Ratio 2.33, a little over 2.
- **G** is **O(n)**: loops *in sequence* add (n + n = 2n); only *nested* loops multiply. Ratio 2.00.

The doubling ratio (1, 2, slightly above 2, 4) is the practical test from *Measuring Growth: Counting and Timing in Practice*.

**2, 3 and 4.**

{{include dsa/practice/p1_recursion.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p1_recursion p1_recursion.c && ./p1_recursion; rm -f p1_recursion}}

*Fibonacci.* The naive version recomputes the same values over and over: `fib(30)` makes **2,692,537 calls** to produce one number, and the call count itself grows like the Fibonacci numbers (about 1.618ⁿ, **exponential**), so `fib(50)` would need tens of billions. Memoisation stores each `fib(k)` the first time and reuses it: **59 calls** (2n − 1), so O(n) time and O(n) space. This is the core idea of dynamic programming: when subproblems overlap, remember them. (An iterative loop with two variables does the same in O(n) time and O(1) space.)

*Hanoi.* To move n disks: move n−1 disks to the spare peg, move the largest disk, move the n−1 disks back on top, so `T(n) = 2·T(n−1) + 1` with `T(0) = 0`, giving `T(n) = 2ⁿ − 1` (1, 31, 1,023 and 1,048,575 moves for 1, 5, 10 and 20 disks). At one move per second, 20 disks take about 12 days, and 64 disks take 2⁶⁴ − 1 ≈ 1.8 × 10¹⁹ seconds, about 585 billion years. Exponential growth means some problems are out of reach however fast the computer: here the *answer itself* is exponentially long, so no algorithm can do better.

*Fast power.* `b^e` equals `(b^(e/2))²` when `e` is even and `b · (b^(e/2))²` when `e` is odd, so each step halves the exponent: O(log e) multiplications (4 levels of recursion for `e` = 13, instead of a 13-step loop). Halving works because exponents add (`b^(2k) = b^k · b^k`). The same trick computes modular powers (used in cryptography), matrix powers and Fibonacci numbers in O(log n) steps. This is **divide and conquer** with a single subproblem of half the size, giving the recurrence `T(e) = T(e/2) + O(1) = O(log e)`.

</details>

## Key takeaways

1. Count steps, then read the doubling ratio: 2 means linear, about 4 means quadratic, slightly above 2 means n log n, barely rising means logarithmic.
2. Nested loops are not automatically quadratic (a doubling inner loop sums to about n); sequential loops add, nested loops multiply.
3. Overlapping subproblems make naive recursion exponential; memoisation turns it linear.
4. Halving the problem each step gives O(log n); recurrences such as T(n) = 2T(n−1) + 1 solve to exponential growth.
