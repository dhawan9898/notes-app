// Polling a status register: why 'volatile' is mandatory for memory-mapped I/O.
#include <stdint.h>

#define READY 0x1u

// WRONG: the compiler may load *status once, since nothing in the loop changes it
uint32_t wait_plain(uint32_t *status) {
    while (!(*status & READY)) { }
    return *status;
}

// RIGHT: every source-level access is performed, in order
uint32_t wait_volatile(volatile uint32_t *status) {
    while (!(*status & READY)) { }
    return *status;
}

// Register write ordering: the two stores must reach the device in this order (index then data)
void write_pair(volatile uint32_t *index, volatile uint32_t *data, uint32_t i, uint32_t d) {
    *index = i;
    *data = d;
}
