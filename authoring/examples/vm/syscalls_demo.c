/* System calls by number, the vDSO, and what a real system call costs compared with a vDSO call. */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

/* the raw system call: 32-bit systems with 64-bit time use clock_gettime64 and a 64-bit timespec */
#ifdef SYS_clock_gettime64
#define RAW_CLOCK_GETTIME SYS_clock_gettime64
struct kernel_ts { long long sec, nsec; };
#else
#define RAW_CLOCK_GETTIME SYS_clock_gettime
struct kernel_ts { long sec, nsec; };
#endif

static double ns_per_call(int use_syscall, int n) {
    struct timespec ts, t0, t1; struct kernel_ts kts;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < n; i++) { if (use_syscall) syscall(RAW_CLOCK_GETTIME, CLOCK_MONOTONIC, &kts); else clock_gettime(CLOCK_MONOTONIC, &ts); }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return ((double)(t1.tv_sec - t0.tv_sec) * 1e9 + (double)(t1.tv_nsec - t0.tv_nsec)) / n;
}

int main(void) {
    printf("1. a system call made by number, bypassing the C wrapper:\n"); fflush(stdout);
    long raw = syscall(SYS_getpid);
    printf("   syscall(SYS_getpid) returned %s\n", raw == getpid() ? "the same PID as getpid()" : "a different value"); fflush(stdout);
    const char msg[] = "   hello written with syscall(SYS_write, fd=1, ...)\n";
    long n = syscall(SYS_write, 1, msg, sizeof msg - 1);
    printf("   write returned %ld (the number of bytes written)\n", n);
    errno = 0; long bad = syscall(SYS_write, 999, msg, sizeof msg - 1);
    printf("   write to an invalid descriptor returned %ld, errno %d (%s)\n", bad, errno, strerror(errno));

    printf("2. the vDSO: part of the kernel mapped into every process\n");
    FILE *f = fopen("/proc/self/maps", "r"); char line[512];
    while (f && fgets(line, sizeof line, f)) if (strstr(line, "[vdso]") || strstr(line, "[vvar]")) { char range[64], tag[32]; sscanf(line, "%63s", range); char *t = strrchr(line, '['); sscanf(t, "%31s", tag); printf("   %s %s\n", range, tag); }
    if (f) fclose(f);

    int N = 200000; double fast = ns_per_call(0, N), slow = ns_per_call(1, N);
    printf("3. clock_gettime via libc/vDSO: %.0f ns per call; via a real system call: %.0f ns per call (ratio %.1fx)\n", fast, slow, slow / fast);
    return 0;
}
