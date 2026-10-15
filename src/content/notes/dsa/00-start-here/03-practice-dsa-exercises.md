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

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

1. **Two-sum (sorted).** Given a sorted array and a target, find two elements that add up to the target, or report none. Try to do better than checking every pair (hint: two indices, one at each end, moving inward).

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 1: two-sum on a sorted array, faster than checking every pair.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* If two elements at different indexes add up to target, store the indexes (i < j) and return 1; otherwise return 0. */
int two_sum_sorted(const int *a, int n, int target, int *i_out, int *j_out) {
    (void)a; (void)n; (void)target; (void)i_out; (void)j_out;
    return 0;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    int a[] = {1, 3, 4, 7, 9, 12};
    int i = -1, j = -1;
    check_int("two_sum_sorted(a, 16) found", two_sum_sorted(a, 6, 16, &i, &j), 1);
    check_int("  a[i] + a[j]", (i >= 0 && j >= 0) ? a[i] + a[j] : -1, 16);
    check_int("  i < j", i < j, 1);
    check_int("two_sum_sorted(a, 13) (first and last)", two_sum_sorted(a, 6, 13, &i, &j), 1);
    check_int("two_sum_sorted(a, 100)", two_sum_sorted(a, 6, 100, &i, &j), 0);
    check_int("two_sum_sorted(a, 2) (an element cannot pair with itself)", two_sum_sorted(a, 6, 2, &i, &j), 0);
    check_int("two_sum_sorted(empty array, 0)", two_sum_sorted(a, 0, 0, &i, &j), 0);
    return summary();
}
```

</details>

2. **Palindrome.** Is a sentence a palindrome if you ignore case and non-letters? (`"A man, a plan, a canal: Panama"`)

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 2: is a sentence a palindrome if you ignore case and non-letters?
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
int is_palindrome(const char *s) {
    (void)s;
    return 0;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    check_int("is_palindrome(\"A man, a plan, a canal: Panama\")", is_palindrome("A man, a plan, a canal: Panama"), 1);
    check_int("is_palindrome(\"hello\")", is_palindrome("hello"), 0);
    check_int("is_palindrome(\"Racecar\") (case)", is_palindrome("Racecar"), 1);
    check_int("is_palindrome(\"\") (empty)", is_palindrome(""), 1);
    check_int("is_palindrome(\"!!\") (no letters)", is_palindrome("!!"), 1);
    check_int("is_palindrome(\"ab, cb\")", is_palindrome("ab, cb"), 0);
    return summary();
}
```

</details>

3. **Binary search.** Return the index of a key in a sorted array, or −1. (Watch the loop condition and the middle calculation.)

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 3: binary search. Return the index of key in a sorted array, or -1.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
int bsearch_idx(const int *a, int n, int key) {
    (void)a; (void)n; (void)key;
    return -1;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    int a[] = {1, 3, 4, 7, 9, 12};
    check_int("bsearch_idx(a, 9)", bsearch_idx(a, 6, 9), 4);
    check_int("bsearch_idx(a, 1) (first)", bsearch_idx(a, 6, 1), 0);
    check_int("bsearch_idx(a, 12) (last)", bsearch_idx(a, 6, 12), 5);
    check_int("bsearch_idx(a, 8) (missing)", bsearch_idx(a, 6, 8), -1);
    check_int("bsearch_idx(a, 0) (below all)", bsearch_idx(a, 6, 0), -1);
    check_int("bsearch_idx(a, 99) (above all)", bsearch_idx(a, 6, 99), -1);
    int one[] = {5};
    check_int("bsearch_idx({5}, 5)", bsearch_idx(one, 1, 5), 0);
    check_int("bsearch_idx(empty, 5)", bsearch_idx(one, 0, 5), -1);
    return summary();
}
```

</details>

4. **Anagrams.** Are two words made of the same letters? (Hint: count the letters.)

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 4: are two words made of the same letters (same counts)?
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Words contain only the letters a-z, in either case. */
int is_anagram(const char *a, const char *b) {
    (void)a; (void)b;
    return 0;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    check_int("is_anagram(\"listen\", \"silent\")", is_anagram("listen", "silent"), 1);
    check_int("is_anagram(\"apple\", \"pale\")", is_anagram("apple", "pale"), 0);
    check_int("is_anagram(\"aab\", \"abb\") (same letters, different counts)", is_anagram("aab", "abb"), 0);
    check_int("is_anagram(\"Dusty\", \"study\") (case)", is_anagram("Dusty", "study"), 1);
    check_int("is_anagram(\"\", \"\")", is_anagram("", ""), 1);
    return summary();
}
```

