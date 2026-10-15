#include "point.h"
#include "point.h"     /* second copy of struct point: a redefinition error */
int main(void) { struct point p = {1, 2}; return p.x; }
