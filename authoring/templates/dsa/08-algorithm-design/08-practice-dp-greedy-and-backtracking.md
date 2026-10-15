---
title: "Practice: Dynamic Programming, Greedy and Backtracking"
track: dsa
module: Algorithm Design
moduleOrder: 8
order: 8
level: advanced
summary: "Four exercises on algorithm design: where greedy coin change fails and dynamic programming succeeds, counting ways and edit distance with a DP table, choosing the right greedy rule for interval scheduling, and counting n-queens solutions by backtracking with pruning, each with a verified C answer."
prerequisites:
  - "Dynamic Programming"
  - "Greedy Algorithms"
  - "Backtracking, Branch and Bound, and Bitmask DP"
sources:
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 14 (Dynamic Programming) and Ch. 15 (Greedy Algorithms)
  - label: Levenshtein, V. I. — Binary codes capable of correcting deletions, insertions and reversals (1966)
  - label: Kleinberg & Tardos — Algorithm Design, Ch. 4 (Greedy) and Ch. 6 (Dynamic Programming)
  - label: Knuth — The Art of Computer Programming, Vol. 4B, §7.2.2 (Backtrack programming)
---

Algorithm design is mostly choosing a strategy and knowing when it is valid. These exercises force that choice: each one has a tempting wrong strategy as well as a right one.

## Exercises

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Coin change.** (a) For coins `{25, 10, 5, 1}` and target 63, how many coins does the "take the biggest coin that fits" strategy use? (b) For coins `{4, 3, 1}` and target 6, does that strategy find the minimum? (c) Write a dynamic-programming solution for the minimum number of coins, then count the number of **different combinations** (order does not matter) that make 10 from `{1, 5, 10}`.

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/design/ex1-coins.c}}

</details>

**2. Edit distance.** Compute the minimum number of insertions, deletions and substitutions needed to turn `kitten` into `sitting`, and `intention` into `execution`. State the meaning of each cell of your table and the recurrence.

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/design/ex2-edit-distance.c}}

</details>

**3. Interval scheduling.** Seven jobs have start and end times (`A 0-9, B 1-3, C 2-5, D 4-6, E 6-8, F 5-7, G 8-10`). Choose the maximum number of non-overlapping jobs. Try two greedy rules, "earliest start first" and "earliest finish first". Which is correct and why?

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/design/ex3-intervals.c}}

</details>

**4. Backtracking.** Count the ways to place *n* queens on an *n*×*n* board with none attacking another, for *n* = 4 to 8. What pruning makes it fast, and why must the choice be undone after the recursive call?

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/design/ex4-queens.c}}

</details>

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

{{include dsa/practice/p8_coins.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p8_coins p8_coins.c && ./p8_coins; rm -f p8_coins}}

(a) For the US-style system greedy is optimal: 63 = 25 + 25 + 10 + 1 + 1 + 1, six coins. (b) For `{4, 3, 1}` and 6, greedy takes 4 + 1 + 1 (three coins) but 3 + 3 uses **two**: the greedy-choice property does *not* hold for arbitrary coin systems. (c) The DP builds `best[a]` for every amount `a` from smaller amounts: `best[a] = 1 + min(best[a - coin])`. It is correct because the problem has **optimal substructure** (an optimal solution for `a` contains an optimal solution for `a - coin`) and **overlapping subproblems** (the same amounts are needed repeatedly), and costs O(target × coins). To count *combinations*, the loop order matters: iterate coin types in the **outer** loop and amounts in the inner one, so each combination is counted once regardless of order; swapping the loops would count sequences (orderings) instead, giving a different number. There are 4 ways to make 10 from 1, 5 and 10.

**2.**

{{include dsa/practice/p8_edit.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p8_edit p8_edit.c && ./p8_edit; rm -f p8_edit}}

`d[i][j]` is the edit distance between the first `i` characters of `a` and the first `j` of `b`. The base row and column are the cost of deleting or inserting everything. Each cell takes the cheapest of three moves: delete `a[i-1]` (from `d[i-1][j]`), insert `b[j-1]` (from `d[i][j-1]`), or substitute (from `d[i-1][j-1]`, free when the characters match). `kitten` to `sitting` is 3 (substitute k→s, substitute e→i, insert g) and `intention` to `execution` is 5. The table has O(n·m) cells, each O(1); keeping only two rows reduces memory to O(m) (but then you cannot recover the edit script).

**3.**

{{include dsa/practice/p8_intervals.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p8_intervals p8_intervals.c && ./p8_intervals; rm -f p8_intervals}}

**Earliest finish first** is correct: it picks B, D, E, G (4 jobs). "Earliest start first" picks `A` (which starts at 0 but runs until 9), blocking everything else, for 1 job. The exchange argument for the right rule: in any optimal schedule, replace the first job by the job that finishes earliest; it ends no later, so everything after it still fits, and the schedule is no worse. Repeating this argument shows the greedy choice is safe at every step. Time is O(n log n) for the sort. Other tempting rules also fail: shortest job first (a short job in the middle can block two others) and fewest conflicts first.

**4.**

{{include dsa/practice/p8_queens.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p8_queens p8_queens.c && ./p8_queens; rm -f p8_queens}}

The counts for n = 4 to 8 are 2, 10, 4, 40 and 92. The search places one queen per row; before recursing, it skips any column or diagonal already attacked. That **pruning** abandons a whole subtree of placements as soon as one conflict appears: for n = 8 there are about 17 million ways to put one queen in each row (8^8), but far fewer partial placements survive the checks. The three boolean arrays make the test O(1): a queen at (row, col) occupies column `col`, diagonal `row + col` and anti-diagonal `row - col`. After exploring a choice you must **undo** it (the last line of the loop), otherwise the marks of an abandoned branch would block later, unrelated branches: this choose-explore-unchoose rhythm is the signature of backtracking.

</details>

## Key takeaways

1. Greedy needs a proof (an exchange argument) for each problem; when the greedy-choice property fails, as with coins `{4, 3, 1}`, use dynamic programming.
2. DP needs optimal substructure and overlapping subproblems; define what each table cell means, and mind loop order when counting combinations versus sequences.
3. For interval scheduling, sort by finish time; a wrong greedy rule can be arbitrarily bad.
4. Backtracking is choose, explore, un-choose, and pruning impossible branches early is what makes it practical.
