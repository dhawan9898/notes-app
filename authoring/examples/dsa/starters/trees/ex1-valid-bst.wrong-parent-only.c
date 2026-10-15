int is_bst(const Node *root) {
    if (!root) return 1;
    if (root->l && root->l->key >= root->key) return 0;     /* compares each node only with its parent */
    if (root->r && root->r->key <= root->key) return 0;
    return is_bst(root->l) && is_bst(root->r);
}
