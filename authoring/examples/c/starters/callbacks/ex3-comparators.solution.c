int by_age(const void *x, const void *y) {
    const Person *a = x, *b = y;
    return (a->age > b->age) - (a->age < b->age);
}

int by_name(const void *x, const void *y) {
    const Person *a = x, *b = y;
    return strcmp(a->name, b->name);
}
