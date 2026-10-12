---
title: Greedy Algorithms
track: dsa
module: Algorithm Design
moduleOrder: 8
order: 2
level: advanced
summary: "When making the locally best choice is provably globally optimal — exchange arguments, interval scheduling, fractional knapsack, job sequencing and Huffman coding (with a canonical bit-level encoder) — tested against brute force, with the plausible greedy rules that fail."
sources:
  - label: Kleinberg & Tardos — Algorithm Design, Ch. 4 (Greedy Algorithms)
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 15 (Greedy Algorithms)
  - label: Huffman — A Method for the Construction of Minimum-Redundancy Codes (Proc. IRE 40(9), 1952)
  - label: Shannon — A Mathematical Theory of Communication (Bell System Technical Journal 27, 1948)
  - label: Edmonds — Matroids and the Greedy Algorithm (Mathematical Programming 1, 1971)
  - label: Liu & Layland — Scheduling Algorithms for Multiprogramming in a Hard-Real-Time Environment (J. ACM 20(1), 1973)
  - label: RFC 1951 — DEFLATE Compressed Data Format Specification version 1.3, §3.2.2 (canonical Huffman codes)
    url: https://www.rfc-editor.org/rfc/rfc1951
  - label: Linux kernel documentation — Deadline Task Scheduling (SCHED_DEADLINE)
    url: https://www.kernel.org/doc/html/latest/scheduler/sched-deadline.html
---

A **greedy algorithm** builds a solution by repeatedly making the choice that looks best *right now* and never reconsidering. It is the simplest and fastest algorithmic paradigm — usually a sort plus a scan, `O(n log n)` — and it is correct only for problems with special structure. The craft of greedy algorithms is therefore mostly about **proof**: knowing when the local choice is safe, and recognising when it is not (see the counterexamples below, and the coin-change failure in the dynamic-programming note).

## Why a greedy choice can be safe

Two standard proof techniques cover almost every case.

- **Greedy-choice property via an exchange argument.** Take any optimal solution `O`. Show that you can *swap* its first decision for the greedy one without making it worse. Then some optimal solution begins with the greedy choice, and by optimal substructure the rest of the problem is the same problem, smaller.
- **Greedy stays ahead.** Show that after each step the greedy solution is at least as good as any other partial solution by some measure, and conclude that it ends at least as good.

If a problem has optimal substructure but the exchange argument fails, you need dynamic programming. The theory of **matroids** (Edmonds, 1971) characterises exactly when the simple "sort by weight, take whatever stays independent" greedy is optimal: it covers minimum spanning trees (the previous graph note), scheduling with deadlines and several other problems, and is why Kruskal's algorithm works. Dijkstra's algorithm is also greedy: the closest unsettled vertex is final because weights are non-negative.

## Four classic problems

### Interval scheduling (activity selection)

Given intervals (meetings, jobs, requests), select the largest set that does not overlap. Of the many natural rules — earliest start, shortest duration, fewest conflicts, earliest finish — **only earliest finish time first is optimal**. Exchange argument: let `g` be the interval that ends first. In any optimal schedule, replace its first interval with `g`: `g` ends no later, so nothing that followed it can now conflict. A sibling problem, the **minimum number of rooms** (or machines, or platforms) needed to serve all intervals, equals the maximum number of simultaneously overlapping intervals — found by sorting starts and ends separately and sweeping.

### Fractional knapsack

If items can be split, take them in decreasing value-per-weight order, filling the last one partially. Exchange: if the knapsack contains a lower-ratio fragment while a higher-ratio item remains unfinished, swap weight between them to gain value. The proof breaks for the **0/1** version, where items cannot be split: the textbook counterexample is below, and 0/1 knapsack needs DP.

### Job sequencing with deadlines

Unit-time jobs have deadlines and profits; at most one job runs per time slot and a job earns profit only if it finishes by its deadline. Greedy: consider jobs in decreasing profit order and put each in the latest free slot at or before its deadline (skip if none). The "latest free slot ≤ d" query is exactly union–find with the pointer `p[t]` = latest free slot at or below `t`, giving near-linear time after the sort.

### Earliest-deadline-first scheduling

A real-time cousin: for a single processor and jobs with deadlines, running the job with the **earliest deadline** first schedules everything whenever any schedule can (Liu & Layland showed EDF is optimal for preemptive uniprocessor scheduling, schedulable up to 100 % utilisation). Linux implements it as the `SCHED_DEADLINE` policy.

{{include dsa/greedy/greedy.c}}

{{run dsa/greedy :: gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined greedy.c -o greedy && ./greedy}}

Each greedy is compared with exhaustive search on thousands of small random instances. The experiment also checks the *wrong* rules: "earliest start" and "shortest interval" — both sound reasonable — fail on a large share of random instances, which is exactly why a greedy rule must come with a proof rather than an intuition. And the 0/1 knapsack counterexample is computed, not just claimed: ratio-greedy gets 160, the optimum is 220.

## Huffman coding

Given symbol frequencies, find a **prefix code** (no codeword is a prefix of another, so a bit stream decodes unambiguously without separators) with minimum expected length. Huffman's greedy algorithm repeatedly merges the two least frequent subtrees into one whose weight is their sum; the merge order builds the code tree, and each symbol's codeword is its path from the root.

