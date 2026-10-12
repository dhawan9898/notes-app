#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../c/perf/timing.h"

/* Sudoku as constraint satisfaction: bitmasks of used digits per row/column/box; choose the empty cell with the FEWEST candidates (MRV heuristic). */
static int grid[81], row_m[9], col_m[9], box_m[9];
static unsigned long long nodes;
static int use_mrv;
#define BOX(i) (((i) / 27) * 3 + ((i) % 9) / 3)

static void place(int i, int d) { int bit = 1 << d; row_m[i / 9] |= bit; col_m[i % 9] |= bit; box_m[BOX(i)] |= bit; grid[i] = d; }
static void unplace(int i, int d) { int bit = ~(1 << d); row_m[i / 9] &= bit; col_m[i % 9] &= bit; box_m[BOX(i)] &= bit; grid[i] = 0; }
static int cands(int i) { return 0x3FE & ~(row_m[i / 9] | col_m[i % 9] | box_m[BOX(i)]); }          /* bits 1..9 */

static int solve(int empties) {
    nodes++;
    if (!empties) return 1;
    int best = -1, bestc = 10;
    for (int i = 0; i < 81; i++) if (!grid[i]) {
        int c = __builtin_popcount(cands(i));
        if (!use_mrv) { best = i; bestc = c; break; }                       /* naive: first empty cell in reading order */
        if (c < bestc) { best = i; bestc = c; if (c <= 1) break; }
    }
    if (bestc == 0) return 0;                                                /* dead end: some cell has no candidate */
    for (int m = cands(best); m; m &= m - 1) {
        int d = __builtin_ctz(m); place(best, d);
        if (solve(empties - 1)) return 1;
        unplace(best, d);
    }
    return 0;
}
static int load(const char *s) {
    memset(grid, 0, sizeof grid); memset(row_m, 0, sizeof row_m); memset(col_m, 0, sizeof col_m); memset(box_m, 0, sizeof box_m); int empties = 0;
    for (int i = 0; i < 81; i++) { if (s[i] >= '1' && s[i] <= '9') place(i, s[i] - '0'); else empties++; }
    return empties;
}
static int valid_solution(void) {
    for (int u = 0; u < 9; u++) { int r = 0, c = 0, b = 0; for (int k = 0; k < 9; k++) { r |= 1 << grid[u * 9 + k]; c |= 1 << grid[k * 9 + u]; b |= 1 << grid[(u / 3) * 27 + (u % 3) * 3 + (k / 3) * 9 + k % 3]; } if (r != 0x3FE || c != 0x3FE || b != 0x3FE) return 0; }
    return 1;
}

int main(void) {
    static const struct { const char *name, *puzzle; } tests[] = {
        { "easy",                    "53..7....6..195....98....6.8...6...34..8.3..17...2...6.6....28....419..5....8..79" },
        { "hard (Inkala, 2012)",     "8..........36......7..9.2...5...7.......457.....1...3...1....68..85...1..9....4.." },
        { "17 clues (sparse)",      "...............3.85..1.2.......5.7.....4...1...9.......5......73..2.1........4...9" },
    };
    printf("%-22s %-16s %14s %10s %s\n", "puzzle", "cell choice", "search nodes", "ms", "result");
    for (int t = 0; t < 3; t++) for (int mrv = 0; mrv < 2; mrv++) {
        int e = load(tests[t].puzzle); use_mrv = mrv; nodes = 0; double t0 = now(); int ok = solve(e); double dt = now() - t0;
        printf("%-22s %-16s %14llu %10.2f %s\n", tests[t].name, mrv ? "fewest candidates" : "reading order", nodes, dt * 1e3, ok && valid_solution() ? "solved, verified" : "FAILED");
    }
    load(tests[1].puzzle); use_mrv = 1; nodes = 0; solve(81 - 21);
    printf("\nsolution of the hard puzzle:\n"); for (int r = 0; r < 9; r++) { for (int c = 0; c < 9; c++) printf("%d%s", grid[r * 9 + c], c % 3 == 2 && c < 8 ? " | " : " "); printf("\n"); if (r % 3 == 2 && r < 8) printf("------+-------+------\n"); }
    return 0;
}
