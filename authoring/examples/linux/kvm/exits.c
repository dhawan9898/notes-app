// What does a VM exit cost? Run the same loop twice inside a guest: once doing an I/O port write per iteration
// (every iteration exits to this program), once doing a plain NOP (no exit at all, the CPU runs guest code natively).
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

static double run_guest(int kvm, int with_exit, uint32_t n, long *exits_out) {
    int vm = ioctl(kvm, KVM_CREATE_VM, 0);
    uint8_t *mem = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    struct kvm_userspace_memory_region r = { .slot = 0, .guest_phys_addr = 0, .memory_size = 0x1000, .userspace_addr = (uintptr_t)mem };
    ioctl(vm, KVM_SET_USER_MEMORY_REGION, &r);
    uint8_t code[] = {
        0x66, 0xb9, n & 0xff, n >> 8 & 0xff, n >> 16 & 0xff, n >> 24 & 0xff,   // mov ecx, n
        0xba, 0xf9, 0x03,                                                      // mov dx, 0x3f9
        with_exit ? 0xee : 0x90,                                               // loop: out dx, al   |   nop
        0x66, 0x49,                                                            // dec ecx
        0x75, 0xfb,                                                            // jnz loop
        0xf4,                                                                  // hlt
    };
    memcpy(mem, code, sizeof code);
    int vcpu = ioctl(vm, KVM_CREATE_VCPU, 0);
    struct kvm_run *run = mmap(NULL, ioctl(kvm, KVM_GET_VCPU_MMAP_SIZE, 0), PROT_READ | PROT_WRITE, MAP_SHARED, vcpu, 0);
    struct kvm_sregs s; ioctl(vcpu, KVM_GET_SREGS, &s); s.cs.base = 0; s.cs.selector = 0; ioctl(vcpu, KVM_SET_SREGS, &s);
    struct kvm_regs g = { .rip = 0, .rflags = 2 }; ioctl(vcpu, KVM_SET_REGS, &g);
    long exits = 0; double t0 = now();
    for (;;) {
        ioctl(vcpu, KVM_RUN, 0); exits++;
        if (run->exit_reason == KVM_EXIT_HLT) break;
        if (run->exit_reason != KVM_EXIT_IO) { printf("unexpected exit %d\n", run->exit_reason); exit(1); }
    }
    double dt = now() - t0;
    *exits_out = exits;
    return dt;
}

int main(void) {
    int kvm = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    long e;
    double t_io = run_guest(kvm, 1, 200000, &e);
    printf("200,000 iterations with an I/O port write each: %ld VM exits, %.3f s -> %.2f us per exit round trip\n", e, t_io, t_io / e * 1e6);
    double t_cpu = run_guest(kvm, 0, 400000000, &e);
    printf("400,000,000 iterations of pure guest computation: %ld VM exit, %.3f s -> %.2f ns per iteration\n", e, t_cpu, t_cpu / 4e8 * 1e9);
    return 0;
}
