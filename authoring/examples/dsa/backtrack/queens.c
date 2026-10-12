#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../c/perf/timing.h"

/* ===== N-Queens #1: array of column positions, O(n) safety test per placement ===== */
static unsigned long long nodes_a;
static int safe(const int *q, int row, int col) { for (int r = 0; r < row; r++) if (q[r] == col || abs(q[r] - col) == row - r) return 0; return 1; }
static long queens_array(int *q, int row, int n) {
    nodes_a++;
    if (row == n) return 1;
    long c = 0; for (int col = 0; col < n; col++) if (safe(q, row, col)) { q[row] = col; c += queens_array(q, row + 1, n); }
    return c;
}
/* ===== N-Queens #2: three bitmasks (columns, / diagonals, \ diagonals); the set of safe squares in a row is ONE expression ===== */
static unsigned long long nodes_b;
static long queens_bits(unsigned cols, unsigned d1, unsigned d2, unsigned full) {
    nodes_b++;
    if (cols == full) return 1;
    long c = 0; unsigned avail = full & ~(cols | d1 | d2);
    while (avail) { unsigned bit = avail & -avail; avail ^= bit; c += queens_bits(cols | bit, ((d1 | bit) << 1) & full, (d2 | bit) >> 1, full); }
    return c;
}

/* ===== subset-sum with and without pruning: count subsets of a[] (positive ints) summing to target ===== */
static unsigned long long nodes_plain, nodes_pruned;
static long ss_plain(const int *a, int n, int i, int sum, int target) { nodes_plain++; if (i == n) return sum == target; return ss_plain(a, n, i + 1, sum, target) + ss_plain(a, n, i + 1, sum + a[i], target); }
static long ss_pruned(const int *a, const long *suffix, int n, int i, int sum, int target) {
    nodes_pruned++;
    if (sum == target) return 1;                                              /* items are positive, so the only way to extend a hit is to exclude everything left */
    if (i == n || sum > target || sum + suffix[i] < target) return 0;         /* overshoot, or even taking everything left cannot reach the target */
    return ss_pruned(a, suffix, n, i + 1, sum, target) + ss_pruned(a, suffix, n, i + 1, sum + a[i], target);
}

/* ===== enumerations ===== */
static void next_perm_demo(void) {                                            /* lexicographic next permutation: the standard in-place algorithm */
    int p[4] = { 1, 2, 3, 4 }; int count = 0;
    for (;;) {
        count++;
        int i = 2; while (i >= 0 && p[i] >= p[i + 1]) i--;
        if (i < 0) break;
        int j = 3; while (p[j] <= p[i]) j--;
        int t = p[i]; p[i] = p[j]; p[j] = t;
        for (int a = i + 1, b = 3; a < b; a++, b--) { t = p[a]; p[a] = p[b]; p[b] = t; }
    }
    printf("next_permutation visited %d permutations of 4 elements (4! = 24)\n", count);
}
static void gosper_demo(void) {                                               /* all 3-element subsets of {0..4} as bitmasks, in increasing order, via Gosper's hack */
    unsigned x = 0x7, limit = 1u << 5; printf("Gosper's hack, 3-subsets of 5 elements:");
    int count = 0; while (x < limit) { printf(" %02x", x); count++; unsigned c = x & -x, r = x + c; x = (((r ^ x) >> 2) / c) | r; }
    printf("  (%d = C(5,3))\n", count);
}

int main(void) {
    printf("%3s %12s | %14s %9s | %14s %9s | %6s\n", "n", "solutions", "array: nodes", "ms", "bitmask: nodes", "ms", "speedup");
    for (int n = 4; n <= 13; n++) {
        int q[16]; nodes_a = 0; double t0 = now(); long a = queens_array(q, 0, n); double ta = now() - t0;
        nodes_b = 0; t0 = now(); long b = queens_bits(0, 0, 0, (1u << n) - 1); double tb = now() - t0;
        assert(a == b);
        printf("%3d %12ld | %14llu %9.1f | %14llu %9.1f | %5.0fx\n", n, a, nodes_a, ta * 1e3, nodes_b, tb * 1e3, tb > 0 ? ta / tb : 0.0);
    }
    printf("(known solution counts: 4->2, 5->10, 6->4, 7->40, 8->92, 9->352, 10->724, 11->2680, 12->14200, 13->73712)\n\n");

    /* pruning: 28 positive numbers, target near the middle */
    int a[28]; uint64_t s = 88172645463325252ULL; for (int i = 0; i < 28; i++) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; a[i] = 1 + (int)((s >> 11) % 100); }
    /* sort descending: large items first make the bounds bite sooner */
    for (int i = 0; i < 28; i++) for (int j = i + 1; j < 28; j++) if (a[j] > a[i]) { int t = a[i]; a[i] = a[j]; a[j] = t; }
    long suffix[29]; suffix[28] = 0; for (int i = 27; i >= 0; i--) suffix[i] = suffix[i + 1] + a[i];
    int target = (int)(suffix[0] / 2);
    nodes_plain = 0; long p1 = ss_plain(a, 28, 0, 0, target);
    nodes_pruned = 0; long p2 = ss_pruned(a, suffix, 28, 0, 0, target);
    assert(p1 == p2);
    printf("subset sum, 28 positive numbers, target = half the total (%d):\n", target);
    printf("  no pruning : %ld subsets found, %llu search nodes\n", p1, nodes_plain);
    printf("  with bounds: %ld subsets found, %llu search nodes  (%.0fx fewer)\n\n", p2, nodes_pruned, (double)nodes_plain / (double)nodes_pruned);
    next_perm_demo(); gosper_demo();
    return 0;
}
