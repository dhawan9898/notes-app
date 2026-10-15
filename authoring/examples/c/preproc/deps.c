// Includes config.h twice (directly and through util.h); the guard makes the second inclusion empty.
#include "util.h"
#include "config.h"
size_t demo_bufsize(void) { return DEMO_BUFSIZE; }
