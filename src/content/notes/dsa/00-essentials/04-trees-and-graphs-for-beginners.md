---
title: "Trees and Graphs for Beginners"
track: dsa
module: DSA Essentials
moduleOrder: 0.5
order: 4
level: beginner
summary: "The pictures behind trees and graphs: a binary search tree built in C with insert, search and in-order traversal, why sorted input makes it lopsided, and an adjacency-matrix graph explored with breadth-first search to find shortest distances, as the on-ramp to the tree and graph notes."
prerequisites:
  - "Thinking in Data Structures"
  - "Structs, Enums and Typedef"
  - "Linked Lists, Stacks and Queues for Beginners"
  - "More Control Flow: switch, Loop Control and Recursion"
sources:
  - label: "Sedgewick & Wayne — Algorithms, 4th ed., §3.2 (Binary Search Trees) and §4.1 (Undirected Graphs)"
  - label: "Cormen, Leiserson, Rivest, Stein — Introduction to Algorithms, 4th ed., Ch. 12 (Binary Search Trees) and §20.2 (Breadth-first search)"
  - label: "Skiena — The Algorithm Design Manual, 3rd ed., Ch. 3 (Data Structures) and Ch. 7 (Graph Traversal)"
  - label: "Knuth — The Art of Computer Programming, Vol. 3, §6.2.2 (Binary Tree Searching)"
---

Two shapes recur throughout computer science. A **tree** organises data hierarchically (folders inside folders, the structure of an expression, an index inside a database). A **graph** describes anything connected by relationships (roads between towns, friends in a network, packets between routers). This note builds one of each in C, using only what the earlier beginner notes covered: structs, pointers, recursion and queues.

## Trees: nodes with children

A **tree** is a set of nodes in which each node has zero or more **children** and exactly one **parent**, except the top node, the **root**. A node with no children is a **leaf**. The **height** is the number of nodes on the longest path from the root down to a leaf. In a **binary tree** each node has at most two children, called left and right.

A **binary search tree** (BST) adds one rule that makes searching fast: for every node, all keys in its left subtree are **smaller** and all keys in its right subtree are **larger**. To find a key, compare with the root: if equal you are done; if smaller go left; if larger go right. Every comparison discards a whole subtree, just as binary search discards half an array.

```c
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
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o bst bst.c && ./bst; rm -f bst
in order: 20 30 40 50 60 70 80 
contains 60: 1, contains 65: 0, height: 3
inserting 1..7 in order gives height 7 (a balanced tree would be 3)
```

Study the output and the code:

- **`insert`** is recursive: it walks down by the same left/right rule until it reaches an empty spot (`NULL`), creates the node there, and returns it. Each node's pointer is reassigned on the way back, which is why it reads `root->left = insert(root->left, key)`. The duplicate `30` is ignored.
- **`inorder`** visits *left subtree, node, right subtree*. Because of the BST rule, that prints the keys **in sorted order**: 20 30 40 50 60 70 80. A BST is therefore a way to keep data sorted while still inserting and searching quickly.
- **`contains`** needs no recursion: it just follows the rule until it finds the key or hits `NULL`.
- The tree built from 50, 30, 70, 20, 40, 60, 80 has height 3, so a search takes at most 3 comparisons for 7 keys. In general a **balanced** tree with n keys has height about log₂ n.
- **The catch:** inserting 1, 2, 3, ..., 7 *in sorted order* produces a tree of height 7. Every key goes to the right of the previous one, and the tree degenerates into a linked list, making search O(n). Real libraries use **self-balancing** trees (AVL, red-black) that rearrange themselves to keep the height near log n; see *Binary Trees and Binary Search Trees* and *Self-Balancing Trees*.

`destroy` frees children before the parent (a *post-order* walk); freeing the parent first would lose the pointers to its children.

## Graphs: things and connections

A **graph** is a set of **vertices** (the things) and **edges** (the connections). Edges may be undirected (a road, usable both ways) or directed (a one-way street, a web link). Trees are graphs without cycles. Two common ways to store a graph in memory are:

