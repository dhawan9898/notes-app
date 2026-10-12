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
  - label: "Bentley — Programming Pearls, 2nd ed."
  - label: "Skiena — The Algorithm Design Manual, 3rd ed., Ch. 1-4 exercises"
  - label: "McDowell — Cracking the Coding Interview, 6th ed. (problem categories)"
---

Write each solution in C yourself, test it on small examples (including an empty input), and **write a slow brute-force version to compare against** on random inputs. Only then read the answers. For each, state the time and memory cost in Big-O.

## Exercises

1. **Two-sum (sorted).** Given a sorted array and a target, find two elements that add up to the target, or report none. Try to do better than checking every pair (hint: two indices, one at each end, moving inward).
2. **Palindrome.** Is a sentence a palindrome if you ignore case and non-letters? (`"A man, a plan, a canal: Panama"`)
3. **Binary search.** Return the index of a key in a sorted array, or −1. (Watch the loop condition and the middle calculation.)
4. **Anagrams.** Are two words made of the same letters? (Hint: count the letters.)
5. **Merge.** Merge two sorted arrays into one sorted array in one pass.
6. **Remove duplicates.** Remove duplicates from a sorted array in place and return the new length.
7. **Balanced brackets.** Is `{[()]}` balanced? `([)]`? `((`? (Hint: a stack.)
8. **Fibonacci.** Write the recursive version and an iterative version; verify that they agree for 0 to 25. Which is faster, and why?
9. **Reverse a linked list.** Reverse a singly linked list in place by changing the `next` pointers.
10. **Most frequent value.** Given values from 0 to 99, find the one that occurs most often. (Hint: a counting array.)

## Answers

<details>
<summary>Show verified solutions</summary>

Costs: 1: O(n) time, O(1) memory. 2: O(n), O(1). 3: O(log n), O(1). 4: O(n), O(1) (a 26-entry table). 5: O(n + m), O(n + m) for the output. 6: O(n), O(1). 7: O(n), O(n) for the stack. 8: recursive O(2ⁿ) vs iterative O(n). 9: O(n), O(1). 10: O(n), O(1) (a 100-entry table).

Key ideas: (1) if the current sum is too small, only moving the left index right can increase it, so no pair is skipped wrongly; (3) `mid = lo + (hi - lo) / 2` avoids overflow of `lo + hi`; (7) push every opener and, at each closer, the top of the stack must be its matching opener; (9) keep `prev`, `current` and save `next` *before* overwriting it.