</details>

5. **Merge.** Merge two sorted arrays into one sorted array in one pass.

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 5: merge two sorted arrays into one sorted array in one pass.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* out has room for na + nb elements. */
void merge(const int *a, int na, const int *b, int nb, int *out) {
    (void)a; (void)na; (void)b; (void)nb; (void)out;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void print_arr(const int *v, int n) {
    printf("{");
    for (int i = 0; i < n; i++) printf(i ? ", %d" : "%d", v[i]);
    printf("}");
}

static void check_arr(const char *what, const int *got, int got_n, const int *want, int want_n) {
    int same = got_n == want_n;
    for (int i = 0; same && i < want_n; i++) same = got[i] == want[i];
    checks++;
    if (!same) failed++;
    printf("%s %s", same ? "PASS" : "FAIL", what);
    if (same) { printf(" = "); print_arr(got, got_n); }
    else { printf(": expected "); print_arr(want, want_n); printf(", got "); print_arr(got, got_n); }
    printf("\n");
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    int a[] = {1, 3, 4, 7, 9, 12}, b[] = {2, 3, 10}, out[16] = {0};
    merge(a, 6, b, 3, out);
    const int want1[] = {1, 2, 3, 3, 4, 7, 9, 10, 12};
    check_arr("merge({1,3,4,7,9,12}, {2,3,10})", out, 9, want1, 9);
    int c[] = {20, 30};
    merge(a, 6, c, 2, out);
    const int want2[] = {1, 3, 4, 7, 9, 12, 20, 30};
    check_arr("merge(a, {20, 30}) (a runs out first)", out, 8, want2, 8);
    merge(c, 2, a, 0, out);
    const int want3[] = {20, 30};
    check_arr("merge({20, 30}, empty)", out, 2, want3, 2);
    return summary();
}
```

</details>

6. **Remove duplicates.** Remove duplicates from a sorted array in place and return the new length.

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 6: remove duplicates from a sorted array in place; return the new length.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
int dedupe(int *a, int n) {
    (void)a;
    return n;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static void print_arr(const int *v, int n) {
    printf("{");
    for (int i = 0; i < n; i++) printf(i ? ", %d" : "%d", v[i]);
    printf("}");
}

static void check_arr(const char *what, const int *got, int got_n, const int *want, int want_n) {
    int same = got_n == want_n;
    for (int i = 0; same && i < want_n; i++) same = got[i] == want[i];
    checks++;
    if (!same) failed++;
    printf("%s %s", same ? "PASS" : "FAIL", what);
    if (same) { printf(" = "); print_arr(got, got_n); }
    else { printf(": expected "); print_arr(want, want_n); printf(", got "); print_arr(got, got_n); }
    printf("\n");
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    int d[] = {1, 1, 2, 2, 2, 3, 5, 5};
    int n = dedupe(d, 8);
    const int want[] = {1, 2, 3, 5};
    check_arr("dedupe({1,1,2,2,2,3,5,5})", d, n, want, 4);
    int same[] = {7, 7, 7};
    n = dedupe(same, 3);
    const int want2[] = {7};
    check_arr("dedupe({7,7,7})", same, n, want2, 1);
    int distinct[] = {1, 2, 3};
    n = dedupe(distinct, 3);
    check_arr("dedupe({1,2,3}) (nothing to remove)", distinct, n, distinct, 3);
    check_int("dedupe(empty)", dedupe(d, 0), 0);
    return summary();
}
```

</details>

7. **Balanced brackets.** Is `{[()]}` balanced? `([)]`? `((`? (Hint: a stack.)

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 7: balanced brackets with a stack.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Return 1 if every (, [ and { is closed by the matching bracket in the right order. Other characters are ignored. */
int balanced(const char *s) {
    (void)s;
    return 0;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    check_int("balanced(\"{[()]}\")", balanced("{[()]}"), 1);
    check_int("balanced(\"([)]\") (counting brackets is not enough)", balanced("([)]"), 0);
    check_int("balanced(\"((\")", balanced("(("), 0);
    check_int("balanced(\")(\")", balanced(")("), 0);
    check_int("balanced(\"\")", balanced(""), 1);
    check_int("balanced(\"f(a[i], {x})\")", balanced("f(a[i], {x})"), 1);
    return summary();
}
```

</details>

8. **Fibonacci.** Write the recursive version and an iterative version; verify that they agree for 0 to 25. Which is faster, and why?

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 8: Fibonacci, recursive and iterative; they must agree.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
long long fib_rec(int n) {
    (void)n;
    return 0;
}

long long fib_iter(int n) {
    (void)n;
    return 0;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    check_int("fib_iter(0)", fib_iter(0), 0);
    check_int("fib_iter(1)", fib_iter(1), 1);
    check_int("fib_iter(10)", fib_iter(10), 55);
    check_int("fib_iter(50)", fib_iter(50), 12586269025LL);
    int agree = 1;
    for (int n = 0; n <= 25; n++) agree &= fib_rec(n) == fib_iter(n);
    check_int("fib_rec and fib_iter agree for 0..25", agree, 1);
    check_int("fib_rec(25)", fib_rec(25), 75025);
    return summary();
}
```

</details>

9. **Reverse a linked list.** Reverse a singly linked list in place by changing the `next` pointers.

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 9: reverse a singly linked list in place by changing the next pointers.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node { int v; struct Node *next; } Node;

/* ===== Your code: edit below this line ===== */
/* Return the new head. Do not allocate nodes. */
Node *reverse_list(Node *head) {
    return head;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static void print_arr(const int *v, int n) {
    printf("{");
    for (int i = 0; i < n; i++) printf(i ? ", %d" : "%d", v[i]);
    printf("}");
}

static void check_arr(const char *what, const int *got, int got_n, const int *want, int want_n) {
    int same = got_n == want_n;
    for (int i = 0; same && i < want_n; i++) same = got[i] == want[i];
    checks++;
    if (!same) failed++;
    printf("%s %s", same ? "PASS" : "FAIL", what);
    if (same) { printf(" = "); print_arr(got, got_n); }
    else { printf(": expected "); print_arr(want, want_n); printf(", got "); print_arr(got, got_n); }
    printf("\n");
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    Node n4 = {4, NULL}, n3 = {3, &n4}, n2 = {2, &n3}, n1 = {1, &n2};
    int got[8], k = 0;
    for (Node *p = reverse_list(&n1); p && k < 8; p = p->next) got[k++] = p->v;
    const int want[] = {4, 3, 2, 1};
    check_arr("reverse_list(1 -> 2 -> 3 -> 4)", got, k, want, 4);
    Node single = {9, NULL};
    Node *r = reverse_list(&single);
    check_int("reverse_list(single node) head value", r ? r->v : -1, 9);
    check_int("  and its next is NULL", r && r->next == NULL, 1);
    check_int("reverse_list(NULL) is NULL", reverse_list(NULL) == NULL, 1);
    return summary();
}
```

</details>

10. **Most frequent value.** Given values from 0 to 99, find the one that occurs most often. (Hint: a counting array.)

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 10: the most frequent value (values 0 to 99), with a counting array.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* On a tie, return the smallest of the most frequent values. n > 0. */
int mode_of(const int *a, int n) {
    (void)a; (void)n;
    return -1;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    int data[] = {4, 7, 4, 2, 7, 7, 9};
    check_int("mode_of({4,7,4,2,7,7,9})", mode_of(data, 7), 7);
    int tie[] = {3, 1, 3, 1};
    check_int("mode_of({3,1,3,1}) (tie: smallest)", mode_of(tie, 4), 1);
    int one[] = {99};
    check_int("mode_of({99})", mode_of(one, 1), 99);
    int zeros[] = {0, 0, 5};
    check_int("mode_of({0,0,5})", mode_of(zeros, 3), 0);
    return summary();
}
```

</details>

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