Why it is optimal (sketch): in an optimal tree the two least frequent symbols can be taken as siblings at the deepest level (exchange argument: swapping a rarer symbol deeper never increases cost), so merging them into a single "symbol" of combined weight reduces the problem to a smaller instance.

A second theorem bounds what is possible at all: **Shannon's source-coding theorem** says no symbol code can average fewer bits per symbol than the entropy `H = −Σ p_i log₂ p_i`, and Huffman's average length lies in `[H, H + 1)`.

**Canonical codes.** Only the *lengths* of an optimal code matter; any assignment of codes with those lengths works. The **canonical** assignment gives codes of equal length consecutive values in symbol order, so a decoder needs only the list of code lengths to reconstruct every codeword — which is why DEFLATE (RFC 1951 §3.2.2; used by gzip, zlib, PNG, ZIP) transmits just the lengths. It also allows a very small decoder: for each length `l`, store the first code and the index of its first symbol, and a code of length `l` is valid exactly when it falls in that length's consecutive block.

The program below builds the code lengths with a heap, assigns canonical codes, encodes two million symbols into an actual bit buffer, decodes it with the canonical table, and compares sizes with the entropy bound. It also cross-checks Huffman's cost against brute force over *every* length assignment that satisfies the Kraft equality for 2000 random small alphabets.

{{include dsa/greedy/huffman.c}}

{{run dsa/greedy :: gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined huffman.c -o huffman -lm && ./huffman}}

Observe: the measured average code length is about 4.48 bits per symbol, within 0.03 bits of the entropy `H ≈ 4.46` (the bound no symbol code can beat) and well under the 5 bits a fixed-width code for 30 symbols needs; the encoded stream is about 56 % of the original bytes; the Kraft sum is exactly 1 (a *complete* prefix code); the decoded stream matches the original bit for bit; and the greedy cost equals the brute-force optimum on all 2000 small alphabets.

An implementation detail worth recording: the first version of this program sized the output buffer with a guess (`N·4/8` bytes) that was too small for a 4.48 bit/symbol code; **AddressSanitizer reported the heap overflow at the first run**. Sizing buffers from the computed bit count is the fix. Bit-packing code is a prime place for off-by-one buffer errors; test it under sanitizers.

Practical notes: real formats **limit the code length** (DEFLATE: 15 bits) using a package–merge or heuristic adjustment; adaptive and arithmetic/range/ANS coders beat Huffman by coding fractional bits per symbol (Huffman cannot go below 1 bit for even a near-certain symbol); and JPEG, MP3 and PNG all use Huffman-style entropy coding as one stage.

## Greedy that fails

| Greedy rule | Fails on |
|---|---|
| Coin change: largest coin first | coins `{4,3,1}`, amount 6 (greedy 4+1+1, optimal 3+3); works for "canonical" systems such as 1, 5, 10, 25 |
| 0/1 knapsack by value/weight ratio | the (10,60), (20,100), (30,120) example above |
| Interval scheduling by earliest start or shortest length | the random instances above |
| Shortest path by always taking the cheapest next edge | graphs where an expensive first edge leads to cheap rest (Dijkstra tracks *accumulated* distance, not edge cost) |
| Travelling salesman: nearest neighbour | by arbitrary factors (it has no constant approximation guarantee) |
| Graph colouring: first fit | can use `n/2` colours on a bipartite graph in bad vertex order |

Greedy is also a respectable **approximation** technique when exact greedy fails: greedy set cover is within a factor `ln n` of optimal (and that is essentially the best possible unless P = NP); greedy for submodular maximisation gives `1 − 1/e`; Graham's list scheduling is a 2-approximation for makespan.

## Pitfalls

- **Intuition is not proof.** Every greedy rule sounds plausible; write the exchange argument, or find a counterexample by brute force on tiny random inputs — a fifteen-line harness like the one above finds most wrong rules in seconds.
- **Ties and sort stability**: a wrong tie-break (finish time equal, which first?) can matter for some variants; define the order completely.
- **Sorting key overflow**: `a - b` comparators on large values overflow; compare with `<` and `>`.
- **Floating-point ratios** in fractional-knapsack-style problems: compare by cross-multiplication (`v1*w2 > v2*w1`) when exactness matters.
- **Closed vs half-open intervals**: whether `[1,3]` and `[3,5]` overlap changes the answer; the code uses half-open intervals.
- **Using greedy where the problem is NP-hard** and presenting the result as optimal; label heuristics as heuristics.
- **Buffer sizing in bit-level encoders** (above): derive the size from the data, not from an assumed ratio.

## Key takeaways

1. A greedy algorithm needs a proof — an exchange argument or "stays ahead" — not just a plausible rule; the wrong-rule experiment shows how often intuition fails.
2. Interval scheduling: sort by finish time. Rooms needed = maximum overlap. Fractional knapsack: sort by ratio. Deadline scheduling: EDF, and union–find for slot lookup.
3. Huffman's merge-two-lightest rule gives an optimal prefix code within one bit of entropy; canonical codes let a format store only code lengths (DEFLATE).
4. Many core graph algorithms (Kruskal, Prim, Dijkstra) are greedy, justified by the cut property and matroid theory.
5. When greedy fails (0/1 knapsack, general coin change), the problem usually needs dynamic programming or is NP-hard.
6. Always cross-check a greedy solution against brute force on small random inputs, and size bit buffers from computed lengths.
