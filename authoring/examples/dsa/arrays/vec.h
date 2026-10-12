#ifndef VEC_H
#define VEC_H
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* A generic dynamic array: elements are stored contiguously; size and capacity are in ELEMENTS. */
typedef struct {
    void  *data;
    size_t elem_size;
    size_t len, cap;
} Vec;

static inline void vec_init(Vec *v, size_t elem_size) { v->data = NULL; v->elem_size = elem_size; v->len = v->cap = 0; }
static inline void vec_free(Vec *v) { free(v->data); vec_init(v, v->elem_size); }

/* Ensure room for at least `want` elements. Returns 0 on success, -1 on overflow or allocation failure. */
static inline int vec_reserve(Vec *v, size_t want) {
    if (want <= v->cap) return 0;
    size_t cap = v->cap ? v->cap : 4;
    while (cap < want) { if (cap > SIZE_MAX / 2) return -1; cap *= 2; }               /* geometric growth: amortized O(1) push */
    if (cap > SIZE_MAX / v->elem_size) return -1;                                        /* byte-size overflow check */
    void *p = realloc(v->data, cap * v->elem_size);
    if (!p) return -1;                                                                   /* old block still valid on failure */
    v->data = p; v->cap = cap;
    return 0;
}

static inline void *vec_at(const Vec *v, size_t i) { assert(i < v->len && "vec index out of range"); return (char *)v->data + i * v->elem_size; }

static inline int vec_push(Vec *v, const void *elem) {
    if (vec_reserve(v, v->len + 1)) return -1;
    memcpy((char *)v->data + v->len * v->elem_size, elem, v->elem_size);
    v->len++;
    return 0;
}

static inline int vec_pop(Vec *v, void *out) {
    if (!v->len) return -1;
    v->len--;
    if (out) memcpy(out, (char *)v->data + v->len * v->elem_size, v->elem_size);
    return 0;
}

/* Insert at index i (0 <= i <= len): shifts the tail with memmove, O(len - i). */
static inline int vec_insert(Vec *v, size_t i, const void *elem) {
    if (i > v->len) return -1;
    if (vec_reserve(v, v->len + 1)) return -1;
    char *base = v->data;
    memmove(base + (i + 1) * v->elem_size, base + i * v->elem_size, (v->len - i) * v->elem_size);
    memcpy(base + i * v->elem_size, elem, v->elem_size);
    v->len++;
    return 0;
}

/* Remove at index i, preserving order: O(len - i). vec_swap_remove is O(1) but changes order. */
static inline int vec_remove(Vec *v, size_t i) {
    if (i >= v->len) return -1;
    char *base = v->data;
    memmove(base + i * v->elem_size, base + (i + 1) * v->elem_size, (v->len - i - 1) * v->elem_size);
    v->len--;
    return 0;
}
static inline int vec_swap_remove(Vec *v, size_t i) {
    if (i >= v->len) return -1;
    char *base = v->data;
    if (i != v->len - 1) memcpy(base + i * v->elem_size, base + (v->len - 1) * v->elem_size, v->elem_size);
    v->len--;
    return 0;
}

/* Give memory back (call after a big shrink). */
static inline int vec_shrink_to_fit(Vec *v) {
    if (v->len == v->cap) return 0;
    if (!v->len) { free(v->data); v->data = NULL; v->cap = 0; return 0; }
    void *p = realloc(v->data, v->len * v->elem_size);
    if (!p) return -1;
    v->data = p; v->cap = v->len;
    return 0;
}

/* Typed convenience macros on top of the generic core. */
#define VEC_GET(T, v, i)  (*(T *)vec_at((v), (i)))
#define VEC_PUSH(T, v, x) do { T _tmp = (x); vec_push((v), &_tmp); } while (0)
#endif
