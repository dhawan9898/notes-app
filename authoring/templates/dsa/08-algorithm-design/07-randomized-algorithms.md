---
title: "Randomized Algorithms: Las Vegas, Monte Carlo and Why Randomness Helps"
track: dsa
module: Algorithm Design
moduleOrder: 8
order: 7
level: advanced
summary: "Algorithms that flip coins: Las Vegas (always right, random time: randomized quicksort and quickselect) and Monte Carlo (fast, small chance of error: Freivalds' matrix check, Miller-Rabin, Karger's min cut, reservoir sampling), with each probability bound measured, plus how to generate random numbers correctly and how to reason about expected cost and amplification."
prerequisites:
  - "Comparison Sorts — Insertion, Merge, Quick, Heap and Introsort"
  - Number Theory Essentials
  - "Hash Tables: Open Addressing, Deletion, Cuckoo and Hash Flooding"
sources:
  - label: "Motwani & Raghavan — Randomized Algorithms (Cambridge University Press, 1995)"
  - label: "Mitzenmacher & Upfal — Probability and Computing (2nd ed., Cambridge University Press, 2017)"
  - label: "Freivalds — Probabilistic machines can use less running time (IFIP Congress 1977)"
  - label: "Rabin — Probabilistic algorithm for testing primality (Journal of Number Theory 12, 1980)"
    url: https://www.sciencedirect.com/science/article/pii/0022314X80900840
  - label: "Karger — Global min-cuts in RNC, and other ramifications of a simple min-cut algorithm (SODA 1993)"
  - label: "Vitter — Random sampling with a reservoir (ACM TOMS 11(1), 1985)"
    url: https://dl.acm.org/doi/10.1145/3147.3165
  - label: "Hoare — Algorithm 65: Find (CACM 4(7), 1961), quickselect"
---

A **randomized algorithm** uses random bits as part of its logic. Randomness is not a trick for the lazy: for some problems the best known algorithm is randomized (or randomization makes a simple algorithm competitive with a complicated deterministic one), and for others, such as defending against an adversary who chooses the input, nothing deterministic can work. Two flavours matter.

| | Las Vegas | Monte Carlo |
|---|---|---|
| Answer | always correct | correct with probability ≥ 1 − ε |
| Running time | random (a random variable) | fixed or bounded |
| Analysis | expected running time, tail bounds | error probability, repetition |
| Examples | randomized quicksort, quickselect, hash tables with random hash functions, treaps | Miller–Rabin, Freivalds, Karger min-cut, Bloom filters |
| Conversion | stop after a time limit and you have a Monte Carlo algorithm | verify the answer and repeat until it is right: a Las Vegas algorithm, when verification is cheap |

## Las Vegas: randomizing the pivot

Quicksort with a fixed rule (always the first or last element as the pivot) has a worst case of O(n²), and the bad inputs are not exotic: *sorted* or *reversed* data makes every pivot the smallest or largest element. An adversary (or merely an unlucky data source) can force it every time. Choosing the pivot **uniformly at random** changes the guarantee: for *every* input, the expected number of comparisons is about **2n ln n ≈ 1.39 n log₂ n**, because the probability that two elements `i < j` are ever compared is `2/(j − i + 1)` (they are compared only if one of them is the first pivot chosen among the elements between them), and summing over all pairs gives `2n H_n`. No input is bad; only unlucky coin flips are, and the probability of a running time much more than the expectation falls off exponentially.

**Quickselect** finds the k-th smallest element by partitioning as quicksort does but recursing into only one side. With random pivots the expected cost is at most about **3.4 n** comparisons for a random rank (and 2n for the median in the best analysis): O(n) expected, versus O(n log n) to sort first. The deterministic **median of medians** variant guarantees O(n) worst case but with large constants, so practical libraries use quickselect with introspection (fall back to heap-based selection if recursion gets deep).

{{include dsa/randomized/rnd.h}}

