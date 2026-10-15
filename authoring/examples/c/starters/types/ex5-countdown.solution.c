int countdown(size_t n, size_t out[]) {
    int steps = 0;
    for (size_t i = n; i-- > 0; ) out[steps++] = i;     /* test first, then decrement: never goes below 0 */
    return steps;
}
