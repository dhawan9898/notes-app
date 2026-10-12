#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include "avl.h"
#include "rb.h"

static uint64_t s = 88172645463325252ULL;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 11); }

int main(void) {
    /* 1. differential + invariant test: AVL and RB vs a membership table, invariants verified after EVERY operation */
    enum { RANGE = 1500, OPS = 100000 };
    static char present[RANGE]; AVL *avl = NULL; RBT rb; rb_init(&rb);
    for (int i = 0; i < OPS; i++) {
        int k = (int)(rnd() % RANGE), op = (int)(rnd() % 3);
        if (op == 0) {
            int ins = 0; avl = avl_insert(avl, k, &ins); int ins2 = rb_insert(&rb, k);
            assert(ins == !present[k] && ins2 == ins); present[k] = 1;
        } else if (op == 1) {
            int del = 0; avl = avl_delete(avl, k, &del); int del2 = rb_delete(&rb, k);
            assert(del == present[k] && del2 == del); present[k] = 0;
        } else {
            assert((avl_find(avl, k) != NULL) == present[k] && rb_find(&rb, k) == present[k]);
        }
        assert(avl_check(avl, -1, RANGE) >= 0);
        assert(rb_check(&rb));
    }
    printf("random insert/delete/find, %d ops: AVL and red-black invariants held after every operation\n\n", OPS);
    avl_free(avl); rb_free(&rb);

    /* 2. worst-case height: sorted insertion */
    printf("%9s %8s | %12s %12s | %12s %12s | %9s\n", "n", "ceil lg", "AVL sorted", "AVL random", "RB sorted", "RB random", "1.44lg n");
    for (int n = 1000; n <= 1000000; n *= 10) {
        AVL *a1 = NULL, *a2 = NULL; RBT r1, r2; rb_init(&r1); rb_init(&r2); int d;
        for (int i = 0; i < n; i++) { d = 0; a1 = avl_insert(a1, i, &d); rb_insert(&r1, i); }
        for (int i = 0; i < n; i++) { int k = (int)(rnd() % (4u * (unsigned)n)); d = 0; a2 = avl_insert(a2, k, &d); rb_insert(&r2, k); }
        printf("%9d %8d | %12d %12d | %12d %12d | %9.1f\n", n, (int)ceil(log2(n + 1)), a1->h, a2->h, rb_height(&r1, r1.root), rb_height(&r2, r2.root), 1.44 * log2(n));
        avl_free(a1); avl_free(a2); rb_free(&r1); rb_free(&r2);
    }
    return 0;
}
