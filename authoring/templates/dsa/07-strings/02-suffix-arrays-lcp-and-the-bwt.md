---
title: "Suffix Arrays, LCP and the Burrows-Wheeler FM-Index"
track: dsa
module: Strings
moduleOrder: 7
order: 2
level: expert
summary: "Index a text once and answer substring questions fast: suffix array construction by prefix doubling with radix sort, Kasai's linear-time LCP array, applications (counting, longest repeated and common substrings, distinct substrings), and the Burrows-Wheeler transform with backward search, all verified against naive code."
prerequisites:
  - String Matching and Edit Distance
  - Binary Search and Its Variants
sources:
  - label: "Manber & Myers — Suffix arrays: a new method for on-line string searches (SIAM J. Comput. 1993)"
    url: https://epubs.siam.org/doi/10.1137/0222058
  - label: "Kasai et al. — Linear-Time Longest-Common-Prefix Computation in Suffix Arrays (CPM 2001)"
    url: https://link.springer.com/chapter/10.1007/3-540-48194-X_17
  - label: "Nong, Zhang & Chan — Two Efficient Algorithms for Linear Time Suffix Array Construction (IEEE Trans. Computers 2011), SA-IS"
    url: https://ieeexplore.ieee.org/document/5582081
  - label: "Burrows & Wheeler — A Block-sorting Lossless Data Compression Algorithm (DEC SRC Research Report 124, 1994)"
  - label: "Ferragina & Manzini — Opportunistic Data Structures with Applications (FOCS 2000), the FM-index"
    url: https://ieeexplore.ieee.org/document/892127
  - label: "Gusfield — Algorithms on Strings, Trees, and Sequences (Cambridge University Press, 1997)"
  - label: "Langmead & Salzberg — Fast gapped-read alignment with Bowtie 2 (Nature Methods 2012)"
    url: https://www.nature.com/articles/nmeth.1923
---

KMP, Z and Boyer–Moore search one pattern in one text in time proportional to the text. When the **text is fixed and the queries are many** — a genome, a log archive, a source tree, a dictionary of names — it pays to build an **index** once and answer each query in time proportional to the *pattern*. The simplest index is the **suffix array**.

## Definition

For a text `s` of length *n*, the suffixes are `s[i..n)` for `i = 0 … n−1`. The **suffix array** `sa` lists the starting positions of the suffixes **in lexicographic order of the suffixes**. For `banana`:

| rank | `sa` | suffix |
|---|---|---|
| 0 | 5 | `a` |
| 1 | 3 | `ana` |
| 2 | 1 | `anana` |
| 3 | 0 | `banana` |
| 4 | 4 | `na` |
| 5 | 2 | `nana` |

Every occurrence of a pattern `p` is the **prefix of some suffix**, and all suffixes that begin with `p` are adjacent in sorted order. So the occurrences of `p` form a contiguous range of `sa`, found with two binary searches in `O(m log n)` (compare `p` with the suffix at the midpoint, at most `m` characters each). The array takes `n` integers: far less than a suffix tree (which needs about 10–20 bytes per character) while answering most of the same questions.

The **LCP array** stores, for each rank *i*, the length of the longest common prefix of the suffixes at ranks *i−1* and *i*. It turns the sorted list into a tree-like structure without building one: the longest common prefix of any two suffixes is the minimum of the LCP values between their ranks (a range-minimum query; see the range-query note).

## Building the suffix array

The naive way sorts the *n* suffixes with a comparison that may read `O(n)` characters, giving `O(n² log n)` in the worst case (for `aaaa…a` every comparison runs to the end). Better:

- **Prefix doubling** (Manber–Myers): sort suffixes by their first character, then by their first 2, 4, 8, … characters. After the round with prefix length *k*, each suffix has a **rank** among all prefixes of length *k*. To sort by the first 2k characters, sort by the pair `(rank[i], rank[i+k])`, which are both known from the previous round. With a **radix sort** (counting sort on the second key, then a stable counting sort on the first key) each round costs `O(n)`, and there are at most `⌈log₂ n⌉` rounds: **O(n log n)** in total. The loop stops early as soon as all ranks are distinct.
- **SA-IS** (Nong–Zhang–Chan), the **DC3/skew** algorithm and others achieve **O(n)** time; SA-IS is what production libraries (libdivsufsort, the `sais` family) use and is several times faster than doubling in practice, at the price of a more intricate implementation.

### The implementation

The code below contains prefix doubling with radix sort, Kasai's LCP computation, the binary-search pattern counter, and an FM-index (explained later).

{{include dsa/suffix/sa.h}}

Two details deserve attention. In `build_sa`, suffixes shorter than `k` have no second half; they are given the smallest possible second key (listed first), which is what makes the "sort by the second key" step free: listing suffixes in the order of the previous round's `sa`, shifted by `k`, already gives them sorted by their second key. In `build_lcp` (Kasai et al.), if suffix *i* shares *h* characters with its predecessor in sorted order, then suffix *i+1* shares at least *h−1* with *its* predecessor, so `h` only ever decreases by one per step and the total work is **O(n)** even though the inner `while` looks quadratic.

### Verification

The test compares each structure with a naive implementation on 4,000 random strings over alphabets of size 1 to 4 (so there are plenty of repeats, and every seventh string is a run of one letter, the worst case for doubling): the suffix array against a plain sort of whole suffixes, the LCP array against direct comparison, and pattern counts from both the suffix array and the FM-index against a scan. It runs under AddressSanitizer and UBSan.

{{include dsa/suffix/test.c}}

