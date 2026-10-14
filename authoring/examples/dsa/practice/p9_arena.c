#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { unsigned char *base; size_t size, used; } Arena;

static int arena_init(Arena *a, size_t size) { a->base = malloc(size); a->size = size; a->used = 0; return a->base != NULL; }

/* Bump allocation: round the offset up to the alignment, hand out the next bytes. No per-object free, no bookkeeping. */
static void *arena_alloc(Arena *a, size_t n, size_t align) {
    size_t off = (a->used + (align - 1)) & ~(align - 1);
    if (off + n > a->size) return NULL;                    /* out of space: the caller decides what to do */
    a->used = off + n;
    return a->base + off;
}
static void arena_reset(Arena *a) { a->used = 0; }          /* frees EVERYTHING at once in O(1) */

typedef struct node { int value; struct node *next; } Node;

int main(void) {
    Arena a;
    if (!arena_init(&a, 4096)) return 1;
    Node *head = NULL;
    for (int i = 1; i <= 100; i++) {                        /* a 100-node list with no malloc call per node and no free loop */
        Node *n = arena_alloc(&a, sizeof *n, _Alignof(Node));
        n->value = i; n->next = head; head = n;
    }
    long sum = 0; for (Node *p = head; p; p = p->next) sum += p->value;
    printf("list sum = %ld using %zu of %zu arena bytes (%zu per node)\n", sum, a.used, a.size, sizeof(Node));

    unsigned char *c = arena_alloc(&a, 1, 1); int *x = arena_alloc(&a, sizeof *x, _Alignof(int));
    printf("after a 1-byte char, the next int is %zu bytes further on and aligned: %s\n", (size_t)((unsigned char *)x - c), ((uintptr_t)x % _Alignof(int)) == 0 ? "yes" : "NO");
    arena_reset(&a);
    printf("after reset: %zu bytes used; the whole list was released in one step\n", a.used);
    free(a.base);
    return 0;
}
