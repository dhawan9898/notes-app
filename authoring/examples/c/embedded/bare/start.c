// A freestanding program: no libc, no C runtime, our own entry point and linker script (x86-64 Linux).
// Real firmware does the same jobs: set up the stack, copy initialised data, zero .bss, call main.
#include <stdint.h>

extern char _stext[], _etext[], _sdata[], _edata[], _sbss[], _ebss[];   // symbols defined by link.ld
int counter = 41;                 // .data: nonzero initial value
int zeroed[4];                    // .bss: must read as zero
const char banner[] = "hello from a freestanding program\n";   // .rodata

static long sys_write(int fd, const void *buf, unsigned long n) {
    long r;
    __asm__ volatile ("syscall" : "=a"(r) : "a"(1), "D"(fd), "S"(buf), "d"(n) : "rcx", "r11", "memory");
    return r;
}
__attribute__((noreturn)) static void sys_exit(int code) { for (;;) __asm__ volatile ("syscall" :: "a"(60), "D"(code) : "rcx", "r11", "memory"); }

static void put_hex(uintptr_t v) {
    char b[18]; int i = 18; b[--i] = '\n';
    do { b[--i] = "0123456789abcdef"[v & 15]; v >>= 4; } while (v);
    b[--i] = 'x'; b[--i] = '0';
    sys_write(1, b + i, 18 - i);
}

int main(void) {
    sys_write(1, banner, sizeof banner - 1);
    counter++;
    sys_write(1, "counter (.data, starts at 41, +1) = ", 36); put_hex(counter);
    sys_write(1, "zeroed[0..3] (.bss) = ", 22); put_hex(zeroed[0] | zeroed[1] | zeroed[2] | zeroed[3]);
    sys_write(1, ".text  size = ", 14); put_hex((uintptr_t)(_etext - _stext));
    sys_write(1, ".data  size = ", 14); put_hex((uintptr_t)(_edata - _sdata));
    sys_write(1, ".bss   size = ", 14); put_hex((uintptr_t)(_ebss - _sbss));
    return 0;
}

// entry point: the "reset handler". The kernel hands us a stack; on a microcontroller we would load SP from the vector table here.
__attribute__((naked, noreturn)) void _start(void) {
    __asm__ volatile ("xor %ebp, %ebp\n\tand $-16, %rsp\n\tcall start_c\n");
}
__attribute__((used, noreturn)) void start_c(void) {
    // (on a microcontroller: copy .data from flash to RAM, zero .bss, enable clocks, then call main)
    sys_exit(main());
}
