#ifndef COMMON_H
#define COMMON_H
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* Loop until all n bytes are sent: send() on a stream socket may accept less. */
static inline int send_all(int fd, const void *buf, size_t n) {
    const char *p = buf;
    while (n) {
        ssize_t w = send(fd, p, n, MSG_NOSIGNAL);          /* MSG_NOSIGNAL: EPIPE instead of SIGPIPE */
        if (w < 0) { if (errno == EINTR) continue; return -1; }
        p += w; n -= (size_t)w;
    }
    return 0;
}

/* Loop until n bytes are received. Returns 0 on success, 1 on clean EOF before the first byte, -1 on error or truncated message. */
static inline int recv_all(int fd, void *buf, size_t n) {
    char *p = buf; size_t got = 0;
    while (got < n) {
        ssize_t r = recv(fd, p + got, n - got, 0);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) return got == 0 ? 1 : -1;
        got += (size_t)r;
    }
    return 0;
}

/* Framing: TCP is a byte stream, not messages. Use a 4-byte big-endian length prefix. */
static inline int send_msg(int fd, const void *data, uint32_t len) {
    uint8_t hdr[4] = { (uint8_t)(len >> 24), (uint8_t)(len >> 16), (uint8_t)(len >> 8), (uint8_t)len };
    return send_all(fd, hdr, 4) == 0 && send_all(fd, data, len) == 0 ? 0 : -1;
}
static inline int recv_msg(int fd, void *buf, uint32_t cap, uint32_t *len) {
    uint8_t hdr[4];
    int rc = recv_all(fd, hdr, 4);
    if (rc) return rc;
    uint32_t n = (uint32_t)hdr[0] << 24 | (uint32_t)hdr[1] << 16 | (uint32_t)hdr[2] << 8 | hdr[3];
    if (n > cap) return -1;                                  /* never trust a peer-supplied length */
    *len = n;
    return n ? recv_all(fd, buf, n) : 0;
}
#endif
