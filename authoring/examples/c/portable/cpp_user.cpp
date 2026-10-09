#include <cstdio>
#include "cabi.h"
int main() { std::printf("C++ calls C: %d, %s\n", cabi_add(40, 2), cabi_version()); }
