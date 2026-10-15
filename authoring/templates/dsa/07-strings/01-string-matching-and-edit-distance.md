---
title: String Matching and Edit Distance
track: dsa
module: Strings
moduleOrder: 7
order: 1
level: advanced
summary: "Naive, KMP, Z, Rabin-Karp, Horspool, glibc memmem and Aho-Corasick on 32 MB of text (measured, including the adversarial case), then Levenshtein distance, LCS, banded DP, and the byte-versus-code-point trap."
prerequisites:
  - Recursion, Divide and Conquer, and Loop Invariants
  - "Hash Tables: Open Addressing, Deletion, Cuckoo and Hash Flooding"
sources:
  - label: Knuth, Morris, Pratt — Fast Pattern Matching in Strings (SIAM J. Computing 6(2), 1977)
  - label: Boyer & Moore — A Fast String Searching Algorithm (CACM 20(10), 1977)
  - label: Horspool — Practical Fast Searching in Strings (Software, Practice and Experience 10(6), 1980)
  - label: Karp & Rabin — Efficient Randomized Pattern-Matching Algorithms (IBM J. Research and Development 31(2), 1987)
  - label: Aho & Corasick — Efficient String Matching, an Aid to Bibliographic Search (CACM 18(6), 1975)
  - label: Crochemore & Perrin — Two-Way String-Matching (J. ACM 38(3), 1991)
  - label: Wagner & Fischer — The String-to-String Correction Problem (J. ACM 21(1), 1974)
  - label: Myers — An O(ND) Difference Algorithm and Its Variations (Algorithmica 1, 1986)
  - label: Navarro — A Guided Tour to Approximate String Matching (ACM Computing Surveys 33(1), 2001)
  - label: Gusfield — Algorithms on Strings, Trees and Sequences (Cambridge University Press, 1997)
  - label: Unicode Standard Annex #15 — Unicode Normalization Forms
    url: https://www.unicode.org/reports/tr15/
---

Searching text is one of the oldest and most heavily optimised problems in computing: `grep`, `strstr`, editors, intrusion-detection systems, DNA aligners, spam filters and version-control diffs all depend on it. The naive method is fine until it isn't, so this note works through the classic algorithms, measures them on real data (including the input that breaks the naive scan), and then moves from *exact* matching to *approximate* matching with edit distance.

## Exact matching: one pattern in one text

Let the text have length `n` and the pattern length `m`. Matching algorithms differ in what they remember about the pattern.

| Algorithm | Preprocessing | Search | Idea |
|---|---|---|---|
| Naive | none | `O(nm)` worst case | slide the pattern one position at a time |
| **KMP** | `O(m)` | `O(n)` guaranteed | on mismatch, jump using the pattern's own prefix structure; text pointer never moves back |
| **Z-algorithm** | `O(n + m)` | `O(n + m)` | compute the longest common prefix of every suffix with the pattern-and-text concatenation |
| **Rabin–Karp** | `O(m)` | `O(n)` expected | compare rolling hashes; verify on hit |
| **Boyer–Moore–Horspool** | `O(m + σ)` | `O(n/m)` best, `O(nm)` worst | compare from the right; skip ahead by the bad-character table |
| **Two-Way** (Crochemore–Perrin) | `O(m)` | `O(n)`, constant extra space | critical factorisation of the pattern; used in glibc |
| **Aho–Corasick** | `O(Σ m_i)` | `O(n + matches)` | trie + failure links for *many* patterns at once |

{{include dsa/strings/match.h}}

### KMP in one paragraph

After matching `k` characters and then failing, the naive scan restarts from scratch one position later, though it already *knows* the text equals the pattern's first `k` characters. KMP precomputes `fail[k]`: the length of the longest proper prefix of `p[0..k)` that is also a suffix of it. On a mismatch at pattern position `k` the matcher falls back to `fail[k]` — the longest prefix that can still be a valid partial match — without re-reading text. Each text character is examined a bounded number of times: the pointer `k` rises at most once per text character and every fallback strictly lowers it, so the total work is at most `2n` steps. The Z-algorithm achieves the same bound with a different table (`z[i]` = length of the common prefix of the string and its suffix starting at `i`), and is often easier to get right.

### Rabin–Karp and rolling hashes

Rabin–Karp treats a window of `m` characters as a number in some base modulo a prime. Sliding the window by one position updates the hash in `O(1)`: subtract the leaving character's contribution, multiply by the base, add the entering character. Matching windows have equal hashes; equal hashes *may* be collisions, so every hit is verified with `memcmp`. The code uses the Mersenne prime `2^61 − 1` (cheap reduction with 128-bit multiplication), making false positives vanishingly rare for non-adversarial data. Its virtue is generality, not speed: it extends to **multiple patterns of the same length** (put hashes in a set), to 2-D matching, to near-duplicate detection and plagiarism checking (hash all k-grams), and to content-defined chunking in deduplicating backups and **rsync** (a weak rolling checksum finds candidate blocks, a strong hash confirms).

### Boyer–Moore–Horspool

