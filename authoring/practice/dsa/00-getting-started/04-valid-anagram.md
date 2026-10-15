---
kind: code
title: Anagrams
module: Getting Started
difficulty: Easy
tags:
  - counting
  - strings
starter: dsa/starters/start-here/ex4-anagram.c
examples:
  - input: '"listen", "silent"'
    output: '1'
  - input: '"aab", "abb"'
    output: '0'
    why: Same letters, different counts.
  - input: '"Dusty", "study"'
    output: '1'
constraints:
  - Only letters `a`–`z` or `A`–`Z`; both strings may be empty.
  - O(n) time, O(1) extra memory (26 counters).
hints:
  - Count letters of the first word up, letters of the second word down.
  - They are anagrams if every counter ends at zero.
---

Are two words made of exactly the same letters, with the same counts? Case does not matter. The words contain only the letters a to z.

## Explanation

**Cost.** O(n), O(1) (a 26-entry table).
