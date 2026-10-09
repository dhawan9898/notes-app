#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* /proc/self/io counts this process's system calls: syscw = number of write-like syscalls so far. */
static long syscw(void) {
    FILE *f = fopen("/proc/self/io", "r");
    char line[128]; long v = -1;
    while (f && fgets(line, sizeof line, f)) if (sscanf(line, "syscw: %ld", &v) == 1) break;
    if (f) fclose(f);
    return v;
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "full";
    FILE *f = fopen("/tmp/bufmodes.out", "w");
    if (!f) return 1;
    if (!strcmp(mode, "none")) setvbuf(f, NULL, _IONBF, 0);          /* unbuffered: every call is a syscall */
    else if (!strcmp(mode, "line")) setvbuf(f, NULL, _IOLBF, 4096);   /* flush at each newline */
    else if (!strcmp(mode, "full")) setvbuf(f, NULL, _IOFBF, 65536);  /* flush when 64 KiB accumulate */
    long before = syscw();
    if (!strcmp(mode, "raw")) {                                       /* bypass stdio: one write() per record */
        int fd = fileno(f);
        for (int i = 0; i < 10000; i++) write(fd, "record\n", 7);
    } else {
        for (int i = 0; i < 10000; i++) fputs("record\n", f);
    }
    fflush(f);
    long after = syscw();
    printf("%-5s: 10000 records of 7 bytes -> %5ld write syscalls\n", mode, after - before - 0);
    fclose(f);
    return 0;
}
