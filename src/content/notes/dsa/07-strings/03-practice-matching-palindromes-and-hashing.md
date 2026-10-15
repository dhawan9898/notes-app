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
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 32 (String Matching)"
  - label: "Knuth, Morris, Pratt — Fast Pattern Matching in Strings (SIAM J. Computing, 1977)"
  - label: "Karp, Rabin — Efficient Randomized Pattern-Matching Algorithms (IBM J. Res. Dev., 1987)"
  - label: "Gusfield — Algorithms on Strings, Trees, and Sequences, Ch. 1-3"
---

String algorithms are mostly about *not repeating work*. These exercises make the saved work visible by counting character comparisons.

## Exercises

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Naive versus KMP.** Search for the pattern `aaaaaaaaab` in a text of 2,000 letters `a`. How many character comparisons does the naive algorithm make, and why is this input a bad case? Implement Knuth-Morris-Pratt (KMP) and compare. What does the "failure function" store?

**2. Longest palindromic substring.** Find the longest palindrome inside `forgeeksskeegfor`, `babad`, `cbbd`, `a` and `abc`. Explain why there are `2n − 1` centres to try, and the complexity.

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 2: the longest palindromic substring, by expanding around each of the 2n - 1 centres.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Store where the longest palindrome starts and its length. If several have the maximum length, any of them is fine. */
void longest_palindrome(const char *s, int *start, int *len) {
    (void)s;
    *start = 0;
    *len = 0;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

static void check_palindrome(const char *s, int want_len) {
    int start = -1, len = -1;
    longest_palindrome(s, &start, &len);
    int ok = len == want_len && start >= 0 && start + len <= (int)strlen(s);
    for (int i = 0; ok && i < len / 2; i++) ok = s[start + i] == s[start + len - 1 - i];
    checks++;
    if (!ok) failed++;
    printf("%s longest palindrome in \"%s\": expected length %d, got \"%.*s\" (length %d)\n",
           ok ? "PASS" : "FAIL", s, want_len, len > 0 && start >= 0 ? len : 0, s + (start > 0 ? start : 0), len);
}

int main(void) {
    check_palindrome("babad", 3);
    check_palindrome("cbbd", 2);
    check_palindrome("forgeeksskeegfor", 10);
    check_palindrome("a", 1);
    check_palindrome("abc", 1);
    check_palindrome("", 0);
    return summary();
}
```

</details>

**3. Rolling hash.** Implement Rabin-Karp to find every occurrence of `the` in a sentence. How does the hash of the window update in O(1) when the window slides one character? Why must a hash match be verified?

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 3: find every occurrence of a pattern with a rolling hash (Rabin-Karp).
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Store the starting index of each occurrence of pat in text (overlaps included) in pos[], at most max of them,
   and return how many there are. Update the window hash in O(1) per step; confirm candidates character by character. */
int find_all(const char *text, const char *pat, int pos[], int max) {
    (void)text; (void)pat; (void)pos; (void)max;
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
    int pos[16] = {0};
    int n = find_all("the cat sat on the mat with the other cat", "the", pos, 16);
    const int want[] = {0, 15, 28, 33};
    check_arr("positions of \"the\"", pos, n, want, 4);
    n = find_all("aaaa", "aa", pos, 16);
    const int overlap[] = {0, 1, 2};
    check_arr("positions of \"aa\" in \"aaaa\" (overlapping)", pos, n, overlap, 3);
    check_int("find_all(\"abc\", \"abcd\") (pattern longer than text)", find_all("abc", "abcd", pos, 16), 0);
    check_int("find_all(\"abc\", \"x\")", find_all("abc", "x", pos, 16), 0);
    n = find_all("abc", "c", pos, 16);
    const int last[] = {2};
    check_arr("positions of \"c\" in \"abc\" (last window)", pos, n, last, 1);
    return summary();
}
```

</details>

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1.**

```c
#include <stdio.h>
#include <string.h>

static long naive_cmp;
static int naive_find(const char *t, const char *p) {
    size_t n = strlen(t), m = strlen(p);
    for (size_t i = 0; i + m <= n; i++) {
        size_t j = 0;
        while (j < m) { naive_cmp++; if (t[i + j] != p[j]) break; j++; }
        if (j == m) return (int)i;
    }
    return -1;
}

/* failure[i] = length of the longest proper prefix of p[0..i] that is also a suffix of it */
static long kmp_cmp;
static int kmp_find(const char *t, const char *p) {
    size_t n = strlen(t), m = strlen(p);
    int fail[64] = {0};
    for (size_t i = 1, k = 0; i < m; i++) {
        while (k > 0 && p[i] != p[k]) k = (size_t)fail[k - 1];
        if (p[i] == p[k]) k++;
        fail[i] = (int)k;
    }
    for (size_t i = 0, k = 0; i < n; i++) {
        while (k > 0) { kmp_cmp++; if (t[i] == p[k]) break; k = (size_t)fail[k - 1]; }
        if (k == 0) { kmp_cmp++; if (t[i] != p[0]) continue; }
        k++;
        if (k == m) return (int)(i - m + 1);
    }
    return -1;
}

int main(void) {
    char text[2001];
    memset(text, 'a', 2000); text[2000] = '\0';                    /* a worst case for the naive search: lots of near matches */
    const char *pattern = "aaaaaaaaab";
    int r1 = naive_find(text, pattern), r2 = kmp_find(text, pattern);
    printf("text: 2000 x 'a', pattern: \"%s\" (absent)\n", pattern);
    printf("naive: result %d after %ld character comparisons\n", r1, naive_cmp);
    printf("KMP:   result %d after %ld character comparisons\n", r2, kmp_cmp);
    const char *t2 = "abracadabra abracadabra", *p2 = "cadab";
    printf("find \"%s\" in \"%s\": naive %d, KMP %d\n", p2, t2, naive_find(t2, p2), kmp_find(t2, p2));
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p7_kmp p7_kmp.c && ./p7_kmp; rm -f p7_kmp
text: 2000 x 'a', pattern: "aaaaaaaaab" (absent)
naive: result -1 after 19910 character comparisons
KMP:   result -1 after 3991 character comparisons
find "cadab" in "abracadabra abracadabra": naive 4, KMP 4
```

The naive search tries every start position and re-compares from the beginning of the pattern each time. Against a text of all `a`s and a pattern that ends in `b`, nearly every position matches nine characters before failing on the last: about 9 × 2,000 = **19,910 comparisons** (O(n·m) in general). KMP never moves backwards in the text: when a mismatch happens after matching `k` characters, it uses the **failure function** to decide how much of the matched part can be reused. `fail[i]` is the length of the longest *proper prefix* of `p[0..i]` that is also a *suffix* of it, so on a mismatch the matcher falls back to `k = fail[k-1]` instead of restarting. Each character of the text is examined a small constant number of times: **3,991 comparisons** (about 2n), O(n + m) in total including building the table. KMP matters when worst cases or streaming input matter; for ordinary searches the library `strstr` or Boyer-Moore-style skipping is often faster in practice. The last line confirms both algorithms give the same answer on an ordinary text.

**2.**

```c
#include <stdio.h>
#include <string.h>

/* Longest palindromic substring: try every centre (a letter, or the gap between two letters) and expand outwards. */
static void longest_palindrome(const char *s, int *best_start, int *best_len) {
    int n = (int)strlen(s);
    *best_start = 0; *best_len = n ? 1 : 0;
    for (int centre = 0; centre < 2 * n - 1; centre++) {
        int l = centre / 2, r = l + centre % 2;                  /* odd length: l == r, even length: r = l + 1 */
        while (l >= 0 && r < n && s[l] == s[r]) { l--; r++; }
        int len = r - l - 1;
        if (len > *best_len) { *best_len = len; *best_start = l + 1; }
    }
}

int main(void) {
    const char *tests[] = {"babad", "cbbd", "forgeeksskeegfor", "a", "abc"};
    for (int i = 0; i < 5; i++) {
        int start, len;
        longest_palindrome(tests[i], &start, &len);
        printf("%-18s -> \"%.*s\" (length %d)\n", tests[i], len, tests[i] + start, len);
    }
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p7_palin p7_palin.c && ./p7_palin; rm -f p7_palin
babad              -> "bab" (length 3)
cbbd               -> "bb" (length 2)
forgeeksskeegfor   -> "geeksskeeg" (length 10)
a                  -> "a" (length 1)
abc                -> "a" (length 1)
```

Every palindrome is symmetric around a **centre**: for odd lengths the centre is a letter (n choices), for even lengths it is the gap between two letters (n − 1 choices), so there are **2n − 1** centres. From each centre, expand outwards while the two ends match and remember the longest. Each expansion costs up to O(n), so the total is O(n²) time and O(1) extra space, which is simple and fast enough for most inputs. (When `babad` has two answers of length 3, `bab` and `aba`, the first found wins; the problem allows either.) Manacher's algorithm reuses the symmetry of palindromes already found to do the same in O(n), and a suffix-array solution needs O(n log n) work; start with the expansion method and only optimise if you must. Always test the even case (`cbbd`), the single character and the no-repeat string, where the answer is length 1.

**3.**

```c
#include <stdio.h>
#include <string.h>

#define BASE 256
#define MOD 1000003                                              /* a small prime so collisions are easy to provoke */

/* Rabin-Karp: compare a rolling hash of each text window with the pattern's hash; verify real matches character by character. */
int main(void) {
    const char *t = "the cat sat on the mat with the other cat", *p = "the";
    int n = (int)strlen(t), m = (int)strlen(p);
    long ph = 0, th = 0, high = 1;
    for (int i = 0; i < m - 1; i++) high = high * BASE % MOD;    /* BASE^(m-1), the weight of the leading character */
    for (int i = 0; i < m; i++) { ph = (ph * BASE + p[i]) % MOD; th = (th * BASE + t[i]) % MOD; }
    int hash_hits = 0, real = 0;
    printf("matches of \"%s\" at:", p);
    for (int i = 0; i + m <= n; i++) {
        if (th == ph) {
            hash_hits++;
            if (strncmp(t + i, p, (size_t)m) == 0) { real++; printf(" %d", i); }     /* a hash match is only a candidate */
        }
        if (i + m < n) th = ((th - t[i] * high % MOD + MOD) * BASE + t[i + m]) % MOD;  /* drop t[i], add t[i + m]: O(1) per step */
    }
    printf("\nwindows with equal hash: %d, confirmed matches: %d\n", hash_hits, real);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p7_rolling p7_rolling.c && ./p7_rolling; rm -f p7_rolling
matches of "the" at: 0 15 28 33
windows with equal hash: 4, confirmed matches: 4
```

Treat each window as a number written in base 256 (modulo a prime). When the window slides, *subtract* the leading character's contribution (`t[i] × BASE^(m−1)`), *multiply* by the base to shift everything left, and *add* the new trailing character: O(1) per step, so the whole scan is O(n + m) on average instead of recomputing an m-character hash for every window. The hash comparison is only a **filter**: two different strings can share a hash (a collision), so every candidate window is confirmed with `strncmp`. Here every equal-hash window was a real match (4 of 4), but with this small modulus a collision is possible, and without verification the program would report false matches. A larger modulus (or two moduli, or a 64-bit modulus) makes collisions rare, which is why verification is cheap on average, but the worst case is still O(n·m). Rolling hashes are also the tool for finding duplicate substrings, plagiarism detection and content-defined chunking in backup tools.

</details>

## Key takeaways

1. The naive search is O(n·m) on adversarial input; KMP uses the failure function (longest proper prefix that is also a suffix) to avoid ever moving backwards, giving O(n + m).
2. A string has 2n − 1 palindrome centres; expanding from each costs O(n²) overall, and Manacher's algorithm gets O(n).
3. A rolling hash updates in O(1) per slide; a matching hash only nominates a candidate, so always verify it.