{{include dsa/randomized/lasvegas.c}}

{{run dsa/randomized :: gcc -O2 -Wall -Wextra lasvegas.c -o lv -lm && ./lv}}

Reading the results:

- Sorted input with a last-element pivot costs **199,990,000** comparisons, exactly n(n−1)/2 for n = 20,000: every partition peels off one element.
- The same input with a random pivot costs about 340,000: within a few percent of the predicted 2n ln n ≈ 396,000 minus lower-order terms, regardless of the input order (sorted and reversed behave alike).
- Over 200 runs the cost varies between roughly 310,000 and 394,000: **the running time is a random variable, but the output is always sorted.** That is what Las Vegas means.
- Quickselect found the correct k-th element in all 200 random trials (checked against a full sort), with about 3 comparisons per element, matching the bound.

**Generating random numbers correctly** is part of the algorithm. `rnd_below(n)` above uses rejection to avoid *modulo bias*: `x % n` for a 64-bit `x` slightly favours small residues unless `n` divides 2⁶⁴, which for large `n` is a measurable skew. Use a good generator (xorshift64, PCG, `getrandom` for security) and never `rand() % n` when quality matters. Shuffling uses **Fisher–Yates** (swap element *i* with a random element of positions 0..i), which produces each permutation with equal probability; the common bug of swapping each element with a random element of the *whole* array makes some permutations more likely than others.

## Monte Carlo: trading certainty for speed

A Monte Carlo algorithm with one-sided error (it can be wrong in only one direction) is cheap to **amplify**: repeat it independently *t* times; if the error probability of one run is at most *p*, the chance that all runs are wrong is at most *p^t*. Twenty repetitions of an algorithm that is wrong half the time give an error of one in a million; at fifty repetitions the failure is rarer than a hardware fault in the computer running it.

### Freivalds: verifying a matrix product

Checking whether `A·B = C` for n×n matrices by recomputing the product costs O(n³) (O(n^2.37) with the best theory). **Freivalds' algorithm** picks a random 0/1 vector `r` and compares `A·(B·r)` with `C·r`: three matrix-vector products, **O(n²)**. If `A·B = C` the test always agrees. If `A·B ≠ C`, the vector `(AB − C)·r` is nonzero with probability at least ½ (a nonzero row of the difference is a linear equation that a random 0/1 vector satisfies with probability at most ½). So the error is one-sided, at most ½ per trial.

### Miller–Rabin: primality testing

Fermat's test (does `a^(n−1) ≡ 1 mod n`?) is fooled by Carmichael numbers such as 561. **Miller–Rabin** writes `n − 1 = 2^s · d` and checks that the sequence `a^d, a^(2d), …, a^((n−1)/2)` either starts with 1 or contains `n − 1`; any other pattern proves `n` composite. A prime always passes. For any odd composite, at most a quarter of the bases `a` are liars, so *k* random bases give error at most 4^−k. (For 64-bit integers a fixed set of 12 bases gives a deterministic, always-correct test; for larger numbers random bases remain the practical choice.)

### Karger: minimum cut by contraction

The **minimum cut** of a graph is the fewest edges whose removal disconnects it. **Karger's algorithm** repeatedly picks a uniformly random edge and *contracts* its endpoints (merges them into one vertex) until two super-vertices remain; the edges between them form a cut. A fixed minimum cut survives all contractions with probability at least `2/(n(n−1))`, because contracting a random edge hits a min-cut edge with small probability when the min cut is small compared with the number of edges. A single run is rarely right for large graphs, but repeating it `O(n² log n)` times finds a minimum cut with high probability, and the Karger–Stein recursion improves the total to roughly `O(n² log³ n)`.

### Reservoir sampling

