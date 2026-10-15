void remove_key(int key) {
    int p = find(key);
    if (p >= 0) slot[p] = TOMBSTONE;    /* "deleted, keep probing": find skips it, insert may reuse it */
}
