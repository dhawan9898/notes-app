int vec_push(IntVec *v, int x) {
    if (v->size == v->cap) {
        size_t ncap = v->cap ? v->cap * 2 : 4;
        int *nd = realloc(v->data, ncap * sizeof *nd);
        if (!nd) return 0;              /* the old block is still valid and still owned by v */
        v->data = nd;
        v->cap = ncap;
    }
    v->data[v->size++] = x;
    return 1;
}

void vec_free(IntVec *v) {
    free(v->data);
    v->data = NULL;
    v->size = v->cap = 0;
}
