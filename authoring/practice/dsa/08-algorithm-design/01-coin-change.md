---
kind: code
title: Coin change
module: Algorithm Design
difficulty: Medium
tags:
  - dynamic programming
starter: dsa/starters/design/ex1-coins.c
examples:
  - input: coins {4, 3, 1}, target 6
    output: min_coins = 2
    why: 3 + 3. The greedy choice 4 + 1 + 1 needs three coins.
  - input: coins {1, 5, 10}, target 10
    output: count_ways = 4
  - input: coins {5, 10}, target 3
    output: min_coins = -1
constraints:
  - Target 0 needs 0 coins and has exactly one way (the empty set).
  - '`count_ways({25,10,5,1}, 100)` is 242: use `long long`.'
hints:
  - '`best[t] = 1 + min(best[t - coin])` over the coins that fit.'
  - >-
    To count multisets rather than sequences, loop over the coins in the **outer** loop and the amounts in the inner
    loop.
---

Given coin values (unlimited supply of each) and a target amount, write two functions:

1. `min_coins` returns the **fewest** coins that add up to the target, or -1 if it is impossible.
2. `count_ways` returns the number of different **multisets** of coins that add up to the target (order does not matter).

## Explanation

(a) For the US-style system greedy is optimal: 63 = 25 + 25 + 10 + 1 + 1 + 1, six coins. (b) For `{4, 3, 1}` and 6, greedy takes 4 + 1 + 1 (three coins) but 3 + 3 uses **two**: the greedy-choice property does *not* hold for arbitrary coin systems. (c) The DP builds `best[a]` for every amount `a` from smaller amounts: `best[a] = 1 + min(best[a - coin])`. It is correct because the problem has **optimal substructure** (an optimal solution for `a` contains an optimal solution for `a - coin`) and **overlapping subproblems** (the same amounts are needed repeatedly), and costs O(target × coins). To count *combinations*, the loop order matters: iterate coin types in the **outer** loop and amounts in the inner one, so each combination is counted once regardless of order; swapping the loops would count sequences (orderings) instead, giving a different number. There are 4 ways to make 10 from 1, 5 and 10.
