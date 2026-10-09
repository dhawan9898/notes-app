#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
#define PATH "/tmp/readbench.dat"
static long count_nl_bytes_read(void) {           /* read() one byte at a time: one syscall per byte */
    int fd = open(PATH, O_RDONLY); char c; long n = 0;
    while (read(fd, &c, 1) == 1) n += c == '\n';
    close(fd); return n;
}
static long count_nl_stdio(void) {                /* getc: buffered, takes a lock per call */
    FILE *f = fopen(PATH, "r"); int c; long n = 0;
    while ((c = getc(f)) != EOF) n += c == '\n';
    fclose(f); return n;
}
static long count_nl_unlocked(void) {             /* getc_unlocked: buffered, no locking */
    FILE *f = fopen(PATH, "r"); int c; long n = 0;
    while ((c = getc_unlocked(f)) != EOF) n += c == '\n';
    fclose(f); return n;
}
static long count_nl_block(void) {                /* read() in 64 KiB blocks */
    int fd = open(PATH, O_RDONLY); static char buf[1 << 16]; long n = 0; ssize_t r;
    while ((r = read(fd, buf, sizeof buf)) > 0) for (ssize_t i = 0; i < r; i++) n += buf[i] == '\n';
    close(fd); return n;
}
static long count_nl_mmap(void) {                 /* mmap the whole file */
    int fd = open(PATH, O_RDONLY); struct stat st; fstat(fd, &st);
    const char *p = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0); long n = 0;
    for (off_t i = 0; i < st.st_size; i++) n += p[i] == '\n';
    munmap((void *)p, (size_t)st.st_size); close(fd); return n;
}
int main(void) {
    FILE *f = fopen(PATH, "w");                    /* 8 MiB of text lines */
    for (int i = 0; i < 400000; i++) fprintf(f, "line number %d of the benchmark file\n", i);
    fclose(f);
    struct { const char *name; long (*fn)(void); } t[] = {
        {"read() 1 byte at a time", count_nl_bytes_read}, {"getc (stdio, locked)", count_nl_stdio},
        {"getc_unlocked", count_nl_unlocked}, {"read() 64 KiB blocks", count_nl_block}, {"mmap", count_nl_mmap} };
    struct stat st; stat(PATH, &st);
    printf("file size: %.1f MiB (page cache warm)\n", st.st_size / 1048576.0);
    for (size_t k = 0; k < 5; k++) { double t0 = now(); long n = t[k].fn(); printf("%-26s %8.1f ms   lines=%ld\n", t[k].name, (now() - t0) * 1e3, n); }
    unlink(PATH);
    return 0;
}
