#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* write(2) may write fewer bytes than asked (pipes, sockets, signals, disk full): loop until done. */
static int write_all(int fd, const void *buf, size_t n) {
    const char *p = buf;
    while (n > 0) {
        ssize_t w = write(fd, p, n);
        if (w < 0) { if (errno == EINTR) continue; return -1; }
        p += w; n -= (size_t)w;
    }
    return 0;
}

/* read(2) may return fewer bytes too; 0 means EOF. Returns bytes read (== n unless EOF) or -1. */
static ssize_t read_full(int fd, void *buf, size_t n) {
    char *p = buf; size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, p + got, n - got);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) break;
        got += (size_t)r;
    }
    return (ssize_t)got;
}

/* Atomically replace a file: write temp, fsync, rename, fsync the directory. */
static int write_file_atomic(const char *dir, const char *name, const void *data, size_t n) {
    char tmp[256], dst[256];
    snprintf(tmp, sizeof tmp, "%s/.%s.XXXXXX", dir, name);
    snprintf(dst, sizeof dst, "%s/%s", dir, name);
    int fd = mkstemp(tmp);                                         /* O_EXCL, mode 0600, unique name */
    if (fd < 0) return -1;
    int rc = -1;
    if (write_all(fd, data, n) == 0 && fsync(fd) == 0) rc = 0;
    if (close(fd) != 0) rc = -1;                                    /* close can report deferred write errors */
    if (rc == 0 && rename(tmp, dst) != 0) rc = -1;
    if (rc != 0) { unlink(tmp); return -1; }
    int dfd = open(dir, O_RDONLY | O_DIRECTORY);
    if (dfd >= 0) { fsync(dfd); close(dfd); }                       /* make the rename durable */
    return 0;
}

int main(void) {
    /* demonstrate a SHORT WRITE: a non-blocking pipe accepts only what fits in its buffer */
    int p[2];
    if (pipe2(p, O_NONBLOCK | O_CLOEXEC)) return 1;
    static char big[1 << 20];
    memset(big, 'x', sizeof big);
    ssize_t w = write(p[1], big, sizeof big);
    printf("write(pipe, 1 MiB) returned %zd (short write: the pipe buffer is 64 KiB)\n", w);
    ssize_t w2 = write(p[1], big, 10);
    printf("second write on the full pipe: %zd, errno=%s\n", w2, w2 < 0 ? strerror(errno) : "-");
    close(p[0]); close(p[1]);

    const char msg[] = "config v2\n";
    if (write_file_atomic("/tmp", "demo_config.txt", msg, sizeof msg - 1) == 0) {
        char back[64] = {0};
        int fd = open("/tmp/demo_config.txt", O_RDONLY | O_CLOEXEC);
        ssize_t r = read_full(fd, back, sizeof back - 1);
        close(fd);
        printf("atomic write ok, read back %zd bytes: %s", r, back);
        struct stat st; stat("/tmp/demo_config.txt", &st);
        printf("mode=%o size=%lld\n", st.st_mode & 0777, (long long)st.st_size);
        unlink("/tmp/demo_config.txt");
    }
    return 0;
}
