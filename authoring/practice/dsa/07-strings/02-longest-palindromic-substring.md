---
kind: code
title: Longest palindromic substring
module: Strings
difficulty: Medium
tags:
  - strings
  - expand around centre
starter: dsa/starters/strings/ex2-palindrome.c
examples:
  - input: '"babad"'
    output: length 3 ("bab" or "aba")
  - input: '"forgeeksskeegfor"'
    output: length 10 ("geeksskeeg")
  - input: '""'
    output: length 0
constraints:
  - '`s` may be empty; `O(n^2)` time is fine.'
  - Both odd-length (one centre character) and even-length (centre between two) palindromes must be found.
hints:
  - For each centre expand while the two characters are equal.
  - Try centre `(i, i)` for odd and `(i, i + 1)` for even lengths.
---

Find the longest palindromic substring of `s`: store where it starts and its length. If several have the maximum length, any of them is accepted. Expand around each of the `2n - 1` centres.

## Explanation

Every palindrome is symmetric around a **centre**: for odd lengths the centre is a letter (n choices), for even lengths it is the gap between two letters (n − 1 choices), so there are **2n − 1** centres. From each centre, expand outwards while the two ends match and remember the longest. Each expansion costs up to O(n), so the total is O(n²) time and O(1) extra space, which is simple and fast enough for most inputs. (When `babad` has two answers of length 3, `bab` and `aba`, the first found wins; the problem allows either.) Manacher's algorithm reuses the symmetry of palindromes already found to do the same in O(n), and a suffix-array solution needs O(n log n) work; start with the expansion method and only optimise if you must. Always test the even case (`cbbd`), the single character and the no-repeat string, where the answer is length 1.
