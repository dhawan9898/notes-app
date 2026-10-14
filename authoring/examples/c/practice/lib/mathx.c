#include "mathx.h"

static int helper_mul(int a, int b) { return a * b; }        /* internal: not exported */
int mx_square(int x) { return helper_mul(x, x); }
int mx_cube(int x)   { return helper_mul(x, mx_square(x)); }
