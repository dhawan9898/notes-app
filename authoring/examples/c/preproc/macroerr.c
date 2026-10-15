// How compilers report an error that is inside a macro expansion.
#define SQUARE(x) ((x) * (x))
#define AREA(r) (3 * SQUARE(r))

struct circle { int radius; };

int area_of(struct circle c) {
    return AREA(c);             /* bug: should be AREA(c.radius) */
}