{{run dsa/suffix :: gcc -O1 -g -Wall -Wextra -Wno-unused-function -fsanitize=address,undefined test.c -o t && ./t}}

## What the suffix array and LCP array can answer

{{include dsa/suffix/apps.c}}

{{run dsa/suffix :: gcc -O2 -Wall -Wextra -Wno-unused-function apps.c -o apps && ./apps}}

- **Counting and locating a pattern**: occurrences of `ana` occupy a contiguous block of `sa`; their starting positions are read off directly (5 matches, including overlapping ones).
- **Longest repeated substring**: the largest value in the LCP array, with its position in `sa`: here `"ana ban"`, which occurs twice.
- **Number of distinct substrings**: every substring is a prefix of some suffix; suffix *i* contributes `n − sa[i]` prefixes but the first `lcp[i]` of them were already counted by its neighbor, so the total is `n(n+1)/2 − Σ lcp[i]`.
- **Longest common substring of two strings**: concatenate them with a separator that appears in neither, build the arrays, and take the largest LCP between *adjacent* suffixes that come from different strings.
- Other uses: the shortest unique substring, the longest palindromic substring (with the reversed text), the k-th smallest substring, and the building block of compressed indexes.

The timing table at the end shows the cost structure. Random text finishes in about five doubling rounds because suffixes are distinguished after a few characters; **periodic and constant text** needs the full log₂ n rounds (20 for a million characters) and is a few times slower, which is the adversarial case to test. Kasai's LCP pass is linear and takes a fraction of the build time.

## The Burrows–Wheeler transform and the FM-index

The **Burrows–Wheeler transform (BWT)** of a text (with a unique terminator `$` smaller than every other character appended) is obtained by sorting all rotations (equivalently all suffixes) and writing down the **last column**: for each suffix in sorted order, the character that *precedes* it, `bwt[i] = s[sa[i] − 1]`. In the output above the BWT of `banana bandana banana` is the string printed on the last line: equal characters cluster together (long runs of `a` and `n`), which is why bzip2 follows the BWT with move-to-front and Huffman coding.

The remarkable fact is that the BWT can be **inverted and searched without the original text**. Define:

- `C[c]` = the number of characters in the text smaller than `c`;
- `Occ(c, i)` = the number of occurrences of `c` in `bwt[0..i)`.

All suffixes beginning with a character `c` occupy a block of rows starting at `C[c]`. **Backward search** processes the pattern from its last character to its first, maintaining the range `[lo, hi)` of rows (sorted suffixes) that start with the part of the pattern matched so far. Prepending character `c` maps the range to `[C[c] + Occ(c, lo), C[c] + Occ(c, hi))`. After the whole pattern is processed, `hi − lo` is the **number of occurrences**, with `m` steps and no reference to the text. The `fm_count` function above does exactly this, and the test confirms it matches the naive count. To *locate* occurrences, the index keeps a sampled subset of the suffix array and walks backwards until it hits a sample.

Space: the BWT string plus sampled `Occ` checkpoints (every 64 positions here) and a sampled suffix array. Real FM-indexes use wavelet trees or compressed bit vectors so that the whole structure takes less space than the *original text* (for DNA about 2 bits per base plus overhead), which is how **Bowtie** and **BWA** align billions of short reads against a human genome (3 billion bases) on a machine with a few gigabytes.

## Choosing the structure

| Need | Structure |
|---|---|
| One pattern, one text, once | KMP, Z, Boyer–Moore–Horspool |
| Many fixed patterns, scan a text | Aho–Corasick automaton |
| Many queries on a fixed text; count and locate | suffix array + binary search (simple, `4n` bytes) |
| Same, with LCP-based questions | suffix array + LCP + RMQ |
| Very large text, memory-bound (genomics) | FM-index (compressed) |
| Online construction, substring counts, lots of memory | suffix automaton, suffix tree (Ukkonen) |
| Approximate matching (edit distance ≤ k) | FM-index with backtracking, or q-gram filtering plus verification |

## Pitfalls

- **Terminator and separators:** the BWT needs a unique smallest terminator not occurring in the text; strings concatenated for common-substring queries need separators that occur nowhere. Mixing them up produces matches across string boundaries.
- **Binary data and the byte 0:** the code above uses byte 0 as the FM terminator; text with embedded zero bytes needs a different alphabet mapping.
- **Memory:** the doubling implementation uses five integer arrays of size *n*; for a 100 MB text use SA-IS or libdivsufsort and 32-bit indexes (a 4 GB text needs 64-bit indexes).
- **Comparison cost:** binary search compares pattern and suffix with `memcmp` for up to `m` bytes; for repetitive text with long patterns, use the LCP to skip, or an auxiliary index.
- **Case, Unicode:** suffix arrays sort bytes; to search Unicode text case-insensitively, normalize the text first, and note that the byte suffixes of UTF-8 start in the middle of characters unless you restrict to valid boundaries.

## Key takeaways

- A suffix array lists all suffixes in sorted order; the occurrences of any pattern form a contiguous range found by binary search in `O(m log n)`.
- Prefix doubling with radix sort builds it in `O(n log n)`; SA-IS does it in `O(n)`; Kasai's algorithm builds the LCP array in `O(n)`.
- With the LCP array, longest repeated substring, distinct substring counting and longest common substring become one linear scan.
- The Burrows–Wheeler transform plus `C` and `Occ` tables gives an FM-index: pattern counting by backward search in `O(m)` steps without storing the text, and in compressed form it makes genome-scale search practical.
- Test construction against a naive sort on tiny alphabets and constant strings: the worst cases are where the bugs hide.
