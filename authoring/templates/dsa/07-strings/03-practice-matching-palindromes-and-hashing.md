---
title: "Practice: Pattern Matching, Palindromes and Rolling Hashes"
track: dsa
module: Strings
moduleOrder: 7
order: 3
level: advanced
summary: "Three exercises on the strings module: comparing naive and Knuth-Morris-Pratt search by counting character comparisons on an adversarial input, finding the longest palindromic substring by expanding around centres, and implementing a Rabin-Karp rolling hash, each with a verified C answer."
prerequisites:
  - "String Matching and Edit Distance"
  - "Suffix Arrays, LCP and the Burrows-Wheeler FM-Index"
sources:
  - label: Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 32 (String Matching)
  - label: Knuth, Morris, Pratt — Fast Pattern Matching in Strings (SIAM J. Computing, 1977)
  - label: Karp, Rabin — Efficient Randomized Pattern-Matching Algorithms (IBM J. Res. Dev., 1987)
  - label: Gusfield — Algorithms on Strings, Trees, and Sequences, Ch. 1-3
---

String algorithms are mostly about *not repeating work*. These exercises make the saved work visible by counting character comparisons.

## Exercises

**1. Naive versus KMP.** Search for the pattern `aaaaaaaaab` in a text of 2,000 letters `a`. How many character comparisons does the naive algorithm make, and why is this input a bad case? Implement Knuth-Morris-Pratt (KMP) and compare. What does the "failure function" store?

**2. Longest palindromic substring.** Find the longest palindrome inside `forgeeksskeegfor`, `babad`, `cbbd`, `a` and `abc`. Explain why there are `2n − 1` centres to try, and the complexity.

**3. Rolling hash.** Implement Rabin-Karp to find every occurrence of `the` in a sentence. How does the hash of the window update in O(1) when the window slides one character? Why must a hash match be verified?

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

{{include dsa/practice/p7_kmp.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p7_kmp p7_kmp.c && ./p7_kmp; rm -f p7_kmp}}

The naive search tries every start position and re-compares from the beginning of the pattern each time. Against a text of all `a`s and a pattern that ends in `b`, nearly every position matches nine characters before failing on the last: about 9 × 2,000 = **19,910 comparisons** (O(n·m) in general). KMP never moves backwards in the text: when a mismatch happens after matching `k` characters, it uses the **failure function** to decide how much of the matched part can be reused. `fail[i]` is the length of the longest *proper prefix* of `p[0..i]` that is also a *suffix* of it, so on a mismatch the matcher falls back to `k = fail[k-1]` instead of restarting. Each character of the text is examined a small constant number of times: **3,991 comparisons** (about 2n), O(n + m) in total including building the table. KMP matters when worst cases or streaming input matter; for ordinary searches the library `strstr` or Boyer-Moore-style skipping is often faster in practice. The last line confirms both algorithms give the same answer on an ordinary text.

**2.**

{{include dsa/practice/p7_palin.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p7_palin p7_palin.c && ./p7_palin; rm -f p7_palin}}

Every palindrome is symmetric around a **centre**: for odd lengths the centre is a letter (n choices), for even lengths it is the gap between two letters (n − 1 choices), so there are **2n − 1** centres. From each centre, expand outwards while the two ends match and remember the longest. Each expansion costs up to O(n), so the total is O(n²) time and O(1) extra space, which is simple and fast enough for most inputs. (When `babad` has two answers of length 3, `bab` and `aba`, the first found wins; the problem allows either.) Manacher's algorithm reuses the symmetry of palindromes already found to do the same in O(n), and a suffix-array solution needs O(n log n) work; start with the expansion method and only optimise if you must. Always test the even case (`cbbd`), the single character and the no-repeat string, where the answer is length 1.

**3.**

{{include dsa/practice/p7_rolling.c}}

{{run dsa/practice :: gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p7_rolling p7_rolling.c && ./p7_rolling; rm -f p7_rolling}}

Treat each window as a number written in base 256 (modulo a prime). When the window slides, *subtract* the leading character's contribution (`t[i] × BASE^(m−1)`), *multiply* by the base to shift everything left, and *add* the new trailing character: O(1) per step, so the whole scan is O(n + m) on average instead of recomputing an m-character hash for every window. The hash comparison is only a **filter**: two different strings can share a hash (a collision), so every candidate window is confirmed with `strncmp`. Here every equal-hash window was a real match (4 of 4), but with this small modulus a collision is possible, and without verification the program would report false matches. A larger modulus (or two moduli, or a 64-bit modulus) makes collisions rare, which is why verification is cheap on average, but the worst case is still O(n·m). Rolling hashes are also the tool for finding duplicate substrings, plagiarism detection and content-defined chunking in backup tools.

</details>

## Key takeaways

1. The naive search is O(n·m) on adversarial input; KMP uses the failure function (longest proper prefix that is also a suffix) to avoid ever moving backwards, giving O(n + m).
2. A string has 2n − 1 palindrome centres; expanding from each costs O(n²) overall, and Manacher's algorithm gets O(n).
3. A rolling hash updates in O(1) per slide; a matching hash only nominates a candidate, so always verify it.
