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
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 14 (Dynamic Programming) and Ch. 15 (Greedy Algorithms)"
  - label: "Levenshtein, V. I. — Binary codes capable of correcting deletions, insertions and reversals (1966)"
  - label: "Kleinberg & Tardos — Algorithm Design, Ch. 4 (Greedy) and Ch. 6 (Dynamic Programming)"
  - label: "Knuth — The Art of Computer Programming, Vol. 4B, §7.2.2 (Backtrack programming)"
---

Algorithm design is mostly choosing a strategy and knowing when it is valid. These exercises force that choice: each one has a tempting wrong strategy as well as a right one.

## Exercises

**1. Coin change.** (a) For coins `{25, 10, 5, 1}` and target 63, how many coins does the "take the biggest coin that fits" strategy use? (b) For coins `{4, 3, 1}` and target 6, does that strategy find the minimum? (c) Write a dynamic-programming solution for the minimum number of coins, then count the number of **different combinations** (order does not matter) that make 10 from `{1, 5, 10}`.

**2. Edit distance.** Compute the minimum number of insertions, deletions and substitutions needed to turn `kitten` into `sitting`, and `intention` into `execution`. State the meaning of each cell of your table and the recurrence.

**3. Interval scheduling.** Seven jobs have start and end times (`A 0-9, B 1-3, C 2-5, D 4-6, E 6-8, F 5-7, G 8-10`). Choose the maximum number of non-overlapping jobs. Try two greedy rules, "earliest start first" and "earliest finish first". Which is correct and why?

**4. Backtracking.** Count the ways to place *n* queens on an *n*×*n* board with none attacking another, for *n* = 4 to 8. What pruning makes it fast, and why must the choice be undone after the recursive call?

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

```c
#include <stdio.h>
#include <limits.h>

#define INF INT_MAX

/* Minimum number of coins for each amount 0..target by dynamic programming (bottom-up). */
static int min_coins(const int *coins, int n, int target) {
    int best[64];
    best[0] = 0;
    for (int a = 1; a <= target; a++) {
        best[a] = INF;
        for (int i = 0; i < n; i++)
            if (coins[i] <= a && best[a - coins[i]] != INF && best[a - coins[i]] + 1 < best[a]) best[a] = best[a - coins[i]] + 1;
    }
    return best[target] == INF ? -1 : best[target];
}

static int greedy_coins(const int *coins_desc, int n, int target) {      /* take the biggest coin that fits, repeatedly */
    int count = 0;
    for (int i = 0; i < n; i++) while (target >= coins_desc[i]) { target -= coins_desc[i]; count++; }
    return target == 0 ? count : -1;
}

/* Number of ORDER-INDEPENDENT ways to make the amount (each coin type considered once, in an outer loop). */
static long ways(const int *coins, int n, int target) {
    long w[64] = {1};
    for (int i = 0; i < n; i++)
        for (int a = coins[i]; a <= target; a++) w[a] += w[a - coins[i]];
    return w[target];
}

int main(void) {
    const int us[] = {25, 10, 5, 1}, odd[] = {4, 3, 1};
    printf("coins 25,10,5,1 target 63: greedy %d, DP %d\n", greedy_coins(us, 4, 63), min_coins(us, 4, 63));
    printf("coins 4,3,1     target  6: greedy %d, DP %d   (greedy takes 4+1+1, DP finds 3+3)\n", greedy_coins(odd, 3, 6), min_coins(odd, 3, 6));
    printf("ways to make 10 from 1,5,10: %ld\n", ways((const int[]){1, 5, 10}, 3, 10));
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p8_coins p8_coins.c && ./p8_coins; rm -f p8_coins
coins 25,10,5,1 target 63: greedy 6, DP 6
coins 4,3,1     target  6: greedy 3, DP 2   (greedy takes 4+1+1, DP finds 3+3)
ways to make 10 from 1,5,10: 4
```

(a) For the US-style system greedy is optimal: 63 = 25 + 25 + 10 + 1 + 1 + 1, six coins. (b) For `{4, 3, 1}` and 6, greedy takes 4 + 1 + 1 (three coins) but 3 + 3 uses **two**: the greedy-choice property does *not* hold for arbitrary coin systems. (c) The DP builds `best[a]` for every amount `a` from smaller amounts: `best[a] = 1 + min(best[a - coin])`. It is correct because the problem has **optimal substructure** (an optimal solution for `a` contains an optimal solution for `a - coin`) and **overlapping subproblems** (the same amounts are needed repeatedly), and costs O(target × coins). To count *combinations*, the loop order matters: iterate coin types in the **outer** loop and amounts in the inner one, so each combination is counted once regardless of order; swapping the loops would count sequences (orderings) instead, giving a different number. There are 4 ways to make 10 from 1, 5 and 10.

**2.**

