int balanced(const char *s) {
    char stack[128];
    int sp = 0;
    for (; *s; s++) {
        if (*s == '(' || *s == '[' || *s == '{') {
            if (sp == (int)sizeof stack) return 0;
            stack[sp++] = *s;
        } else if (*s == ')' || *s == ']' || *s == '}') {
            if (sp == 0) return 0;
            char open = stack[--sp];
            if ((*s == ')' && open != '(') || (*s == ']' && open != '[') || (*s == '}' && open != '{')) return 0;
        }
    }
    return sp == 0;
}
