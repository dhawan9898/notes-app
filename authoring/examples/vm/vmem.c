/* Watch virtual memory work: reserved versus resident, anonymous versus file-backed pages. Linux only. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static long status_mib(const char *key) {                               /* VmSize or VmRSS from /proc/self/status, in MiB */
    FILE *f = fopen("/proc/self/status", "r"); char line[256]; long kb = -1; size_t n = strlen(key);
    while (f && fgets(line, sizeof line, f)) if (!strncmp(line, key, n)) { kb = atol(line + n + 1); break; }
    if (f) fclose(f);
    return kb / 1024;
}
static void show(const char *label) { printf("%-44s VmSize %5ld MiB   VmRSS %4ld MiB\n", label, status_mib("VmSize"), status_mib("VmRSS")); }

int main(void) {
    const size_t MiB = 1024 * 1024, page = (size_t)sysconf(_SC_PAGESIZE);
    show("1. at start");
    char *m = mmap(NULL, 512 * MiB, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);       /* ask for 512 MiB of anonymous memory */
    if (m == MAP_FAILED) { perror("mmap"); return 1; }
    show("2. after mmap of 512 MiB (not touched)");
    for (size_t off = 0; off < 128 * MiB; off += page) m[off] = 1;                                       /* touch 128 MiB, one byte per page */
    show("3. after touching 128 MiB of it");
    for (size_t off = 128 * MiB; off < 512 * MiB; off += page) m[off] = 1;
    show("4. after touching all 512 MiB");
    munmap(m, 512 * MiB);
    show("5. after munmap");

    /* file-backed mapping: pages come from the page cache, shared between processes */
    char path[] = "/tmp/pr_mem_demo.XXXXXX"; int fd = mkstemp(path);
    char *chunk = malloc(MiB); memset(chunk, 0xA5, MiB);
    for (int i = 0; i < 64; i++) if (write(fd, chunk, MiB) != (ssize_t)MiB) { perror("write"); return 1; }
    free(chunk);
    char *fm = mmap(NULL, 64 * MiB, PROT_READ, MAP_PRIVATE, fd, 0);
    show("6. file mapped (64 MiB), not read yet");
    volatile unsigned long sum = 0;
    for (size_t off = 0; off < 64 * MiB; off += page) sum += (unsigned char)fm[off];
    show("7. after reading one byte per page");
    munmap(fm, 64 * MiB); close(fd); unlink(path);
    printf("page size: %zu bytes\n", page);
    return 0;
}