```c
#include <stdio.h>
#include <string.h>

static int min3(int a, int b, int c) { int m = a < b ? a : b; return m < c ? m : c; }

/* Edit distance: fewest insertions, deletions and substitutions turning a into b. d[i][j] = distance of the prefixes a[0..i), b[0..j). */
static int edit_distance(const char *a, const char *b) {
    int n = (int)strlen(a), m = (int)strlen(b);
    int d[32][32];
    for (int i = 0; i <= n; i++) d[i][0] = i;                    /* delete all of a's prefix */
    for (int j = 0; j <= m; j++) d[0][j] = j;                    /* insert all of b's prefix */
    for (int i = 1; i <= n; i++)
        for (int j = 1; j <= m; j++)
            d[i][j] = min3(d[i - 1][j] + 1,                      /* delete a[i-1] */
                           d[i][j - 1] + 1,                      /* insert b[j-1] */
                           d[i - 1][j - 1] + (a[i - 1] != b[j - 1]));   /* substitute (cost 0 if equal) */
    return d[n][m];
}

int main(void) {
    const char *pairs[][2] = {{"kitten", "sitting"}, {"flaw", "lawn"}, {"", "abc"}, {"same", "same"}, {"intention", "execution"}};
    for (int i = 0; i < 5; i++) printf("edit_distance(\"%s\", \"%s\") = %d\n", pairs[i][0], pairs[i][1], edit_distance(pairs[i][0], pairs[i][1]));
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p8_edit p8_edit.c && ./p8_edit; rm -f p8_edit
edit_distance("kitten", "sitting") = 3
edit_distance("flaw", "lawn") = 2
edit_distance("", "abc") = 3
edit_distance("same", "same") = 0
edit_distance("intention", "execution") = 5
```

`d[i][j]` is the edit distance between the first `i` characters of `a` and the first `j` of `b`. The base row and column are the cost of deleting or inserting everything. Each cell takes the cheapest of three moves: delete `a[i-1]` (from `d[i-1][j]`), insert `b[j-1]` (from `d[i][j-1]`), or substitute (from `d[i-1][j-1]`, free when the characters match). `kitten` to `sitting` is 3 (substitute k→s, substitute e→i, insert g) and `intention` to `execution` is 5. The table has O(n·m) cells, each O(1); keeping only two rows reduces memory to O(m) (but then you cannot recover the edit script).

**3.**

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct { int start, end; const char *name; } Job;

static int by_end(const void *x, const void *y) { return ((const Job *)x)->end - ((const Job *)y)->end; }
static int by_start(const void *x, const void *y) { return ((const Job *)x)->start - ((const Job *)y)->start; }

static int pick(Job *jobs, int n, const char *label) {
    int count = 0, last_end = -1;
    printf("%-22s", label);
    for (int i = 0; i < n; i++)
        if (jobs[i].start >= last_end) { printf(" %s", jobs[i].name); last_end = jobs[i].end; count++; }
    printf("  -> %d jobs\n", count);
    return count;
}

int main(void) {
    Job jobs[] = {{0, 9, "A"}, {1, 3, "B"}, {2, 5, "C"}, {4, 6, "D"}, {6, 8, "E"}, {5, 7, "F"}, {8, 10, "G"}};
    int n = (int)(sizeof jobs / sizeof jobs[0]);
    qsort(jobs, (size_t)n, sizeof jobs[0], by_end);
    pick(jobs, n, "earliest finish first:");
    qsort(jobs, (size_t)n, sizeof jobs[0], by_start);
    pick(jobs, n, "earliest start first:");
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p8_intervals p8_intervals.c && ./p8_intervals; rm -f p8_intervals
earliest finish first: B D E G  -> 4 jobs
earliest start first:  A  -> 1 jobs
```

**Earliest finish first** is correct: it picks B, D, E, G (4 jobs). "Earliest start first" picks `A` (which starts at 0 but runs until 9), blocking everything else, for 1 job. The exchange argument for the right rule: in any optimal schedule, replace the first job by the job that finishes earliest; it ends no later, so everything after it still fits, and the schedule is no worse. Repeating this argument shows the greedy choice is safe at every step. Time is O(n log n) for the sort. Other tempting rules also fail: shortest job first (a short job in the middle can block two others) and fewest conflicts first.

**4.**

```c
#include <stdio.h>

static int n, solutions;
static int col_used[16], diag1[32], diag2[32];

static void place(int row) {
    if (row == n) { solutions++; return; }
    for (int c = 0; c < n; c++) {
        if (col_used[c] || diag1[row + c] || diag2[row - c + n - 1]) continue;       /* attacked: prune this whole branch */
        col_used[c] = diag1[row + c] = diag2[row - c + n - 1] = 1;                   /* choose */
        place(row + 1);                                                              /* explore */
        col_used[c] = diag1[row + c] = diag2[row - c + n - 1] = 0;                   /* un-choose: backtrack */
    }
}

int main(void) {
    for (n = 4; n <= 8; n++) { solutions = 0; place(0); printf("%d-queens: %d solutions\n", n, solutions); }
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p8_queens p8_queens.c && ./p8_queens; rm -f p8_queens
4-queens: 2 solutions
5-queens: 10 solutions
6-queens: 4 solutions
7-queens: 40 solutions
8-queens: 92 solutions
```

The counts for n = 4 to 8 are 2, 10, 4, 40 and 92. The search places one queen per row; before recursing, it skips any column or diagonal already attacked. That **pruning** abandons a whole subtree of placements as soon as one conflict appears: for n = 8 there are about 17 million ways to put one queen in each row (8^8), but far fewer partial placements survive the checks. The three boolean arrays make the test O(1): a queen at (row, col) occupies column `col`, diagonal `row + col` and anti-diagonal `row - col`. After exploring a choice you must **undo** it (the last line of the loop), otherwise the marks of an abandoned branch would block later, unrelated branches: this choose-explore-unchoose rhythm is the signature of backtracking.

</details>

## Key takeaways

1. Greedy needs a proof (an exchange argument) for each problem; when the greedy-choice property fails, as with coins `{4, 3, 1}`, use dynamic programming.
2. DP needs optimal substructure and overlapping subproblems; define what each table cell means, and mind loop order when counting combinations versus sequences.
3. For interval scheduling, sort by finish time; a wrong greedy rule can be arbitrarily bad.
4. Backtracking is choose, explore, un-choose, and pruning impossible branches early is what makes it practical.
