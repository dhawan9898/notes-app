/* The dereference happens first, so p must be non-null; the check is "dead". */
int read_checked(int *p) {
    int v = *p;
    if (p == 0)
        return -1;
    return v;
}
