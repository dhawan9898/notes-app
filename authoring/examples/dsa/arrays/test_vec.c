#include <stdio.h>
#include <stdlib.h>
#include "vec.h"

/* Differential test: apply random operations to Vec and to a simple fixed-size reference array; compare after every step. */
int main(void) {
    enum { MAXN = 512 };
    int ref[MAXN]; size_t rn = 0;
    Vec v; vec_init(&v, sizeof(int));
    srand(12345);
    long ops = 0;
    for (int step = 0; step < 200000; step++) {
        int op = rand() % 6, x = rand();
        switch (op) {
        case 0: case 1: if (rn < MAXN) { VEC_PUSH(int, &v, x); ref[rn++] = x; ops++; } break;
        case 2: { int out; int rc = vec_pop(&v, &out); if (rn) { assert(rc == 0 && out == ref[rn - 1]); rn--; ops++; } else assert(rc == -1); } break;
        case 3: if (rn < MAXN) { size_t i = (size_t)rand() % (rn + 1); vec_insert(&v, i, &x); memmove(ref + i + 1, ref + i, (rn - i) * sizeof(int)); ref[i] = x; rn++; ops++; } break;
        case 4: if (rn) { size_t i = (size_t)rand() % rn; vec_remove(&v, i); memmove(ref + i, ref + i + 1, (rn - i - 1) * sizeof(int)); rn--; ops++; } break;
        case 5: if (rn) { size_t i = (size_t)rand() % rn; vec_swap_remove(&v, i); ref[i] = ref[rn - 1]; rn--; ops++; } break;
        }
        assert(v.len == rn);
        for (size_t i = 0; i < rn; i++) assert(VEC_GET(int, &v, i) == ref[i]);
        if (step % 50000 == 49999) vec_shrink_to_fit(&v);
    }
    printf("%ld random operations matched the reference model; final len=%zu cap=%zu\n", ops, v.len, v.cap);
    vec_free(&v);
    return 0;
}
