---
kind: code
title: Fibonacci, recursive and iterative
module: Getting Started
difficulty: Easy
tags:
  - recursion
  - iteration
starter: dsa/starters/start-here/ex8-fibonacci.c
examples:
  - input: fib_iter(10)
    output: '55'
  - input: fib_iter(50)
    output: '12586269025'
  - input: fib_rec(25)
    output: '75025'
constraints:
  - '`0 <= n <= 90` for `fib_iter`; `0 <= n <= 25` for `fib_rec`.'
  - Return type is `long long`.
hints:
  - 'The iterative version needs only two variables: the previous and the current value.'
  - 'The recursive version makes about 240,000 calls for `n = 25`: that is why it is slow.'
---

Write `fib_rec` (a direct recursive definition) and `fib_iter` (a loop). `fib(0) = 0`, `fib(1) = 1`, `fib(n) = fib(n-1) + fib(n-2)`. They must agree for every `n` from 0 to 25, and `fib_iter` must handle `n = 50` without overflowing a `long long`.

## Explanation

**Cost.** recursive O(2ⁿ) vs iterative O(n).