To pick *k* items uniformly from a stream whose length is unknown, keep the first *k*; then for item number `i` (counting from 0) generate `j` uniformly in `[0, i]` and, if `j < k`, replace the j-th stored item. Each item ends up in the sample with probability exactly `k/n`, using O(k) memory and one pass. (Weighted and distributed variants exist; Vitter's skipping variants avoid generating a random number per item.)

{{include dsa/randomized/montecarlo.c}}

{{run dsa/randomized :: gcc -O2 -Wall -Wextra montecarlo.c -o mc -lm && ./mc}}

How the measurements line up with the theory:

- **Freivalds**: a product corrupted in a single entry is wrongly accepted by almost exactly 50 % of single trials (the bound is tight when only one entry is wrong) and by about 3 % of 5-trial runs (the bound is (½)⁵ = 3.1 %). The correct product is always accepted.
- **Miller–Rabin**: the Carmichael number 561, which fools Fermat's test for every base coprime to it, is rejected by Miller–Rabin on all but a few percent of single random bases. The strong pseudoprime 3,215,031,751 (composite, but a strong liar for all prime bases up to 11) is mistaken for a prime by 24.5 % of random single bases, essentially the worst-case bound of 25 %. With 20 bases both are rejected, and the 64-bit prime 2⁶⁴ − 59 is correctly accepted.
- **Karger**: on two 6-cliques joined by three edges the minimum cut is 3. One contraction run finds it about 19 % of the time, far above the worst-case guarantee of 2/(12·11) = 1.5 %, because the graph has a clear structure; 100 runs find it essentially always.
- **Reservoir sampling**: over 200,000 trials of sampling 5 of 40 items, every item was selected about 25,000 times with a maximum deviation of about 1.5 %, and the chi-square statistic is close to its expected value of 39 (the number of degrees of freedom).

## More places randomness helps

- **Hashing**: choosing the hash function at random from a universal family defeats adversarial inputs (see the hash table note's flooding experiment); **treaps** and **skip lists** use random priorities and levels to stay balanced without rotations; **Bloom filters** and **count-min sketches** are Monte Carlo data structures (see the probabilistic structures note).
- **Hashing for similarity**: MinHash and locality-sensitive hashing estimate set similarity and find near neighbours.
- **Load balancing**: "power of two choices" (pick two servers at random, use the less loaded) reduces the maximum load from `log n / log log n` to `log log n`.
- **Randomized rounding** and **random sampling** for approximation algorithms; **Monte Carlo simulation** and **MCMC** for numerical estimates (error shrinks as 1/√samples).
- **Property-based testing and fuzzing** are randomized search for counterexamples; seeds must be logged so failures can be reproduced.

## Practical advice

- **Seed deliberately**: a fixed seed gives reproducible runs (essential for tests and debugging); seed from the OS (`getrandom`, `/dev/urandom`) when unpredictability is required, such as hash-table seeds against flooding.
- **Test randomized code statistically**: check correctness always (Las Vegas outputs), measure error rates against the proven bound (Monte Carlo), and use enough trials: a bound violated once in a million runs shows only with millions of runs.
- **Use the right generator**: the C library's `rand()` is low quality and not thread-safe; xorshift or PCG is fine for algorithms; use a cryptographic source for security.
- **Independence matters**: the amplification argument `p^t` needs independent random bits for each repetition; reusing a poor generator with correlated outputs invalidates it.
- **Derandomization** (conditional expectations, pairwise independence, expander walks) turns many randomized algorithms into deterministic ones when guarantees must be absolute, usually at a cost in simplicity.

## Key takeaways

- Las Vegas algorithms are always correct with random running time (randomized quicksort: expected 2n ln n comparisons for every input); Monte Carlo algorithms are fast with a small, controllable error probability.
- One-sided error is amplified exponentially by independent repetition: error ≤ p^t.
- Freivalds checks a matrix product in O(n²), Miller–Rabin tests primality with error ≤ 4^−k, Karger finds a min cut by random contraction, and reservoir sampling draws a uniform sample from a stream in O(k) memory.
- Generate random numbers without modulo bias, shuffle with Fisher–Yates, and verify claimed probabilities experimentally.
