---
kind: code
title: Valid palindrome
module: Getting Started
difficulty: Easy
tags:
  - strings
  - two pointers
starter: dsa/starters/start-here/ex2-palindrome.c
examples:
  - input: '"A man, a plan, a canal: Panama"'
    output: '1'
  - input: '"hello"'
    output: '0'
  - input: '"!!"'
    output: '1'
    why: 'No letters at all: an empty comparison is trivially a palindrome.'
constraints:
  - '`s` is NUL-terminated and may be empty.'
  - Pass `unsigned char` values to `<ctype.h>` functions.
hints:
  - Move two indices inward, skipping characters that are not alphanumeric (`isalnum`).
  - Compare with `tolower` on both sides.
---

Is a sentence a palindrome if you ignore case and everything that is not a letter or a digit? Write `int is_palindrome(const char *s)` returning 1 or 0.

## Explanation

**Cost.** O(n), O(1).
