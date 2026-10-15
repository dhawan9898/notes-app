static int within(const Node *n, long lo, long hi) {     /* every key must satisfy lo < key < hi */
    if (!n) return 1;
    if (n->key <= lo || n->key >= hi) return 0;
    return within(n->l, lo, n->key) && within(n->r, n->key, hi);
}

int is_bst(const Node *root) {
    return within(root, LONG_MIN, LONG_MAX);
}
