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
