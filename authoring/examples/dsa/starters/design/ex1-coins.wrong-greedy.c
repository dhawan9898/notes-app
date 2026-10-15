int min_coins(const int *coins, int nc, int target) {
    int used = 0;                                    /* biggest coin first: fails for {4, 3, 1} */
    for (int i = 0; i < nc; i++)
        while (target >= coins[i]) { target -= coins[i]; used++; }
    return target == 0 ? used : -1;
}

long long count_ways(const int *coins, int nc, int target) {
    long long ways[1024] = {1};
    for (int i = 0; i < nc; i++)
        for (int t = coins[i]; t <= target; t++) ways[t] += ways[t - coins[i]];
    return ways[target];
}
