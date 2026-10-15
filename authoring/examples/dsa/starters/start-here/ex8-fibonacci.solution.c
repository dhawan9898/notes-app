long long fib_rec(int n) {
    return n < 2 ? n : fib_rec(n - 1) + fib_rec(n - 2);
}

long long fib_iter(int n) {
    long long a = 0, b = 1;
    for (int i = 0; i < n; i++) {
        long long t = a + b;
        a = b;
        b = t;
    }
    return a;
}
