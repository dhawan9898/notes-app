int by_age(const void *x, const void *y) {
    const Person *a = x, *b = y;
    return a->age - b->age;         /* overflows for far-apart ages */
}

int by_name(const void *x, const void *y) {
    return strcmp(((const Person *)x)->name, ((const Person *)y)->name);
}
