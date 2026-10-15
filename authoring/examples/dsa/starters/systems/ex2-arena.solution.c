void *arena_alloc(Arena *a, size_t size, size_t align) {
    uintptr_t start = (uintptr_t)a->base + a->used;
    uintptr_t aligned = (start + align - 1) & ~(uintptr_t)(align - 1);      /* round up to a multiple of align */
    size_t offset = (size_t)(aligned - (uintptr_t)a->base);
    if (offset > a->cap || size > a->cap - offset) return NULL;
    a->used = offset + size;
    return a->base + offset;
}

void arena_reset(Arena *a) {
    a->used = 0;
}
