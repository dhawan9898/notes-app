---
title: "Practice: Trees, Heaps and Tries"
track: dsa
module: Trees
moduleOrder: 5
order: 9
level: advanced
summary: "Four exercises on the trees module: validating a binary search tree correctly, finding the k-th smallest key and checking balance, selecting the k largest items with a size-k min-heap, and counting words by prefix with a trie, each with a verified C answer."
prerequisites:
  - "Binary Trees and Binary Search Trees"
  - "Heaps and Priority Queues"
  - "Tries, Radix Trees and Longest-Prefix Match"
sources:
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 6 (Heapsort), Ch. 12 (Binary Search Trees)"
  - label: "Sedgewick & Wayne — Algorithms, 4th ed., §2.4 (Priority Queues), §3.2, §5.2 (Tries)"
  - label: "Knuth — The Art of Computer Programming, Vol. 3, §6.3 (Digital Searching)"
---

Tree problems reward getting the *invariant* right. For each exercise, state the invariant in one sentence before writing code, and test the nasty case (the trap tree in exercise 1, the empty prefix, a chain).

## Exercises

Exercises that ask you to write or fix a function come with a **starter program**: open it, press **Run** to see which tests fail, then **Edit code**, change only the part between the "Your code" markers and run it again until every check passes. The checks print what they expected and what your code returned.

**1. Validate a BST.** Write `valid(root)`. A common wrong answer checks only that each node is greater than its left child and smaller than its right child. Build a tree that passes that check but is *not* a BST, and explain the correct invariant.

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 1: is a binary tree a valid binary search tree?
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node { int key; struct Node *l, *r; } Node;

/* ===== Your code: edit below this line ===== */
/* Every key in the left subtree must be smaller than the node's key, every key in the right subtree larger.
   Hint: pass down the range of keys allowed at each node. */