- an **adjacency matrix**: a table where `adj[a][b]` is 1 if there is an edge from `a` to `b` (simple, O(1) edge test, uses n² memory);
- an **adjacency list**: for each vertex, a list of its neighbours (compact for graphs with few edges, the usual choice for large graphs; see *Graph Representations and Traversals*).

The most basic question about a graph is "what can I reach from here, and how far is it?". **Breadth-first search** (BFS) answers it using a **queue**: visit the start, then everything 1 edge away, then everything 2 edges away, and so on, because the queue serves vertices in the order they were discovered.

```c
#include <stdio.h>

#define N 7

/* An undirected graph as an adjacency matrix: adj[a][b] = 1 means a road between a and b. */
static int adj[N][N];

static void road(int a, int b) { adj[a][b] = adj[b][a] = 1; }

int main(void) {
    const char *name[N] = {"Home", "Shop", "School", "Park", "Cafe", "Gym", "Library"};
    road(0, 1); road(0, 2); road(1, 3); road(2, 3); road(3, 4); road(4, 5);   /* Library (6) is unreachable */

    /* Breadth-first search: visit everything 1 step away, then 2 steps, ... using a queue. */
    int dist[N], queue[N], head = 0, tail = 0;
    for (int i = 0; i < N; i++) dist[i] = -1;          /* -1 = not reached yet */
    dist[0] = 0; queue[tail++] = 0;
    while (head < tail) {
        int v = queue[head++];
        for (int w = 0; w < N; w++)
            if (adj[v][w] && dist[w] < 0) { dist[w] = dist[v] + 1; queue[tail++] = w; }
    }
    for (int i = 0; i < N; i++) {
        if (dist[i] < 0) printf("%-8s unreachable from Home\n", name[i]);
        else printf("%-8s %d step(s) from Home\n", name[i], dist[i]);
    }
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -fsanitize=address,undefined -o graph_bfs graph_bfs.c && ./graph_bfs; rm -f graph_bfs
Home     0 step(s) from Home
Shop     1 step(s) from Home
School   1 step(s) from Home
Park     2 step(s) from Home
Cafe     3 step(s) from Home
Gym      4 step(s) from Home
Library  unreachable from Home
```

`dist[w] < 0` means "not yet discovered", which also prevents revisiting a vertex and looping forever in a graph with cycles (Home to School to Park to Shop to Home is one). Because BFS explores in rings, the first time a vertex is reached is by a **shortest path** measured in edges: the Park is 2 steps from Home, the Gym 4. The Library has no road, so it stays unreachable, and BFS reports that too. Each vertex enters the queue once, so with a matrix the work is O(V²); with adjacency lists it is O(V + E).

## How the structures relate

| Structure | Shape | Typical question | Notes |
|---|---|---|---|
| Array / list | a line | "item number *i*?" "next item?" | the simplest; see earlier notes |
| Binary search tree | hierarchy with an order | "does this key exist?" "everything in order" | keep it balanced |
| Hash table | no shape | "value for this key?" | no ordering |
| Graph | general network | "is X reachable?" "shortest route?" | trees and lists are special cases |

## Pitfalls

- **Unbalanced trees from sorted input** (O(n) instead of O(log n)).
- **Forgetting the base case** `if (!root)` in a recursive tree function, which dereferences `NULL`.
- **Freeing a parent before its children.**
- **BFS without a "visited" marker** loops forever on a graph with cycles.
- **Using a matrix for a huge sparse graph** (a million vertices would need a trillion entries).
- **Assuming the graph is connected**: always handle unreachable vertices.

## Key takeaways

1. A binary search tree keeps keys ordered: left is smaller, right is larger; in-order traversal prints sorted data.
2. Search cost equals tree height, so balanced trees (height about log n) matter; sorted insertion into a plain BST creates a chain.
3. A graph is vertices plus edges, stored as a matrix or adjacency lists; trees are graphs without cycles.
4. BFS uses a queue and a visited marker, and finds shortest paths measured in edges.
