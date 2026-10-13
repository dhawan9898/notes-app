// A complete (tiny) virtual machine monitor on the KVM API: one vCPU, 64 KiB of guest RAM, real-mode guest code.
// The guest adds two numbers, prints a string through an I/O port, then halts. Every "device" access exits to this program.
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define DIE(msg) do { perror(msg); exit(1); } while (0)

int main(void) {
    int kvm = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (kvm < 0) DIE("open /dev/kvm");
    printf("KVM API version: %d (must be 12)\n", ioctl(kvm, KVM_GET_API_VERSION, 0));

    int vm = ioctl(kvm, KVM_CREATE_VM, 0);                                        // a VM is a file descriptor
    if (vm < 0) DIE("KVM_CREATE_VM");

    // Guest "RAM" is ordinary anonymous memory of this process, registered as guest physical memory at address 0.
    size_t mem_size = 0x10000;
    uint8_t *mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    struct kvm_userspace_memory_region region = { .slot = 0, .guest_phys_addr = 0x0, .memory_size = mem_size, .userspace_addr = (uintptr_t)mem };
    if (ioctl(vm, KVM_SET_USER_MEMORY_REGION, &region) < 0) DIE("KVM_SET_USER_MEMORY_REGION");

    // 16-bit real-mode program at guest address 0:
    //   mov ax, 2; add ax, 3        ; ax = 5
    //   mov dx, 0x3f8               ; "serial port" I/O address
    //   mov al, 'H'; out dx, al ...  ; one OUT per character
    //   add ax, '0' ... out         ; print the sum as a digit
    //   hlt
    const uint8_t code[] = {
        0xb8, 0x02, 0x00,        // mov ax, 2
        0x05, 0x03, 0x00,        // add ax, 3
        0x89, 0xc3,              // mov bx, ax      (remember the sum)
        0xba, 0xf8, 0x03,        // mov dx, 0x3f8
        0xb0, 'H', 0xee,         // mov al,'H' ; out dx,al
        0xb0, 'i', 0xee,         // mov al,'i' ; out dx,al
        0xb0, ' ', 0xee,         // mov al,' ' ; out dx,al
        0x88, 0xd8,              // mov al, bl
        0x04, '0', 0xee,         // add al,'0' ; out dx,al   -> prints the digit 5
        0xb0, '\n', 0xee,        // newline
        0xf4,                    // hlt
    };
    memcpy(mem, code, sizeof code);

    int vcpu = ioctl(vm, KVM_CREATE_VCPU, 0);
    int run_size = ioctl(kvm, KVM_GET_VCPU_MMAP_SIZE, 0);
    struct kvm_run *run = mmap(NULL, run_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu, 0);   // shared exit-reason area

    struct kvm_sregs sregs; ioctl(vcpu, KVM_GET_SREGS, &sregs);
    sregs.cs.base = 0; sregs.cs.selector = 0;                                    // real mode, code segment at address 0
    ioctl(vcpu, KVM_SET_SREGS, &sregs);
    struct kvm_regs regs = { .rip = 0x0, .rflags = 0x2 };                         // bit 1 of rflags is always set
    ioctl(vcpu, KVM_SET_REGS, &regs);

    int exits = 0, io_exits = 0;
    printf("guest output: ");
    for (;;) {
        if (ioctl(vcpu, KVM_RUN, 0) < 0) DIE("KVM_RUN");                          // runs guest code natively until something needs the host
        exits++;
        switch (run->exit_reason) {
        case KVM_EXIT_IO:
            if (run->io.direction == KVM_EXIT_IO_OUT && run->io.port == 0x3f8) {
                putchar(*((char *)run + run->io.data_offset)); io_exits++;
            }
            break;
        case KVM_EXIT_HLT:
            ioctl(vcpu, KVM_GET_REGS, &regs);
            printf("guest halted. final ax=%llu bx=%llu, rip=0x%llx\n", (unsigned long long)regs.rax, (unsigned long long)regs.rbx, (unsigned long long)regs.rip);
            printf("VM exits handled: %d total, %d were I/O port writes (one per character)\n", exits, io_exits);
            return 0;
        default:
            printf("unexpected exit reason %d\n", run->exit_reason); return 1;
        }
    }
}
