void trie_insert(TrieNode *root, const char *word) {
    TrieNode *n = root;
    n->words_below++;
    for (; *word; word++) {
        int c = *word - 'a';
        if (!n->child[c]) n->child[c] = new_node();
        n = n->child[c];
        n->words_below++;
    }
    n->is_word = 1;
}

static const TrieNode *walk(const TrieNode *n, const char *s) {
    for (; n && *s; s++) n = n->child[*s - 'a'];
    return n;
}

int count_prefix(const TrieNode *root, const char *prefix) {
    const TrieNode *n = walk(root, prefix);
    return n ? n->words_below : 0;
}

int contains(const TrieNode *root, const char *word) {
    const TrieNode *n = walk(root, word);
    return n && n->is_word;
}
