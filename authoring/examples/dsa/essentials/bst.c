#include <stdio.h>
#include <stdlib.h>

typedef struct tnode { int key; struct tnode *left, *right; } TNode;

static TNode *insert(TNode *root, int key) {          /* smaller keys go left, larger go right */
    if (!root) {
        TNode *n = calloc(1, sizeof *n);
        if (!n) exit(1);
        n->key = key;
        return n;
    }
    if (key < root->key) root->left = insert(root->left, key);
    else if (key > root->key) root->right = insert(root->right, key);
    return root;                                      /* duplicates are ignored */
}
static int contains(const TNode *root, int key) {
    while (root) {
        if (key == root->key) return 1;
        root = key < root->key ? root->left : root->right;     /* discard half the tree each step */
    }
    return 0;
}
static void inorder(const TNode *root) {              /* left, node, right: visits keys in sorted order */
    if (!root) return;
    inorder(root->left); printf("%d ", root->key); inorder(root->right);
}
static int height(const TNode *root) {
    if (!root) return 0;
    int l = height(root->left), r = height(root->right);
    return 1 + (l > r ? l : r);
}
static void destroy(TNode *root) { if (root) { destroy(root->left); destroy(root->right); free(root); } }

int main(void) {
    int keys[] = {50, 30, 70, 20, 40, 60, 80, 30};    /* the repeated 30 is ignored */
    TNode *root = NULL;
    for (int i = 0; i < 8; i++) root = insert(root, keys[i]);
    printf("in order: "); inorder(root); printf("\n");
    printf("contains 60: %d, contains 65: %d, height: %d\n", contains(root, 60), contains(root, 65), height(root));
    destroy(root);

    TNode *chain = NULL;                               /* the same idea with sorted input: a lopsided tree */
    for (int k = 1; k <= 7; k++) chain = insert(chain, k);
    printf("inserting 1..7 in order gives height %d (a balanced tree would be 3)\n", height(chain));
    destroy(chain);
    return 0;
}
