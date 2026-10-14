#ifndef GEOMETRY_H            /* include guard: lets the header be included twice without errors */
#define GEOMETRY_H

typedef struct { double x, y; } Vec;

double vec_length(Vec v);                 /* a declaration: "this function exists, here is its shape" */
Vec    vec_add(Vec a, Vec b);

#endif
