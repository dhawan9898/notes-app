// Exercise 2: an arena (bump) allocator with alignment and reset.
// Press Run to see which checks fail, then Edit code, change only the marked part, and Run again until every check passes.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { unsigned char *base; size_t cap, used; } Arena;

/* ===== Your code: edit below this line ===== */
/* Return size bytes aligned to align (a power of two) from the arena, or NULL if it does not fit. */
void *arena_alloc(Arena *a, size_t size, size_t align) {
    (void)a; (void)size; (void)align;
    return NULL;
}

/* Release everything at once. */
void arena_reset(Arena *a) {
    (void)a;
}
/* ===== End of your code ===== */

/* ===== Tests: no need to change anything below ===== */
static int checks, failed;

static void check_int(const char *what, long long got, long long want) {
    checks++;
    if (got == want) printf("PASS %s = %lld\n", what, got);
    else { failed++; printf("FAIL %s: expected %lld, got %lld\n", what, want, got); }
}

static int summary(void) {
    printf("%d of %d checks passed%s\n", checks - failed, checks, failed ? "" : ". Well done!");
    return failed != 0;
}

int main(void) {
    static _Alignas(16) unsigned char memory[4096];
    Arena a = {memory, sizeof memory, 0};
    typedef struct Node { int v; struct Node *next; } Node;
    Node *head = NULL;
    int built = 0;
    for (int i = 1; i <= 100; i++) {
        Node *n = arena_alloc(&a, sizeof *n, _Alignof(Node));
        if (!n) break;
        n->v = i;
        n->next = head;
        head = n;
        built++;
    }
    long sum = 0;
    for (Node *p = head; p; p = p->next) sum += p->v;
    check_int("nodes allocated from the arena", built, 100);
    check_int("sum of the list", sum, 5050);
    check_int("bytes used (no padding between equal-sized nodes)", (long long)a.used, (long long)(100 * sizeof(Node)));
    arena_reset(&a);
    check_int("bytes used after arena_reset", (long long)a.used, 0);
    char *c = arena_alloc(&a, 1, 1);
    int *x = arena_alloc(&a, sizeof *x, _Alignof(int));
    check_int("an int after a 1-byte char is aligned", x != NULL && (uintptr_t)x % _Alignof(int) == 0, 1);
    check_int("  and starts right after the padding", x != NULL && c != NULL && (char *)x - c == (long)_Alignof(int), 1);
    check_int("a request larger than the arena returns NULL", arena_alloc(&a, 5000, 1) == NULL, 1);
    return summary();
}
