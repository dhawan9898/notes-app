#include <stdio.h>
#include <string.h>

#define MAXF 8

/* Count page faults (cache misses) for a reference string under FIFO, LRU and Belady's optimal (OPT) replacement. */
static int simulate(const int *refs, int n, int frames, int policy) {       /* 0 = FIFO, 1 = LRU, 2 = OPT */
    int page[MAXF], stamp[MAXF], used = 0, faults = 0, clock = 0, next_victim = 0;
    for (int i = 0; i < n; i++) {
        int hit = -1;
        for (int f = 0; f < used; f++) if (page[f] == refs[i]) hit = f;
        clock++;
        if (hit >= 0) { if (policy == 1) stamp[hit] = clock; continue; }     /* hit: LRU refreshes the timestamp */
        faults++;
        if (used < frames) { page[used] = refs[i]; stamp[used] = clock; used++; continue; }
        int victim = 0;
        if (policy == 0) { victim = next_victim; next_victim = (next_victim + 1) % frames; }              /* oldest loaded */
        else if (policy == 1) { for (int f = 1; f < frames; f++) if (stamp[f] < stamp[victim]) victim = f; }   /* least recently used */
        else {                                                                                            /* used farthest in the future */
            int far = -1;
            for (int f = 0; f < frames; f++) {
                int when = n;                                                                             /* never used again */
                for (int j = i + 1; j < n; j++) if (refs[j] == page[f]) { when = j; break; }
                if (when > far) { far = when; victim = f; }
            }
        }
        page[victim] = refs[i]; stamp[victim] = clock;
    }
    return faults;
}

int main(void) {
    const int refs[] = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};                   /* the classic sequence that shows Belady's anomaly */
    int n = (int)(sizeof refs / sizeof refs[0]);
    printf("reference string: 1 2 3 4 1 2 5 1 2 3 4 5 (%d references)\n", n);
    printf("%-8s %6s %6s %6s\n", "frames", "FIFO", "LRU", "OPT");
    for (int frames = 3; frames <= 5; frames++)
        printf("%-8d %6d %6d %6d\n", frames, simulate(refs, n, frames, 0), simulate(refs, n, frames, 1), simulate(refs, n, frames, 2));
    return 0;
}
