#include "acc.h"
#include <stdio.h>
#include <stdlib.h>

struct acc { double *v; size_t n, cap; };

acc_t *acc_new(void) { return calloc(1, sizeof(acc_t)); }
void acc_free(acc_t *a) { if (a) { free(a->v); free(a); } }

int acc_add(acc_t *a, double x) {
    if (!a) return ACC_ERR_ARG;
    if (a->n == a->cap) {
        size_t nc = a->cap ? a->cap * 2 : 8;
        double *p = realloc(a->v, nc * sizeof *p);
        if (!p) return ACC_ERR_NOMEM;
        a->v = p; a->cap = nc;
    }
    a->v[a->n++] = x;
    return ACC_OK;
}

int acc_stats_get(const acc_t *a, acc_stats *out) {
    if (!a || !out) return ACC_ERR_ARG;
    if (a->n == 0) return ACC_ERR_EMPTY;
    double lo = a->v[0], hi = a->v[0], sum = 0;
    for (size_t i = 0; i < a->n; i++) { if (a->v[i] < lo) lo = a->v[i]; if (a->v[i] > hi) hi = a->v[i]; sum += a->v[i]; }
    *out = (acc_stats){ lo, hi, sum / a->n, a->n };
    return ACC_OK;
}

int acc_foreach(const acc_t *a, void (*cb)(double, void *), void *user) {
    if (!a || !cb) return ACC_ERR_ARG;
    for (size_t i = 0; i < a->n; i++) cb(a->v[i], user);
    return ACC_OK;
}

char *acc_describe(const acc_t *a) {
    acc_stats s;
    if (acc_stats_get(a, &s) != ACC_OK) return NULL;
    char *buf = malloc(96);
    if (buf) snprintf(buf, 96, "n=%zu min=%g max=%g mean=%g", s.n, s.min, s.max, s.mean);
    return buf;
}
void acc_string_free(char *s) { free(s); }

const char *acc_strerror(int code) {
    switch (code) {
    case ACC_OK: return "ok"; case ACC_ERR_NOMEM: return "out of memory";
    case ACC_ERR_EMPTY: return "no data"; case ACC_ERR_ARG: return "invalid argument";
    default: return "unknown error";
    }
}