```c
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 1. Two-sum on a sorted array with two indices moving inward: O(n) */
static int two_sum_sorted(const int *a, int n, int target, int *i_out, int *j_out) { int i = 0, j = n - 1; while (i < j) { int s = a[i] + a[j]; if (s == target) { *i_out = i; *j_out = j; return 1; } if (s < target) i++; else j--; } return 0; }
/* 2. Palindrome ignoring case and non-letters */
static int is_palindrome(const char *s) { int i = 0, j = (int)strlen(s) - 1; while (i < j) { while (i < j && !isalpha((unsigned char)s[i])) i++; while (i < j && !isalpha((unsigned char)s[j])) j--; if (tolower((unsigned char)s[i]) != tolower((unsigned char)s[j])) return 0; i++; j--; } return 1; }
/* 3. Binary search: index of key in a sorted array, or -1 */
static int bsearch_idx(const int *a, int n, int key) { int lo = 0, hi = n - 1; while (lo <= hi) { int mid = lo + (hi - lo) / 2; if (a[mid] == key) return mid; if (a[mid] < key) lo = mid + 1; else hi = mid - 1; } return -1; }
/* 4. Are two words anagrams? Count letters. */
static int is_anagram(const char *a, const char *b) { int c[26] = {0}; for (; *a; a++) c[tolower((unsigned char)*a) - 'a']++; for (; *b; b++) c[tolower((unsigned char)*b) - 'a']--; for (int i = 0; i < 26; i++) if (c[i]) return 0; return 1; }
/* 5. Merge two sorted arrays into one sorted array */
static void merge(const int *a, int na, const int *b, int nb, int *out) { int i = 0, j = 0, k = 0; while (i < na && j < nb) out[k++] = a[i] <= b[j] ? a[i++] : b[j++]; while (i < na) out[k++] = a[i++]; while (j < nb) out[k++] = b[j++]; }
/* 6. Remove duplicates from a sorted array in place; returns the new length */
static int dedupe(int *a, int n) { if (n == 0) return 0; int w = 1; for (int r = 1; r < n; r++) if (a[r] != a[w - 1]) a[w++] = a[r]; return w; }
/* 7. Balanced brackets using a stack */
static int balanced(const char *s) { char st[128]; int sp = 0; for (; *s; s++) { if (strchr("([{", *s)) st[sp++] = *s; else if (strchr(")]}", *s)) { if (!sp) return 0; char o = st[--sp]; if ((*s == ')' && o != '(') || (*s == ']' && o != '[') || (*s == '}' && o != '{')) return 0; } } return sp == 0; }
/* 8. Fibonacci, three ways, all agreeing */
static long fib_rec(int n) { return n < 2 ? n : fib_rec(n - 1) + fib_rec(n - 2); }
static long fib_iter(int n) { long a = 0, b = 1; for (int i = 0; i < n; i++) { long t = a + b; a = b; b = t; } return a; }
/* 9. Reverse a singly linked list */
typedef struct Node { int v; struct Node *next; } Node;
static Node *reverse_list(Node *h) { Node *prev = NULL; while (h) { Node *nx = h->next; h->next = prev; prev = h; h = nx; } return prev; }
/* 10. Most frequent element (the "mode") with a counting array for values 0..99 */
static int mode_of(const int *a, int n) { int cnt[100] = {0}, best = 0; for (int i = 0; i < n; i++) cnt[a[i]]++; for (int v = 1; v < 100; v++) if (cnt[v] > cnt[best]) best = v; return best; }

int main(void) {
    int a[] = { 1, 3, 4, 7, 9, 12 }, i, j;
    assert(two_sum_sorted(a, 6, 16, &i, &j)); printf("Ex 1: two numbers summing to 16 are a[%d]=%d and a[%d]=%d; for 100: %s\n", i, a[i], j, a[j], two_sum_sorted(a, 6, 100, &i, &j) ? "found" : "none");
    printf("Ex 2: \"A man, a plan, a canal: Panama\" palindrome? %s;  \"hello\"? %s\n", is_palindrome("A man, a plan, a canal: Panama") ? "yes" : "no", is_palindrome("hello") ? "yes" : "no");
    printf("Ex 3: index of 9 = %d, of 8 = %d\n", bsearch_idx(a, 6, 9), bsearch_idx(a, 6, 8));
    printf("Ex 4: listen/silent anagrams? %s;  apple/pale? %s\n", is_anagram("listen", "silent") ? "yes" : "no", is_anagram("apple", "pale") ? "yes" : "no");
    int b[] = { 2, 3, 10 }, m[9]; merge(a, 6, b, 3, m); printf("Ex 5: merged:"); for (int k = 0; k < 9; k++) printf(" %d", m[k]); printf("\n");
    int d[] = { 1, 1, 2, 2, 2, 3, 5, 5 }; int nd = dedupe(d, 8); printf("Ex 6: after dedupe length %d:", nd); for (int k = 0; k < nd; k++) printf(" %d", d[k]); printf("\n");
    printf("Ex 7: \"{[()]}\" %s;  \"([)]\" %s;  \"((\" %s\n", balanced("{[()]}") ? "balanced" : "NOT", balanced("([)]") ? "balanced" : "NOT", balanced("((") ? "balanced" : "NOT");
    for (int n = 0; n <= 25; n++) assert(fib_rec(n) == fib_iter(n));
    printf("Ex 8: fib(25) = %ld (recursive and iterative agree for n = 0..25)\n", fib_iter(25));
    Node n3 = { 3, NULL }, n2 = { 2, &n3 }, n1 = { 1, &n2 }; Node *r = reverse_list(&n1); printf("Ex 9: reversed list:"); for (Node *p = r; p; p = p->next) printf(" %d", p->v); printf("\n");
    int data[] = { 4, 7, 4, 2, 7, 7, 9 }; printf("Ex 10: mode = %d\n", mode_of(data, 7));
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -g -fsanitize=address,undefined -o answers answers.c && ./answers
Ex 1: two numbers summing to 16 are a[2]=4 and a[5]=12; for 100: none
Ex 2: "A man, a plan, a canal: Panama" palindrome? yes;  "hello"? no
Ex 3: index of 9 = 4, of 8 = -1
Ex 4: listen/silent anagrams? yes;  apple/pale? no
Ex 5: merged: 1 2 3 3 4 7 9 10 12
Ex 6: after dedupe length 4: 1 2 3 5
Ex 7: "{[()]}" balanced;  "([)]" NOT;  "((" NOT
Ex 8: fib(25) = 75025 (recursive and iterative agree for n = 0..25)
Ex 9: reversed list: 3 2 1
Ex 10: mode = 7
```

</details>

## Key takeaways

1. Many "clever" solutions are a counting array, two indices, or a stack.
2. Always write the slow version first and compare.
3. Edge cases (empty, one element, duplicates) are where solutions break.

**Next:** *Complexity Analysis and Amortized Analysis*.
