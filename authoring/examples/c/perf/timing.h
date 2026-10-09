#ifndef TIMING_H
#define TIMING_H
#include <time.h>
static inline double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
/* Prevent the optimizer from deleting a computed value or hoisting work out of a loop. */
#define DO_NOT_OPTIMIZE(v) __asm__ volatile("" : : "r,m"(v) : "memory")
#endif
