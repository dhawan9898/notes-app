---
kind: code
title: Reverse a string in place
module: Getting Started
difficulty: Easy
tags:
  - strings
  - two pointers
starter: c/starters/start-here/ex5-reverse.c
examples:
  - input: '"stressed"'
    output: '"desserts"'
  - input: '"hello world"'
    output: '"dlrow olleh"'
  - input: '""'
    output: '""'
    why: The empty string has nothing to swap.
constraints:
  - '`s` points to a writable, NUL-terminated array (not a string literal).'
  - Odd and even lengths, a single character and the empty string must all work.
hints:
  - Find the length with `strlen`, then swap `s[i]` with `s[len - 1 - i]` for the first half only.
  - 'Stop when the two indices meet: swapping the whole string reverses it twice.'
---

Write `void reverse(char *s)` that reverses the NUL-terminated string `s` **in place**: no second buffer.

## Explanation

Swap `s[i]` with `s[n-1-i]` for `i` from 0 up to (not including) `n/2`. Stopping at the middle matters: going to `n` would swap everything back.
