int balanced(const char *s) {
    int depth = 0;                  /* counts brackets but ignores their kind and order */
    for (; *s; s++) {
        if (*s == '(' || *s == '[' || *s == '{') depth++;
        else if (*s == ')' || *s == ']' || *s == '}') { if (--depth < 0) return 0; }
    }
    return depth == 0;
}
