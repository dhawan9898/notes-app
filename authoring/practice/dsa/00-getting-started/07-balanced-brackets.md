---
kind: code
title: Balanced brackets
module: Getting Started
difficulty: Easy
tags:
  - stack
  - strings
starter: dsa/starters/start-here/ex7-brackets.c
examples:
  - input: '"{[()]}"'
    output: '1'
  - input: '"([)]"'
    output: '0'
    why: 'Counting brackets is not enough: the order matters.'
  - input: '")("'
    output: '0'
constraints:
  - The string may be empty (balanced).
  - Do not read below the bottom of your stack when a closing bracket arrives first.
hints:
  - Push every opening bracket; on a closing bracket pop and compare.
  - At the end the stack must be empty.
---

Return 1 if every `(`, `[` and `{` in the string is closed by the matching bracket in the right order, otherwise 0. All other characters are ignored.

## Explanation

**Cost.** O(n), O(n) for the stack.

**Key idea.** push every opener and, at each closer, the top of the stack must be its matching opener.
