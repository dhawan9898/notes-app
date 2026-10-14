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
