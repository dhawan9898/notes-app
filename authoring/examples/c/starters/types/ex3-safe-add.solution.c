bool safe_add(int a, int b, int *out) {
    if (b > 0 && a > INT_MAX - b) return false;     /* would exceed INT_MAX */
    if (b < 0 && a < INT_MIN - b) return false;     /* would go below INT_MIN */
    *out = a + b;
    return true;
}
