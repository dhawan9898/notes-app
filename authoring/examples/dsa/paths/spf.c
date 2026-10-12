#include <stdio.h>
#include <string.h>

/* The SPF calculation of a link-state protocol (OSPF/IS-IS) on a small topology: Dijkstra from the local router, tracking for each
 * destination the SET of first-hop neighbours on ALL equal-cost shortest paths (ECMP).  O(V^2) array version, as in RFC 2328 Sec. 16.1. */
#define N 7
static const char *name[N] = { "R1", "R2", "R3", "R4", "R5", "R6", "R7" };
static const int cost[N][N] = {                       /* symmetric link costs, 0 = no link */
    /*R1*/ { 0, 10, 10,  0,  0,  0,  0 },
    /*R2*/ {10,  0,  0, 10,  0,  0,  0 },
    /*R3*/ {10,  0,  0, 10,  0,  0,  0 },
    /*R4*/ { 0, 10, 10,  0, 10,  0,  0 },
    /*R5*/ { 0,  0,  0, 10,  0, 20, 10 },
    /*R6*/ { 0,  0,  0,  0, 20,  0, 10 },
    /*R7*/ { 0,  0,  0,  0, 10, 10,  0 } };

int main(void) {
    int dist[N], done[N] = {0}; unsigned nexthops[N] = {0};            /* bitmask of first-hop neighbours of R1 */
    for (int i = 0; i < N; i++) dist[i] = 1 << 30;
    dist[0] = 0;
    for (int iter = 0; iter < N; iter++) {
        int u = -1; for (int i = 0; i < N; i++) if (!done[i] && (u < 0 || dist[i] < dist[u])) u = i;
        if (u < 0 || dist[u] >= 1 << 30) break;
        done[u] = 1;
        for (int v = 0; v < N; v++) {
            if (!cost[u][v] || done[v]) continue;
            int nd = dist[u] + cost[u][v];
            unsigned via = u == 0 ? 1u << v : nexthops[u];             /* a neighbour of the root is its own first hop; others inherit */
            if (nd < dist[v]) { dist[v] = nd; nexthops[v] = via; }
            else if (nd == dist[v]) nexthops[v] |= via;                /* equal-cost path: ECMP, union the first hops */
        }
    }
    printf("SPF tree rooted at R1\n%-6s %-6s %s\n", "dest", "cost", "next hop(s)");
    for (int i = 1; i < N; i++) {
        printf("%-6s %-6d", name[i], dist[i]);
        for (int v = 0; v < N; v++) if (nexthops[i] >> v & 1) printf(" %s", name[v]);
        printf("\n");
    }
    return 0;
}