Compare the pattern against the text window *right to left*. On a mismatch, look at the text character aligned with the pattern's last position: if it does not occur in the pattern, the whole window can move forward by `m`; otherwise align its last occurrence in the pattern. For large alphabets and long patterns most windows are skipped after a single comparison, so the algorithm reads far fewer than `n` characters — "sublinear" in practice. Full Boyer–Moore adds a *good-suffix* rule for better worst-case behavior; Horspool drops it for simplicity.

### Verification and measurement

The test cross-checks every algorithm against the naive scan on 80,000 random text/pattern pairs over alphabets of 1–4 letters (small alphabets create heavy overlaps and repetition — the cases where fallback logic breaks).

{{include dsa/strings/test_match.c}}

{{run dsa/strings :: gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined test_match.c -o test_match && ./test_match}}

The benchmark scans 32 MB of random DNA (alphabet 4) and of English-like text (27 symbols with realistic letter frequencies), with patterns taken from the text, then the adversarial input for the naive scan.

{{include dsa/strings/bench_match.c}}

{{run dsa/strings :: gcc -std=gnu11 -O2 -Wall -Wextra bench_match.c -o bench_match && ./bench_match}}

What to take from the table (values are nanoseconds per text byte; ratios, not absolutes, are portable):

- **On random text the naive scan is not slow.** Mismatches usually occur on the first or second comparison, so it is nearly linear — which is why it survives in so many libraries for short patterns.
- **Horspool beats KMP when the alphabet is large and the pattern long** (English, `m = 64`: several times faster) because it skips; on a 4-letter alphabet with short patterns there is little to skip, so the advantage shrinks.
- **Rabin–Karp and Z are slower per byte** than the others here: the hash update does a modular multiplication per character and Z builds a large auxiliary array. Their value is generality.
- **`memmem` is the fastest by a wide margin** — typically 2–5× faster than the best hand-written algorithm here and up to ~20× faster than the naive scan. The C library uses vectorised code (SSE/AVX2 scanning for a rare byte or a two-byte anchor) plus the Two-Way algorithm for long needles, so *do not hand-roll single-pattern exact search in production C*; use `memmem`/`strstr`. (Note `memmem` is a GNU/BSD extension, not ISO C, hence `_GNU_SOURCE`.)
- **The adversarial row is the lesson for the naive scan**: on `aaaa…a` against `aaa…ab` it re-examines nearly the whole pattern at every position and its cost per byte grows linearly with `m` (from a few ns at `m = 8` to over a hundred at `m = 512`), while KMP stays flat at roughly 2 ns per byte and Horspool at about 3–4 ns, independent of `m`. Input like this arises in real data (long runs, repetitive logs) and in attacks; any matcher fed untrusted input needs a worst-case bound.

## Many patterns at once: Aho–Corasick

Searching for `K` patterns by running a single-pattern search `K` times costs `K` passes over the text. Aho–Corasick builds a trie of all patterns, adds a **failure link** to each node (the longest proper suffix of the node's string that is also a trie node — KMP's fallback generalised to a tree), and then completes every missing transition by following failure links at build time. The result is a deterministic finite automaton: scanning the text is one table lookup per byte, regardless of `K`, and a node's *output count* includes matches that end at its failure-link ancestors (so `she` also reports `he`).

It is the algorithm behind `fgrep`/`grep -F` with many patterns, antivirus and intrusion-detection signature matching (Snort, Suricata; Hyperscan generalises the idea), profanity and keyword filters, and bioinformatics motif search.

{{include dsa/strings/ac.c}}

{{run dsa/strings :: gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined ac.c -o ac_asan && ./ac_asan | head -3 && gcc -std=gnu11 -O2 -Wall -Wextra ac.c -o ac && ./ac | tail -7}}

The first lines (sanitizer build) check 20,000 random pattern sets — with duplicates and nested patterns — against the sum of naive counts. The table (optimized build) shows the automaton scanning 32 MB in about the same time (within a factor of 1.5) whether it holds 1 pattern or 1000 — its cost is per byte, not per pattern — and then growing only gently at 10,000 patterns because the 58,000-node automaton (6 MB) no longer fits in the CPU's fast caches. Running `memmem` once per pattern wins for a single pattern (about 20× faster here) but loses once there are a few dozen: the crossover is between 10 and 100 patterns, and at 10,000 patterns the repeated scans would take on the order of a minute against a third of a second. The automaton's size is the price: dense tables cost `nodes × alphabet` words, which is why production implementations compress transitions or use sparse representations for large alphabets.

## Suffix structures (overview)

For many queries against one fixed text, build an *index* instead. A **suffix array** lists the starting positions of all suffixes in sorted order; any pattern occurrence is a contiguous range, found with binary search in `O(m log n)`. It can be built in `O(n)` (SA-IS; Nong, Zhang & Chan) and stores `4n` bytes. A **suffix tree** or **suffix automaton** answers more queries (longest repeated substring, longest common substring, counting distinct substrings) in linear space but with large constants. Compressed forms — the **FM-index** built on the Burrows–Wheeler transform, the same transform bzip2 uses — are how DNA aligners (BWA, Bowtie) search a human genome in memory of a few GB.

