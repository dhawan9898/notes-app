#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A bump-pointer arena: very fast allocation, freed all at once. */
typedef struct {
    unsigned char *base;
    size_t cap, used;
} Arena;

static int arena_init(Arena *a, size_t cap) {
    a->base = malloc(cap);
    a->cap = cap;
    a->used = 0;
    return a->base != NULL;
}

static void *arena_alloc(Arena *a, size_t size, size_t align) {
    uintptr_t cur = (uintptr_t)a->base + a->used;
    uintptr_t aligned = (cur + (align - 1)) & ~(uintptr_t)(align - 1);   /* align must be a power of two */
    size_t offset = (size_t)(aligned - (uintptr_t)a->base);
    if (offset > a->cap || size > a->cap - offset) return NULL;           /* overflow-safe bounds check */
    a->used = offset + size;
    return a->base + offset;
}

static void arena_reset(Arena *a) { a->used = 0; }
static void arena_free(Arena *a) { free(a->base); a->base = NULL; }

#define ARENA_NEW(a, T) ((T *)arena_alloc((a), sizeof(T), alignof(T)))
#define ARENA_ARRAY(a, T, n) ((T *)arena_alloc((a), sizeof(T) * (n), alignof(T)))

typedef struct Node { int value; struct Node *next; } Node;

int main(void) {
    Arena a;
    if (!arena_init(&a, 1 << 16)) return 1;

    Node *head = NULL;
    for (int i = 0; i < 1000; i++) {           /* 1000 nodes, no individual frees */
        Node *n = ARENA_NEW(&a, Node);
        n->value = i; n->next = head; head = n;
    }
    char *tag = arena_alloc(&a, 3, 1);          /* odd size, then an aligned double */
    double *d = ARENA_NEW(&a, double);
    memcpy(tag, "ab", 3);
    *d = 3.14;
    printf("nodes: head=%d used=%zu bytes of %zu; double aligned: %s\n", head->value, a.used, a.cap,
           ((uintptr_t)d % alignof(double)) == 0 ? "yes" : "NO");
    printf("big request fails cleanly: %s\n", ARENA_ARRAY(&a, double, 1000000) ? "BUG" : "NULL");
    arena_reset(&a);                            /* everything freed in O(1) */
    printf("after reset used=%zu\n", a.used);
    arena_free(&a);
    return 0;
}
