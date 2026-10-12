#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A tiny dependency resolver, the core of make, systemd's job ordering, package managers and build systems.
 * Input lines:  "target: dep1 dep2 ..."   (target needs deps first).  Output: a valid build order, or the cycle that blocks one. */
#define MAXN 64
static char name[MAXN][32]; static int nn;
static int adj[MAXN][MAXN], deg[MAXN];                  /* edge dep -> target: "dep must come before target" */

static int id(const char *s) { for (int i = 0; i < nn; i++) if (!strcmp(name[i], s)) return i; strcpy(name[nn], s); return nn++; }

static int colour[MAXN], parent[MAXN];
/* DFS over edges dep -> target; on a back edge u -> v (v is grey) walk parent[] from u back to v to print the cycle */
static int find_cycle(int u) {
    colour[u] = 1;
    for (int k = 0; k < deg[u]; k++) {
        int v = adj[u][k];
        if (colour[v] == 1) {
            printf("cycle: %s", name[v]);
            int path[MAXN], len = 0; for (int x = u; x != v; x = parent[x]) path[len++] = x;
            while (len) printf(" -> %s", name[path[--len]]);
            printf(" -> %s\n", name[v]); return 1;
        }
        if (colour[v] == 0) { parent[v] = u; if (find_cycle(v)) return 1; }
    }
    colour[u] = 2; return 0;
}

static void resolve(const char *title, const char *spec[], int lines) {
    nn = 0; memset(deg, 0, sizeof deg); memset(colour, 0, sizeof colour);
    for (int i = 0; i < lines; i++) {
        char buf[128]; strcpy(buf, spec[i]); char *colon = strchr(buf, ':'); *colon = 0;
        int t = id(buf);
        for (char *tok = strtok(colon + 1, " "); tok; tok = strtok(NULL, " ")) { int d = id(tok); adj[d][deg[d]++] = t; }
    }
    printf("== %s ==\n", title);
    int indeg[MAXN] = {0}, order[MAXN], h = 0, t = 0;
    for (int u = 0; u < nn; u++) for (int k = 0; k < deg[u]; k++) indeg[adj[u][k]]++;
    for (int u = 0; u < nn; u++) if (!indeg[u]) order[t++] = u;
    while (h < t) { int u = order[h++]; for (int k = 0; k < deg[u]; k++) if (--indeg[adj[u][k]] == 0) order[t++] = adj[u][k]; }
    if (t == nn) { printf("order:"); for (int i = 0; i < nn; i++) printf(" %s", name[order[i]]); printf("\n"); }
    else { printf("no valid order (%d of %d units can be ordered); ", t, nn); for (int u = 0; u < nn; u++) if (!colour[u] && find_cycle(u)) break; }
    printf("\n");
}

int main(void) {
    const char *ok[] = { "app: libnet libui", "libui: libgfx libfont", "libnet: libc", "libgfx: libc", "libfont: libc", "libc:" };
    resolve("acyclic build graph", ok, 6);
    const char *bad[] = { "network-online: network", "network: dbus", "dbus: network-online", "app: network-online" };
    resolve("with a dependency cycle", bad, 4);
    return 0;
}
