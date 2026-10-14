#include <stdio.h>
#include <limits.h>

#define INF INT_MAX

/* Minimum number of coins for each amount 0..target by dynamic programming (bottom-up). */
static int min_coins(const int *coins, int n, int target) {
    int best[64];
    best[0] = 0;
    for (int a = 1; a <= target; a++) {
        best[a] = INF;
        for (int i = 0; i < n; i++)
            if (coins[i] <= a && best[a - coins[i]] != INF && best[a - coins[i]] + 1 < best[a]) best[a] = best[a - coins[i]] + 1;
    }
    return best[target] == INF ? -1 : best[target];
}

static int greedy_coins(const int *coins_desc, int n, int target) {      /* take the biggest coin that fits, repeatedly */
    int count = 0;
    for (int i = 0; i < n; i++) while (target >= coins_desc[i]) { target -= coins_desc[i]; count++; }
    return target == 0 ? count : -1;
}

/* Number of ORDER-INDEPENDENT ways to make the amount (each coin type considered once, in an outer loop). */
static long ways(const int *coins, int n, int target) {
    long w[64] = {1};
    for (int i = 0; i < n; i++)
        for (int a = coins[i]; a <= target; a++) w[a] += w[a - coins[i]];
    return w[target];
}

int main(void) {
    const int us[] = {25, 10, 5, 1}, odd[] = {4, 3, 1};
    printf("coins 25,10,5,1 target 63: greedy %d, DP %d\n", greedy_coins(us, 4, 63), min_coins(us, 4, 63));
    printf("coins 4,3,1     target  6: greedy %d, DP %d   (greedy takes 4+1+1, DP finds 3+3)\n", greedy_coins(odd, 3, 6), min_coins(odd, 3, 6));
    printf("ways to make 10 from 1,5,10: %ld\n", ways((const int[]){1, 5, 10}, 3, 10));
    return 0;
}
