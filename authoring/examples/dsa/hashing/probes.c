// Measure average probes per lookup at fixed load factors and compare with Knuth's linear-probing formulas.
#include "oamap.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    const size_t cap = 1 << 20;
    printf("%6s %12s %12s %12s %12s\n", "load", "hit(meas)", "hit(Knuth)", "miss(meas)", "miss(Knuth)");
    for (double a = 0.30; a < 0.96; a += 0.15) {
        map_t m; map_init(&m, cap);
        size_t n = (size_t)(a * cap);
        for (size_t i = 0; i < n; i++) map_put_nogrow(&m, i * 2654435761u + 1, i);      // keys 1, ...
        m.probes = 0;
        for (size_t i = 0; i < n; i++) map_get(&m, i * 2654435761u + 1, NULL);
        double hit = (double)m.probes / n;
        m.probes = 0;
        for (size_t i = 0; i < n; i++) map_get(&m, (i + (1ull << 40)) * 2654435761u + 1, NULL);   // absent keys
        double miss = (double)m.probes / n;
        printf("%6.2f %12.2f %12.2f %12.2f %12.2f\n", a, hit, 0.5 * (1 + 1 / (1 - a)), miss, 0.5 * (1 + 1 / ((1 - a) * (1 - a))));
        map_free(&m);
    }
    return 0;
}
