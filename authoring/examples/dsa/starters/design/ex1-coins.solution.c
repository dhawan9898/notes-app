int min_coins(const int *coins, int nc, int target) {
    int best[1024];                                  /* best[t] = fewest coins for t, or -1 */
    best[0] = 0;
    for (int t = 1; t <= target; t++) {
        best[t] = -1;
        for (int i = 0; i < nc; i++)
            if (coins[i] <= t && best[t - coins[i]] >= 0 && (best[t] < 0 || best[t - coins[i]] + 1 < best[t]))
                best[t] = best[t - coins[i]] + 1;
    }
    return best[target];
}

long long count_ways(const int *coins, int nc, int target) {
    long long ways[1024] = {1};                      /* ways[0] = 1: the empty choice */
    for (int i = 0; i < nc; i++)                     /* coins in the outer loop: each multiset counted once */
        for (int t = coins[i]; t <= target; t++) ways[t] += ways[t - coins[i]];
    return ways[target];
}
