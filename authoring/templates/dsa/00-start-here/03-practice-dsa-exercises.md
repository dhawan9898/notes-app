---
title: "Practice: DSA Exercises"
track: dsa
module: Start Here
moduleOrder: 0
order: 3
level: beginner
summary: "Ten graded warm-up problems (two-sum, palindrome, binary search, anagrams, merging, removing duplicates, balanced brackets, Fibonacci, reversing a list, counting) with verified C solutions and a brute-force testing habit."
prerequisites:
  - Thinking in Data Structures
  - Pointers for Beginners
sources:
  - label: Bentley — Programming Pearls, 2nd ed.
  - label: Skiena — The Algorithm Design Manual, 3rd ed., Ch. 1-4 exercises
  - label: McDowell — Cracking the Coding Interview, 6th ed. (problem categories)
---

Write each solution in C yourself, test it on small examples (including an empty input), and **write a slow brute-force version to compare against** on random inputs. Only then read the answers. For each, state the time and memory cost in Big-O.

## Exercises

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

1. **Two-sum (sorted).** Given a sorted array and a target, find two elements that add up to the target, or report none. Try to do better than checking every pair (hint: two indices, one at each end, moving inward).

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/start-here/ex1-two-sum.c}}

</details>

2. **Palindrome.** Is a sentence a palindrome if you ignore case and non-letters? (`"A man, a plan, a canal: Panama"`)

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/start-here/ex2-palindrome.c}}

</details>

3. **Binary search.** Return the index of a key in a sorted array, or −1. (Watch the loop condition and the middle calculation.)

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/start-here/ex3-binary-search.c}}

</details>

4. **Anagrams.** Are two words made of the same letters? (Hint: count the letters.)

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/start-here/ex4-anagram.c}}

</details>

5. **Merge.** Merge two sorted arrays into one sorted array in one pass.

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/start-here/ex5-merge.c}}

</details>

6. **Remove duplicates.** Remove duplicates from a sorted array in place and return the new length.

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/start-here/ex6-dedupe.c}}

</details>

7. **Balanced brackets.** Is `{[()]}` balanced? `([)]`? `((`? (Hint: a stack.)

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/start-here/ex7-brackets.c}}

</details>

8. **Fibonacci.** Write the recursive version and an iterative version; verify that they agree for 0 to 25. Which is faster, and why?

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/start-here/ex8-fibonacci.c}}

</details>

9. **Reverse a linked list.** Reverse a singly linked list in place by changing the `next` pointers.

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/start-here/ex9-reverse-list.c}}

</details>

10. **Most frequent value.** Given values from 0 to 99, find the one that occurs most often. (Hint: a counting array.)

<details class="starter">
<summary>Starter program with tests</summary>

{{include dsa/starters/start-here/ex10-mode.c}}

</details>

## Answers

<details>
<summary>Show verified solutions</summary>

Costs: 1: O(n) time, O(1) memory. 2: O(n), O(1). 3: O(log n), O(1). 4: O(n), O(1) (a 26-entry table). 5: O(n + m), O(n + m) for the output. 6: O(n), O(1). 7: O(n), O(n) for the stack. 8: recursive O(2ⁿ) vs iterative O(n). 9: O(n), O(1). 10: O(n), O(1) (a 100-entry table).

Key ideas: (1) if the current sum is too small, only moving the left index right can increase it, so no pair is skipped wrongly; (3) `mid = lo + (hi - lo) / 2` avoids overflow of `lo + hi`; (7) push every opener and, at each closer, the top of the stack must be its matching opener; (9) keep `prev`, `current` and save `next` *before* overwriting it.

{{include dsa/beginner/answers.c}}

{{run dsa/beginner :: gcc -std=c17 -Wall -Wextra -g -fsanitize=address,undefined -o answers answers.c && ./answers}}

</details>

## Key takeaways

1. Many "clever" solutions are a counting array, two indices, or a stack.
2. Always write the slow version first and compare.
3. Edge cases (empty, one element, duplicates) are where solutions break.

**Next:** *Complexity Analysis and Amortized Analysis*.
