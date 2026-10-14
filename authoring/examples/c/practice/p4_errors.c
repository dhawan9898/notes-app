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
