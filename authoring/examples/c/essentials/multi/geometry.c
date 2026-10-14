#include "geometry.h"      /* quotes: a header from this project; <...> is for system headers */
#include <math.h>

double vec_length(Vec v) {                /* the definition: the actual code */
    return sqrt(v.x * v.x + v.y * v.y);
}

Vec vec_add(Vec a, Vec b) {
    Vec r = {a.x + b.x, a.y + b.y};
    return r;
}
