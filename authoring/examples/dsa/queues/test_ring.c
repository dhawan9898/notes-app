#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "ring.h"

/* Differential test against a naive deque model (array with memmove). */
int main(void) {
    enum { CAP = 16 };
    Ring r; ring_init(&r, CAP);
    int model[CAP]; size_t n = 0;
    srand(99);
    long pushes = 0, wraps = 0;
    for (int step = 0; step < 300000; step++) {
        int v = rand();
        switch (rand() % 4) {
        case 0: { bool ok = ring_push(&r, v); assert(ok == (n < CAP)); if (ok) { model[n++] = v; pushes++; } break; }
        case 1: { int out; bool ok = ring_pop(&r, &out); assert(ok == (n > 0)); if (ok) { assert(out == model[0]); memmove(model, model + 1, (n - 1) * sizeof *model); n--; } break; }
        case 2: { bool ok = ring_push_front(&r, v); assert(ok == (n < CAP)); if (ok) { memmove(model + 1, model, n * sizeof *model); model[0] = v; n++; } break; }
        case 3: { int out; bool ok = ring_pop_back(&r, &out); assert(ok == (n > 0)); if (ok) { assert(out == model[n - 1]); n--; } break; }
        }
        assert(ring_len(&r) == n);
        if (r.tail > 4 * CAP && (r.tail & r.mask) < 3) wraps++;
    }
    printf("300000 random deque operations matched the model (%ld successful pushes, counters wrapped past capacity many times)\n", pushes);
    ring_free(&r);
    return 0;
}
