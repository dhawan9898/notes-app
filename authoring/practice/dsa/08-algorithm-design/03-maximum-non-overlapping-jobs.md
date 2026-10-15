---
kind: code
title: Maximum number of jobs
module: Algorithm Design
difficulty: Medium
tags:
  - greedy
  - intervals
starter: dsa/starters/design/ex3-intervals.c
examples:
  - input: A(0,9) B(1,3) C(2,5) D(4,6) E(6,8) F(5,7) G(8,10)
    output: '4'
    why: For example B, D, E, G.
  - input: (2,3) (0,1) (1,2)
    output: '3'
    why: Back-to-back jobs all fit.
  - input: no jobs
    output: '0'
constraints:
  - '`n >= 0`.'
  - The greedy choice needs the jobs sorted by end time.
hints:
  - Sort by end time (use `qsort` with a safe comparator).
  - Take a job when its start is at least the end of the last chosen job.
---

Each job has a `start` and an `end`. Two jobs can run one after another if one **starts when the other ends** (touching is allowed). Return the maximum number of jobs that can be scheduled; you may reorder the array.

## Explanation

**Earliest finish first** is correct: it picks B, D, E, G (4 jobs). "Earliest start first" picks `A` (which starts at 0 but runs until 9), blocking everything else, for 1 job. The exchange argument for the right rule: in any optimal schedule, replace the first job by the job that finishes earliest; it ends no later, so everything after it still fits, and the schedule is no worse. Repeating this argument shows the greedy choice is safe at every step. Time is O(n log n) for the sort. Other tempting rules also fail: shortest job first (a short job in the middle can block two others) and fewest conflicts first.
