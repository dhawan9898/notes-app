#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Binary buddy allocator over a 16 MiB region, block sizes 16 B .. 16 MiB (orders 4..24).  A block of order k at offset o has its buddy at o ^ (1 << k):
 * allocation splits a larger block in halves; free() merges a block with its buddy whenever the buddy is free and the same size, repeating upward.
 * This is the scheme behind the Linux page allocator (orders are page counts: order 0 = 1 page ... order 10 = 4 MiB with 4 KiB pages). */
#define MINO 4
#define MAXO 24
#define NORD (MAXO + 1)
static uint8_t *region; static uint32_t freehead[NORD];       /* offset+1 of the first free block of each order (0 = empty) */
static uint8_t *state;                                         /* per 16-byte unit: 0 = interior of a block, 1 + order = FREE head, 0x80 | order = ALLOCATED head */
#define UNIT(o) ((o) >> MINO)
#define LNEXT(o) (*(uint32_t *)(region + (o) + 4))
#define LPREV(o) (*(uint32_t *)(region + (o) + 8))
static void list_add(uint32_t o, int k) { LNEXT(o) = freehead[k]; LPREV(o) = 0; if (freehead[k]) LPREV(freehead[k] - 1) = o + 1; freehead[k] = o + 1; state[UNIT(o)] = (uint8_t)(1 + k); }
static void list_del(uint32_t o, int k) { uint32_t n = LNEXT(o), p = LPREV(o); if (p) LNEXT(p - 1) = n; else freehead[k] = n; if (n) LPREV(n - 1) = p; state[UNIT(o)] = 0; }
static unsigned long long unsplit_events;

static void *balloc(size_t n, int *order_out) {
    int k = MINO; while (((size_t)1 << k) < n) k++;
    if (k > MAXO) return NULL;
    int j = k; while (j <= MAXO && !freehead[j]) j++;
    if (j > MAXO) return NULL;                                   /* no block big enough */
    uint32_t o = freehead[j] - 1; list_del(o, j);
    while (j > k) { j--; list_add(o + (1u << j), j); }           /* split: keep the lower half, free the upper half (its buddy) */
    state[UNIT(o)] = (uint8_t)(0x80 | k); if (order_out) *order_out = k;
    return region + o;
}
static void bfree(void *p) {
    uint32_t o = (uint32_t)((uint8_t *)p - region); int k = state[UNIT(o)] & 0x7F; assert(state[UNIT(o)] & 0x80);
    while (k < MAXO) {
        uint32_t b = o ^ (1u << k);
        if (state[UNIT(b)] != 1 + k) break;                      /* buddy is not a free block of exactly this order */
        list_del(b, k); if (b < o) o = b; k++; unsplit_events++;
    }
    list_add(o, k);
}

static uint64_t s = 88172645463325252ULL;
static uint64_t rnd(void) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
typedef struct { uint8_t *p; size_t n; int order; uint8_t tag; } Live;

int main(void) {
    region = aligned_alloc(16, (size_t)1 << MAXO); state = calloc((size_t)1 << (MAXO - MINO), 1);
    list_add(0, MAXO);
    size_t cap = 100000, nlive = 0; Live *live = malloc(cap * sizeof *live); unsigned long long asked = 0, granted = 0; size_t live_req = 0, live_blk = 0, peak_blk = 0;
    for (int op = 0; op < 2000000; op++) {
        if (nlive == 0 || (nlive < cap && rnd() % 100 < 50 + (live_blk < (8u << 20) ? 5 : -5))) {
            size_t n = 16 + rnd() % (rnd() % 4 == 0 ? 20000 : 600); int k; uint8_t *p = balloc(n, &k);
            if (p) { uint8_t tag = (uint8_t)rnd(); memset(p, tag, n); live[nlive++] = (Live){ p, n, k, tag }; asked += n; granted += (size_t)1 << k; live_req += n; live_blk += (size_t)1 << k; if (live_blk > peak_blk) peak_blk = live_blk; }
        } else {
            size_t i = rnd() % nlive; Live l = live[i];
            for (size_t j = 0; j < l.n; j += 37) assert(l.p[j] == l.tag);          /* neighbours never wrote into our block */
            bfree(l.p); live_req -= l.n; live_blk -= (size_t)1 << l.order; live[i] = live[--nlive];
        }
    }
    printf("random allocate/free workload, 2,000,000 operations, sizes 16 B .. 20 KB\n");
    printf("internal fragmentation: requested %.1f MB, granted %.1f MB (blocks rounded up to a power of two) -> %.1f%% of granted space is wasted inside blocks\n",
           asked / 1048576.0, granted / 1048576.0, 100.0 * (1.0 - (double)asked / (double)granted));
    printf("buddy merges performed: %llu; peak live blocks %.1f MB\n", unsplit_events, peak_blk / 1048576.0);
    for (size_t i = 0; i < nlive; i++) bfree(live[i].p);
    int blocks = 0; for (int k = 0; k <= MAXO; k++) for (uint32_t o = freehead[k]; o; o = LNEXT(o - 1)) blocks++;
    printf("after freeing everything: %d free block(s), largest order %d -> %s\n", blocks, freehead[MAXO] ? MAXO : -1, blocks == 1 && freehead[MAXO] ? "fully coalesced into the original 16 MiB block" : "NOT fully coalesced");
    assert(blocks == 1 && freehead[MAXO]);
    free(live); free(region); free(state); return 0;
}
