#ifndef MATHX_H
#define MATHX_H
#define MATHX_API __attribute__((visibility("default")))
MATHX_API int mathx_square(int x);
MATHX_API int mathx_cube(int x);
MATHX_API const char *mathx_version(void);
#endif
