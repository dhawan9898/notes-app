---
kind: question
title: One cleanup path
module: Advanced Language
difficulty: Hard
---

Write `char *slurp(const char *path, size_t *len)` that reads a whole file into a NUL-terminated buffer. It must return `NULL` with `errno` set on every failure, free everything it allocated and close the file on **every** error path, using a single cleanup label. Why save `errno` before cleaning up?

## Explanation

```c
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read a whole file into memory. Returns NULL and sets errno on failure; single exit point cleans up. */
static char *slurp(const char *path, size_t *len_out) {
    char *buf = NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;                                   /* errno already set by fopen */
    if (fseek(f, 0, SEEK_END) != 0) goto fail;
    long size = ftell(f);
    if (size < 0) goto fail;
    rewind(f);
    buf = malloc((size_t)size + 1);
    if (!buf) goto fail;
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) { errno = EIO; goto fail; }
    buf[size] = '\0';
    fclose(f);
    *len_out = (size_t)size;
    return buf;
fail:
    {
        int saved = errno;                                 /* fclose/free may change errno */
        free(buf);
        fclose(f);
        errno = saved;
    }
    return NULL;
}

int main(void) {
    FILE *t = fopen("sample.txt", "w");
    if (!t) { perror("sample.txt"); return 1; }
    fputs("hello from a file\n", t);
    fclose(t);

    size_t len;
    char *text = slurp("sample.txt", &len);
    if (text) { printf("read %zu bytes: %s", len, text); free(text); }

    text = slurp("does-not-exist.txt", &len);
    if (!text) printf("slurp failed: %s (errno %d)\n", strerror(errno), errno);
    remove("sample.txt");
    return 0;
}
```

```text
$ gcc -std=c17 -Wall -Wextra -g -fsanitize=address,undefined -o p4_errors p4_errors.c && ./p4_errors; rm -f p4_errors sample.txt
read 18 bytes: hello from a file
slurp failed: No such file or directory (errno 2)
```

The `goto fail` pattern keeps one cleanup block instead of repeating `free`/`fclose` at every error. Each failure sets `errno` (library calls do it automatically; the short read sets `EIO` by hand). `errno` is saved because `free` and `fclose` are allowed to change it, and the caller needs the *original* reason: the second call shows "No such file or directory (errno 2)". The AddressSanitizer build confirms there are no leaks or invalid accesses on either path. Note that `goto` for cleanup is accepted style in C; jumping *backwards* or into scopes is what makes code unreadable.
