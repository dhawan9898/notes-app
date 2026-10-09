#include <stdio.h>
#include <stdlib.h>
#include "timing.h"

#define N 4000000
struct particle_aos { float x, y, z, vx, vy, vz; int id; char name[20]; };      /* 48 bytes; we only need x */
struct particles_soa { float *x, *y, *z, *vx, *vy, *vz; };

int main(void) {
    struct particle_aos *aos = malloc(N * sizeof *aos);
    struct particles_soa soa = { malloc(N * 4), malloc(N * 4), malloc(N * 4), malloc(N * 4), malloc(N * 4), malloc(N * 4) };
    for (int i = 0; i < N; i++) { aos[i].x = (float)i; soa.x[i] = (float)i; }
    float s1 = 0, s2 = 0;
    double t0 = now();
    for (int rep = 0; rep < 20; rep++) for (int i = 0; i < N; i++) s1 += aos[i].x;           /* touches one float per 48-byte record */
    double t1 = now();
    for (int rep = 0; rep < 20; rep++) for (int i = 0; i < N; i++) s2 += soa.x[i];           /* contiguous floats */
    double t2 = now();
    DO_NOT_OPTIMIZE(s1); DO_NOT_OPTIMIZE(s2);
    printf("array of structs : %7.1f ms (%zu bytes/record, 4 of them used)\nstruct of arrays : %7.1f ms  (%.1fx faster)\n", (t1 - t0) * 1e3, sizeof(struct particle_aos), (t2 - t1) * 1e3, (t1 - t0) / (t2 - t1));
    return 0;
}
