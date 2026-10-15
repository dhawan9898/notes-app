static void inorder(const Node *n, int *k, int *out, int *found) {
    if (!n || *found) return;                      /* stop early once the answer is known */
    inorder(n->l, k, out, found);
    if (*found) return;
    if (--*k == 0) { *out = n->key; *found = 1; return; }
    inorder(n->r, k, out, found);
}

int kth_smallest(const Node *root, int k, int *out) {
    int found = 0;
    inorder(root, &k, out, &found);
    return found;
}

static int height_or_minus1(const Node *n) {       /* height, or -1 as soon as some subtree is unbalanced */
    if (!n) return 0;
    int hl = height_or_minus1(n->l), hr = height_or_minus1(n->r);
    if (hl < 0 || hr < 0 || hl - hr > 1 || hr - hl > 1) return -1;
    return 1 + (hl > hr ? hl : hr);
}

int is_balanced(const Node *root) {
    return height_or_minus1(root) >= 0;
}
