---
kind: code
title: Rabin-Karp pattern search
module: Strings
difficulty: Hard
tags:
  - strings
  - rolling hash
starter: dsa/starters/strings/ex3-rabin-karp.c
examples:
  - input: text = "the cat sat on the mat with the other cat", pat = "the"
    output: positions {0, 15, 28, 33}
  - input: '"aaaa", "aa"'
    output: '{0, 1, 2}'
    why: Overlapping matches count.
  - input: '"abc", "abcd"'
    output: '0'
    why: The pattern is longer than the text.
constraints:
  - Pattern longer than text returns 0.
  - The last window of the text must be checked too.
hints:
  - hash = (hash - first * base^(m-1)) * base + next, all modulo a prime.
  - 'Keep intermediate values non-negative: add the modulus before subtracting.'
  - Compare with `memcmp` when the hashes are equal.
---

Find **every** occurrence of `pat` in `text` (overlaps included) with a rolling hash. Store the start index of each in `pos[]` (at most `max`) and return how many there are. Update the window hash in O(1) per step and confirm each candidate character by character, because different strings can share a hash.

## Explanation

Treat each window as a number written in base 256 (modulo a prime). When the window slides, *subtract* the leading character's contribution (`t[i] × BASE^(m−1)`), *multiply* by the base to shift everything left, and *add* the new trailing character: O(1) per step, so the whole scan is O(n + m) on average instead of recomputing an m-character hash for every window. The hash comparison is only a **filter**: two different strings can share a hash (a collision), so every candidate window is confirmed with `strncmp`. Here every equal-hash window was a real match (4 of 4), but with this small modulus a collision is possible, and without verification the program would report false matches. A larger modulus (or two moduli, or a 64-bit modulus) makes collisions rare, which is why verification is cheap on average, but the worst case is still O(n·m). Rolling hashes are also the tool for finding duplicate substrings, plagiarism detection and content-defined chunking in backup tools.
