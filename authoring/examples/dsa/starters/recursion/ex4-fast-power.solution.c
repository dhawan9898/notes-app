long long power_fast(long long b, int e) {
    if (e == 0) return 1;
    long long half = power_fast(b, e / 2);          /* b^e = (b^(e/2))^2, times b once more if e is odd */
    long long sq = mul(half, half);
    return e % 2 ? mul(sq, b) : sq;
}
