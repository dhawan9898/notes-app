#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char buf[8];
    const char *lit = "hello";
    printf("sizeof(\"hello\")=%zu strlen=%zu sizeof(lit)=%zu\n", sizeof("hello"), strlen("hello"), sizeof lit);

    /* strncpy does not guarantee termination */
    memset(buf, 'X', sizeof buf);
    strncpy(buf, "0123456789", sizeof buf);
    printf("strncpy result terminated? %s\n", memchr(buf, '\0', sizeof buf) ? "yes" : "NO (no NUL inside the buffer)");

    /* snprintf always terminates and returns the length it WANTED to write */
    int need = snprintf(buf, sizeof buf, "%s-%d", "abcdef", 12345);
    printf("snprintf wanted %d bytes, buffer holds \"%s\" (%zu chars)\n", need, buf, strlen(buf));
    if (need >= (int)sizeof buf) puts("  -> truncation detected by comparing the return value");

    /* strtol: the only correct way to parse numbers */
    const char *inputs[] = {"42", "  -17xyz", "", "99999999999999999999", "abc"};
    for (size_t i = 0; i < sizeof inputs / sizeof *inputs; i++) {
        char *end; errno = 0;
        long v = strtol(inputs[i], &end, 10);
        const char *status = end == inputs[i] ? "no digits" : errno == ERANGE ? "out of range" : *end ? "trailing junk" : "ok";
        printf("strtol(\"%s\") = %ld  [%s]\n", inputs[i], v, status);
    }

    /* memmove vs memcpy for overlapping regions */
    char s[] = "abcdef";
    memmove(s + 2, s, 4);
    printf("memmove overlap -> %s\n", s);

    /* UTF-8: bytes are not characters */
    const char *u = "naïve ✓";
    size_t cps = 0;
    for (const unsigned char *p = (const unsigned char *)u; *p; p++) if ((*p & 0xC0) != 0x80) cps++;
    printf("\"%s\": strlen=%zu bytes, %zu code points\n", u, strlen(u), cps);
    return 0;
}
