/* Simulate one policy: pick(frame contents, time) chooses the victim when the cache is full. */
enum { FIFO, LRU, OPT };

static int simulate(const int *refs, int n, int frames, int policy) {
    int page[16], loaded[16], used[16], size = 0, misses = 0;
    for (int t = 0; t < n; t++) {
        int hit = -1;
        for (int i = 0; i < size; i++) if (page[i] == refs[t]) hit = i;
        if (hit >= 0) { used[hit] = t; continue; }
        misses++;
        int slot = size;
        if (size == frames) {
            slot = 0;
            for (int i = 1; i < size; i++) {
                if (policy == FIFO && loaded[i] < loaded[slot]) slot = i;
                if (policy == LRU && used[i] < used[slot]) slot = i;
                if (policy == OPT) {                 /* evict the page needed furthest in the future (or never) */
                    int ni = n, ns = n;
                    for (int u = t + 1; u < n; u++) if (refs[u] == page[i]) { ni = u; break; }
                    for (int u = t + 1; u < n; u++) if (refs[u] == page[slot]) { ns = u; break; }
                    if (ni > ns) slot = i;
                }
            }
        } else size++;
        page[slot] = refs[t];
        loaded[slot] = used[slot] = t;
    }
    return misses;
}

int misses_fifo(const int *refs, int n, int frames) { return simulate(refs, n, frames, FIFO); }
int misses_lru(const int *refs, int n, int frames)  { return simulate(refs, n, frames, LRU); }
int misses_opt(const int *refs, int n, int frames)  { return simulate(refs, n, frames, OPT); }
