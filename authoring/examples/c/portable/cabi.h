#ifndef CABI_H
#define CABI_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {                                   /* disable C++ name mangling for these declarations */
#endif

typedef struct { int32_t x; int32_t y; double weight; } cpoint;      /* plain data: layout is predictable */

int      cabi_add(int a, int b);
double   cabi_dot(const double *a, const double *b, size_t n);
void     cabi_fill(cpoint *out, size_t n);                           /* caller allocates, library fills */
const char *cabi_version(void);

#ifdef __cplusplus
}
#endif
#endif
