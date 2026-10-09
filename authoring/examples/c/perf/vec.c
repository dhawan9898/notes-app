#include <stddef.h>
float sum_float(const float *a, size_t n) { float s = 0; for (size_t i = 0; i < n; i++) s += a[i]; return s; }
int   sum_int(const int *a, size_t n)     { int s = 0;   for (size_t i = 0; i < n; i++) s += a[i]; return s; }
void  saxpy(float *restrict y, const float *restrict x, float a, size_t n) { for (size_t i = 0; i < n; i++) y[i] += a * x[i]; }