## Approximate matching: edit distance

Often the strings are merely *close*: spell checking, fuzzy search, DNA alignment, deduplication, diff. The **Levenshtein distance** between `a` and `b` is the minimum number of single-character insertions, deletions and substitutions turning one into the other. The classic dynamic program (Wagner–Fischer) fills a table where `D[i][j]` is the distance between the first `i` characters of `a` and the first `j` of `b`:

`D[i][j] = min( D[i−1][j−1] + [a_i ≠ b_j],  D[i−1][j] + 1,  D[i][j−1] + 1 )`

with `D[i][0] = i` and `D[0][j] = j`. It takes `O(nm)` time; since each row depends only on the previous one, the table need not be stored — two rows give `O(min(n, m))` space (recovering the alignment itself needs Hirschberg's divide-and-conquer to stay linear in space). The **longest common subsequence** (LCS) is the same table with different costs (match = +1, no substitution) and is the basis of `diff`: the characters not in the LCS are the changes. (Production `diff` and `git diff` use Myers' `O(ND)` algorithm, fast when the two files differ little; the dynamic-programming note covers the general technique.)

**Bounded distance.** If you only care whether two strings are within `k` edits — "did you mean…?" — only cells within `k` of the diagonal can matter, because each step off the diagonal costs an edit. The *banded* DP does `O(nk)` work. Bit-parallel versions (Myers 1999) pack a column into machine words for another factor of ~64.

{{include dsa/strings/edit.c}}

{{run dsa/strings :: gcc -std=gnu11 -O2 -g -Wall -Wextra -fsanitize=address,undefined edit.c -o edit_asan && ./edit_asan | head -5 && gcc -std=gnu11 -O2 -Wall -Wextra edit.c -o edit && ./edit | tail -6}}

The sanitizer build runs the correctness checks: 20,000 random pairs where the DP equals an exponential brute-force recursion, is symmetric, and the banded version equals `min(d, k+1)` for every `k`; the classic examples (`kitten`→`sitting` = 3) agree with the textbook. The optimized build shows the banded DP, on two 20,000-character strings that differ at about 1 % of positions: with a band wide enough to contain the true distance it returns the same answer as the full table, roughly 8–60× faster here depending on the band width, and with a band too narrow it correctly reports "more than `k`" even faster.

### Bytes are not characters

The UTF-8 line is the most common real-world bug in this area. `"naïve"` stores `ï` as two bytes (`C3 AF`), so an edit distance computed on bytes says the words differ by **2** edits, while on code points it is **1**. And code points are still not what users see: `é` can be one code point or `e` plus a combining accent, so two strings that *look* identical can have a non-zero distance until both are normalised (Unicode Normalization Forms, UAX #15). Case-insensitive comparison has the same problem (`ß`, Turkish dotless `i`). For anything user-facing, decode UTF-8 to code points (or grapheme clusters) before applying these algorithms, normalise first, and be explicit about locale.

## Pitfalls

- **`strlen` in the loop condition** (`for (i = 0; i < strlen(s); i++)`) is quadratic if the compiler cannot hoist it.
- **Embedded NUL bytes**: `strstr`/`strcmp` stop at them; binary data needs `memmem`/`memcmp` with explicit lengths.
- **Signed `char` as an array index**: bytes ≥ 0x80 become negative indices; cast to `unsigned char` before indexing tables (the code does).
- **Overlapping matches**: decide whether `aa` occurs once or twice in `aaa` — the algorithms here report overlaps; `strstr`-in-a-loop that advances by `m` does not.
- **Rolling-hash weaknesses**: a small modulus or fixed base lets adversaries craft collisions (and anti-hash tests exist for 2^64 overflow hashes); verify hits, use a large prime and a randomised base.
- **Quadratic memory** for full edit-distance tables on large inputs (20,000 × 20,000 × 4 bytes = 1.6 GB): use two rows or a band.
- **Treating distance as similarity**: a distance of 3 means very different things for 4-letter and 400-letter strings; normalise by length when comparing.

## Key takeaways

1. The naive scan is `O(nm)` in the worst case but fast on random text; KMP and the Z-algorithm guarantee `O(n + m)`; Horspool is sublinear on typical text.
2. For a single pattern in production C, call `memmem`/`strstr` — vectorised and Two-Way-based, they beat the textbook algorithms by large factors.
3. Aho–Corasick scans once for any number of patterns at one lookup per byte; it overtakes repeated single-pattern searches at a few dozen patterns.
4. Levenshtein distance is an `O(nm)` DP that needs only two rows of memory; if you only need to know whether the distance is at most `k`, the banded DP is `O(nk)` and orders of magnitude faster.
5. Characters are not bytes: decode and normalise Unicode before measuring distance.
6. Always include an adversarial input in matching benchmarks — repeated characters expose worst cases that random data hides.
