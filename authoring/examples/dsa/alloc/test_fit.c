#define _GNU_SOURCE
#include <stdio.h>
#include <math.h>
#include "fit.h"
#include "../../c/perf/timing.h"

static uint64_t s = 88172645463325252ULL;
static uint64_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
typedef struct { uint8_t *p; size_t n; uint8_t tag; } Live;
static size_t pick_size(void) { double u = (double)(rnd() >> 11) / 9007199254740992.0; return (size_t)exp(log(16.0) + u * (log(16384.0) - log(16.0))); }   /* log-uniform 16 B .. 16 KB */

int main(void) {
    static const char *names[] = { "first fit", "next fit", "best fit" };
    size_t HEAP = 64u << 20, TARGET = 40u << 20; int OPS = 3000000;
    printf("%-10s | %10s %12s | %10s %12s %12s | %s\n", "policy", "steps/alloc", "failed allocs", "free blocks", "free bytes", "largest free", "peak utilisation");
    for (int pol = 0; pol < 3; pol++) {
        s = 88172645463325252ULL; Heap h; heap_init(&h, HEAP, pol); size_t cap = 400000, nlive = 0; Live *live = malloc(cap * sizeof *live); size_t requested_live = 0, peak_req = 0;
        for (int op = 0; op < OPS; op++) {
            int do_alloc = nlive == 0 || (requested_live < TARGET ? rnd() % 100 < 55 : rnd() % 100 < 45);
            if (do_alloc && nlive < cap) {
                size_t n = pick_size(); uint8_t *p = h_malloc(&h, n);
                if (p) { uint8_t tag = (uint8_t)rnd(); memset(p, tag, n); live[nlive++] = (Live){ p, n, tag }; requested_live += n; if (requested_live > peak_req) peak_req = requested_live; }
            } else {
                size_t i = rnd() % nlive; Live l = live[i];
                for (size_t k = 0; k < l.n; k += 61) if (l.p[k] != l.tag) { printf("CORRUPTION in %s\n", names[pol]); return 1; }      /* nobody overwrote our payload */
                h_free(&h, l.p); requested_live -= l.n; live[i] = live[--nlive];
            }
            if (op % 500000 == 0) heap_check(&h, NULL, NULL);
        }
        size_t largest, nfree; size_t fb = heap_check(&h, &largest, &nfree);
        printf("%-10s | %10.1f %12llu | %10zu %9.1f MB %9.2f MB | %.1f%% of heap holds live data at peak\n", names[pol], (double)h.steps / (double)h.allocs, h.fails, nfree, fb / 1048576.0, largest / 1048576.0, 100.0 * (double)peak_req / (double)HEAP);
        for (size_t i = 0; i < nlive; i++) h_free(&h, live[i].p);
        size_t l2, n2; size_t fb2 = heap_check(&h, &l2, &n2);
        if (n2 != 1) { printf("after freeing everything there should be ONE free block, found %zu\n", n2); return 1; }
        (void)fb2; free(live); heap_destroy(&h);
    }
    printf("\nafter freeing every allocation each heap coalesced back into a single free block (verified by the checker)\n");
    return 0;
}
