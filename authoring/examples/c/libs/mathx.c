#include "mathx.h"
static int helper_mul(int a, int b) { return a * b; }          /* internal: not exported */
int internal_but_global(int x) { return x + 1; }               /* global symbol that is NOT marked default visibility */
int mathx_square(int x) { return helper_mul(x, x); }
int mathx_cube(int x) { return helper_mul(helper_mul(x, x), x); }
const char *mathx_version(void) { return "mathx 1.2"; }
