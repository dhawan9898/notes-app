// errno rules, printf return/format traps and getline.
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    FILE *f = fopen("/nonexistent/file", "r");
    int saved = errno;                                              // copy at once: any later library call may overwrite errno
    printf("fopen failed: %s (errno %d); errno is only meaningful right after a call that reported failure\n", strerror(saved), saved);
    (void)f;
    errno = ENOENT; strtol("1", NULL, 10);                          // a successful call
    printf("a successful call does not clear errno: still %d (%s). Set errno = 0 before calls whose only error signal is errno\n", errno, strerror(errno));

    int n = printf("%s %d\n", "printf returns the number of characters written:", 42);
    printf("(that was %d characters)\n", n);
    printf("sizeof(long)=%zu sizeof(size_t)=%zu: use %%zu for size_t, %%ld for long, PRId64 for int64_t\n", sizeof(long), sizeof(size_t));

    FILE *tmp = tmpfile();
    fputs("first line\nsecond, longer line than the first\n\nlast", tmp); rewind(tmp);
    char *line = NULL; size_t cap = 0; ssize_t len; int i = 0;
    while ((len = getline(&line, &cap, tmp)) != -1) printf("getline #%d: %zd bytes: \"%.*s\"\n", ++i, len, (int)(len && line[len - 1] == '\n' ? len - 1 : len), line);
    free(line); fclose(tmp);

    volatile size_t huge = (size_t)-1; errno = 0; char *big = malloc(huge);
    printf("malloc(SIZE_MAX) -> %s, errno %d (%s)\n", big ? "non-NULL" : "NULL", errno, strerror(errno));
    return 0;
}
