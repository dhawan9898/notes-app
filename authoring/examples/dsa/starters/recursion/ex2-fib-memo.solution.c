long long fib_memo(int n) {
    calls++;
    if (n < 2) return n;
    if (memo[n]) return memo[n];                    /* already computed: reuse it */
    return memo[n] = fib_memo(n - 1) + fib_memo(n - 2);
}
