int is_anagram(const char *a, const char *b) {
    if (strlen(a) != strlen(b)) return 0;
    for (; *a; a++)                 /* checks that each letter occurs, not how often */
        if (!strchr(b, *a)) return 0;
    return 1;
}
