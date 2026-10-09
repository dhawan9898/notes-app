#include "cabi.h"
int cabi_add(int a, int b) { return a + b; }
double cabi_dot(const double *a, const double *b, size_t n) { double s = 0; for (size_t i = 0; i < n; i++) s += a[i] * b[i]; return s; }
void cabi_fill(cpoint *out, size_t n) { for (size_t i = 0; i < n; i++) out[i] = (cpoint){ (int32_t)i, (int32_t)(i * i), 0.5 * (double)i }; }
const char *cabi_version(void) { return "cabi 1.0"; }