int is_bst(const Node *root) {
    (void)root;
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

static Node pool[32];
static int used;
static Node *mk(int key, Node *l, Node *r) { Node *n = &pool[used++]; n->key = key; n->l = l; n->r = r; return n; }

int main(void) {
    Node *good = mk(50, mk(30, mk(20, NULL, NULL), mk(40, NULL, NULL)), mk(70, mk(60, NULL, NULL), mk(80, NULL, NULL)));
    Node *trap = mk(50, mk(30, mk(20, NULL, NULL), mk(55, NULL, NULL)), mk(70, NULL, NULL));   /* 55 > 30, but it is left of 50 */
    Node *flipped = mk(50, mk(70, NULL, NULL), mk(30, NULL, NULL));
    check_int("is_bst(good tree)", is_bst(good), 1);
    check_int("is_bst(55 under 30, left of 50)", is_bst(trap), 0);
    check_int("is_bst(children swapped)", is_bst(flipped), 0);
    check_int("is_bst(empty tree)", is_bst(NULL), 1);
    check_int("is_bst(single node)", is_bst(mk(1, NULL, NULL)), 1);
    return summary();
}
```

</details>

**2. k-th smallest and balance.** Find the k-th smallest key without copying the tree into an array (stop early). Then write a function that reports whether a tree is height-balanced (subtree heights differ by at most 1 at **every** node) in one pass.

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 2: the k-th smallest key without copying the tree, and a height-balance check.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node { int key; struct Node *l, *r; } Node;

/* ===== Your code: edit below this line ===== */
/* Store the k-th smallest key (k = 1 is the smallest) in *out and return 1, or return 0 if the tree has fewer than k keys. */
int kth_smallest(const Node *root, int k, int *out) {
    (void)root; (void)k; (void)out;
    return 0;
}

/* Return 1 if, at every node, the heights of the two subtrees differ by at most 1. */
int is_balanced(const Node *root) {
    (void)root;
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

static Node pool[32];
static int used;
static Node *mk(int key, Node *l, Node *r) { Node *n = &pool[used++]; n->key = key; n->l = l; n->r = r; return n; }

int main(void) {
    Node *good = mk(50, mk(30, mk(20, NULL, NULL), mk(40, NULL, NULL)), mk(70, mk(60, NULL, NULL), mk(80, NULL, NULL)));
    int v = -1;
    check_int("kth_smallest(good, 3) found", kth_smallest(good, 3, &v), 1);
    check_int("  key", v, 40);
    v = -1; kth_smallest(good, 1, &v);
    check_int("kth_smallest(good, 1)", v, 20);
    v = -1; kth_smallest(good, 7, &v);
    check_int("kth_smallest(good, 7)", v, 80);
    check_int("kth_smallest(good, 8) (only 7 keys)", kth_smallest(good, 8, &v), 0);
    check_int("is_balanced(good)", is_balanced(good), 1);
    check_int("is_balanced(chain 1-2-3)", is_balanced(mk(1, NULL, mk(2, NULL, mk(3, NULL, NULL)))), 0);
    check_int("is_balanced(empty)", is_balanced(NULL), 1);
    /* root heights match (2 and 2) but each child is a chain: balance must hold at every node */
    Node *deep = mk(10, mk(5, mk(4, mk(3, NULL, NULL), NULL), NULL), mk(15, NULL, mk(16, NULL, mk(17, NULL, NULL))));
    check_int("is_balanced(equal heights at the root only)", is_balanced(deep), 0);
    return summary();
}
```

</details>

**3. Top-k with a heap.** From a stream of *n* integers, report the *k* largest using O(k) memory. Which kind of heap (min or max) do you need, and why? State the time complexity.

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 3: the k largest values of a stream with O(k) memory (a min-heap of size k).
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Your code: edit below this line ===== */
/* Write the k largest values of data[0..n) into out[0..k), smallest first. k <= n. */
void top_k(const int *data, int n, int k, int *out) {
    (void)data; (void)n; (void)k; (void)out;
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
    int data[] = {42, 7, 99, 15, 3, 88, 61, 27, 54, 70, 12, 95};
    int out[12] = {0};
    top_k(data, 12, 4, out);
    const int want4[] = {70, 88, 95, 99};
    check_arr("top_k(data, 4)", out, 4, want4, 4);
    top_k(data, 12, 1, out);
    const int want1[] = {99};
    check_arr("top_k(data, 1)", out, 1, want1, 1);
    int dup[] = {5, 9, 9, 1, 9};
    top_k(dup, 5, 3, out);
    const int want_dup[] = {9, 9, 9};
    check_arr("top_k({5,9,9,1,9}, 3)", out, 3, want_dup, 3);
    int asc[] = {1, 2, 3, 4, 5, 6};
    top_k(asc, 6, 6, out);
    check_arr("top_k(1..6, 6) (k == n)", out, 6, asc, 6);
    return summary();
}
```

</details>

**4. Prefix counting.** Build a trie over the words `car, card, care, cart, cat, dog, do` and answer "how many words start with this prefix?" in time proportional to the prefix length. Why must a trie node also record whether a word *ends* there?

<details class="starter">
<summary>Starter program with tests</summary>

```c
// Exercise 4: a trie that counts the words starting with a prefix in time proportional to the prefix.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* words: lowercase a-z only */
typedef struct TrieNode {
    struct TrieNode *child[26];
    int words_below;    /* words that pass through (or end at) this node */
    int is_word;        /* a word ends exactly here */
} TrieNode;

static TrieNode *new_node(void) { return calloc(1, sizeof(TrieNode)); }

/* ===== Your code: edit below this line ===== */
void trie_insert(TrieNode *root, const char *word) {
    (void)root; (void)word;
}

int count_prefix(const TrieNode *root, const char *prefix) {
    (void)root; (void)prefix;
    return 0;
}

int contains(const TrieNode *root, const char *word) {
    (void)root; (void)word;
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

static void free_trie(TrieNode *n) {
    if (!n) return;
    for (int i = 0; i < 26; i++) free_trie(n->child[i]);
    free(n);
}

int main(void) {
    TrieNode *root = new_node();
    const char *words[] = {"car", "card", "care", "cart", "cat", "dog", "do"};
    for (int i = 0; i < 7; i++) trie_insert(root, words[i]);
    check_int("count_prefix(\"car\")", count_prefix(root, "car"), 4);
    check_int("count_prefix(\"ca\")", count_prefix(root, "ca"), 5);
    check_int("count_prefix(\"do\")", count_prefix(root, "do"), 2);
    check_int("count_prefix(\"x\")", count_prefix(root, "x"), 0);
    check_int("count_prefix(\"\") (every word)", count_prefix(root, ""), 7);
    check_int("contains(\"car\")", contains(root, "car"), 1);
    check_int("contains(\"ca\") (a prefix is not a word)", contains(root, "ca"), 0);
    check_int("contains(\"cards\")", contains(root, "cards"), 0);
    free_trie(root);
    return summary();
}
```

</details>

## Answers

Try before opening.

<details>
<summary>Show answers and verified programs</summary>

**1 and 2.**

```c
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct node { int key; struct node *l, *r; } Node;

static Node *mk(int k, Node *l, Node *r) { Node *n = malloc(sizeof *n); if (!n) exit(1); n->key = k; n->l = l; n->r = r; return n; }
static void destroy(Node *n) { if (n) { destroy(n->l); destroy(n->r); free(n); } }

/* Valid BST: EVERY key in the left subtree is smaller and every key in the right is larger, not just the direct children. */
static int valid(const Node *n, long lo, long hi) {
    if (!n) return 1;
    if (n->key <= lo || n->key >= hi) return 0;
    return valid(n->l, lo, n->key) && valid(n->r, n->key, hi);
}

/* k-th smallest by an in-order walk that stops early (k counts down). */
static int kth(const Node *n, int *k, int *out) {
    if (!n) return 0;
    if (kth(n->l, k, out)) return 1;
    if (--*k == 0) { *out = n->key; return 1; }
    return kth(n->r, k, out);
}

/* Height-balanced: for every node, subtree heights differ by at most 1. Returns height, or -1 if unbalanced. */
static int balanced_height(const Node *n) {
    if (!n) return 0;
    int l = balanced_height(n->l), r = balanced_height(n->r);
    if (l < 0 || r < 0 || abs(l - r) > 1) return -1;
    return 1 + (l > r ? l : r);
}

int main(void) {
    Node *good = mk(50, mk(30, mk(20, NULL, NULL), mk(40, NULL, NULL)), mk(70, mk(60, NULL, NULL), mk(80, NULL, NULL)));
    Node *trap = mk(50, mk(30, mk(20, NULL, NULL), mk(55, NULL, NULL)), mk(70, NULL, NULL));   /* 55 is 'right of 30' but left of 50: invalid */
    printf("good tree valid: %d, trap tree valid: %d\n", valid(good, LONG_MIN, LONG_MAX), valid(trap, LONG_MIN, LONG_MAX));
    printf("trap tree checked only against parents would pass: 55 > 30 is %d\n", 55 > 30);
    int k = 3, out = -1;
    kth(good, &k, &out);
    printf("3rd smallest in good tree: %d\n", out);
    printf("good balanced: %s\n", balanced_height(good) >= 0 ? "yes" : "no");
    Node *chain = mk(1, NULL, mk(2, NULL, mk(3, NULL, NULL)));
    printf("chain 1-2-3 balanced: %s\n", balanced_height(chain) >= 0 ? "yes" : "no");
    destroy(good); destroy(trap); destroy(chain);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p5_bst p5_bst.c && ./p5_bst; rm -f p5_bst
good tree valid: 1, trap tree valid: 0
trap tree checked only against parents would pass: 55 > 30 is 1
3rd smallest in good tree: 40
good balanced: yes
chain 1-2-3 balanced: no
```

*Validation.* The invariant is about **ranges**, not neighbours: every key in a node's left subtree must be smaller than the node *and* larger than everything above that bound. The trap tree has `55` as the right child of `30` (55 > 30, so the parent-only check passes) but it sits in the left subtree of `50`, so it is too large. The correct check passes an open interval `(lo, hi)` down the recursion: going left sets `hi = node->key`, going right sets `lo = node->key`. (Use a wider type than the keys for the initial bounds, as the code does with `long`, or the extreme key `INT_MIN` would be wrongly rejected.) An alternative proof: an in-order traversal of a valid BST is strictly increasing.

*k-th smallest.* An in-order walk visits keys in sorted order, so count down `k` and stop when it reaches zero: O(height + k) time, O(height) stack. Augmenting each node with its subtree size gives O(height) lookup by rank.

*Balance.* The function returns the height, or `-1` as an "unbalanced" signal that propagates up, so each node is visited once (O(n)). Computing heights separately at every node would be O(n²) on a chain. The chain `1-2-3` is correctly reported unbalanced.

**3.**

```c
#include <stdio.h>
#include <stdlib.h>

/* Min-heap of ints in an array: parent of i is (i-1)/2, children 2i+1 and 2i+2. */
typedef struct { int *a; int n, cap; } Heap;

static void swap(int *x, int *y) { int t = *x; *x = *y; *y = t; }
static void push(Heap *h, int v) {
    int i = h->n++; h->a[i] = v;
    while (i > 0 && h->a[(i - 1) / 2] > h->a[i]) { swap(&h->a[(i - 1) / 2], &h->a[i]); i = (i - 1) / 2; }   /* sift up */
}
static int pop_min(Heap *h) {
    int top = h->a[0]; h->a[0] = h->a[--h->n];
    for (int i = 0;;) {                                                                                       /* sift down */
        int l = 2 * i + 1, r = l + 1, m = i;
        if (l < h->n && h->a[l] < h->a[m]) m = l;
        if (r < h->n && h->a[r] < h->a[m]) m = r;
        if (m == i) break;
        swap(&h->a[i], &h->a[m]); i = m;
    }
    return top;
}

int main(void) {
    int data[] = {42, 7, 99, 15, 3, 88, 61, 27, 54, 70, 12, 95};
    int n = (int)(sizeof data / sizeof data[0]), k = 4;

    /* Top-k largest with a min-heap of size k: O(n log k) time and O(k) space. */
    Heap h = {malloc((size_t)k * sizeof(int)), 0, k};
    for (int i = 0; i < n; i++) {
        if (h.n < k) push(&h, data[i]);
        else if (data[i] > h.a[0]) { pop_min(&h); push(&h, data[i]); }      /* beats the smallest of the current top k */
    }
    printf("the %d largest, smallest first:", k);
    while (h.n) printf(" %d", pop_min(&h));
    printf("\n");
    free(h.a);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p5_heap p5_heap.c && ./p5_heap; rm -f p5_heap
the 4 largest, smallest first: 70 88 95 99
```

You need a **min-heap of size k**, which seems backwards but is the point: its root is the *smallest of the current top k*, i.e. the item most likely to be displaced. A new value only matters if it beats that root; then the root is popped and the new value pushed. Each of the *n* items costs at most one pop and one push, O(log k), so the total is **O(n log k)** time and **O(k)** space, and it works on a stream you cannot store. Sorting everything would take O(n log n) time and O(n) memory. (For one-off selection from an array in memory, quickselect is O(n) on average.)

**4.**

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct trie { struct trie *next[26]; int words_through; int is_word; } Trie;

static Trie *new_node(void) { Trie *t = calloc(1, sizeof *t); if (!t) exit(1); return t; }
static void insert(Trie *root, const char *w) {
    Trie *t = root;
    for (; *w; w++) {
        int c = *w - 'a';
        if (!t->next[c]) t->next[c] = new_node();
        t = t->next[c];
        t->words_through++;                           /* how many inserted words pass through this node */
    }
    t->is_word = 1;
}
static int count_prefix(const Trie *root, const char *p) {
    const Trie *t = root;
    for (; *p; p++) { t = t->next[*p - 'a']; if (!t) return 0; }
    return t->words_through;
}
static int contains(const Trie *root, const char *w) {
    const Trie *t = root;
    for (; *w; w++) { t = t->next[*w - 'a']; if (!t) return 0; }
    return t->is_word;
}
static void destroy(Trie *t) { if (!t) return; for (int i = 0; i < 26; i++) destroy(t->next[i]); free(t); }

int main(void) {
    const char *words[] = {"car", "card", "care", "cart", "cat", "dog", "do"};
    Trie *root = new_node();
    for (int i = 0; i < 7; i++) insert(root, words[i]);
    printf("words with prefix \"car\": %d, \"ca\": %d, \"do\": %d, \"x\": %d\n",
           count_prefix(root, "car"), count_prefix(root, "ca"), count_prefix(root, "do"), count_prefix(root, "x"));
    printf("contains \"car\": %d, \"ca\": %d (a prefix is not automatically a word), \"card\": %d\n",
           contains(root, "car"), contains(root, "ca"), contains(root, "card"));
    destroy(root);
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o p5_trie p5_trie.c && ./p5_trie; rm -f p5_trie
words with prefix "car": 4, "ca": 5, "do": 2, "x": 0
contains "car": 1, "ca": 0 (a prefix is not automatically a word), "card": 1
```

Each node counts how many inserted words pass through it (`words_through`), so after walking the prefix in O(length of prefix), that node's counter *is* the answer: 4 words start with `car`, 5 with `ca`. The separate `is_word` flag is needed because a prefix is not necessarily a word: `ca` is on the path of five words but is not itself one (`contains("ca")` is 0), whereas `car` is both a word and a prefix of three others. The cost is memory: this layout uses 26 pointers per node; compressed (radix) tries and hash- or array-mapped children reduce it.

</details>

## Key takeaways

1. A BST's invariant spans whole subtrees (a range check), not just parent-child pairs; an in-order traversal must be strictly increasing.
2. Return a sentinel (such as `-1`) from a recursive height function to check balance in one pass instead of O(n²).
3. Top-k selection uses a min-heap of size k: O(n log k) time and O(k) memory, and it works on streams.
4. Tries answer prefix questions in O(prefix length); store a count per node for prefix totals and a separate end-of-word flag.
