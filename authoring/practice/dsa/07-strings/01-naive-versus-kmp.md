---
kind: question
title: Naive versus KMP
module: Strings
difficulty: Hard
---

Search for the pattern `aaaaaaaaab` in a text of 2,000 letters `a`. How many character comparisons does the naive algorithm make, and why is this input a bad case? Implement Knuth-Morris-Pratt (KMP) and compare. What does the "failure function" store?

## Explanation

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
