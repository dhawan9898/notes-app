#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- 1. errno: set by failing library calls, meaningful ONLY right after a failure ---- */
static void errno_demo(void) {
    FILE *f = fopen("/nonexistent/file", "r");
    if (!f) printf("fopen failed: errno=%d (%s)\n", errno, strerror(errno));

    errno = 0;
    long v = strtol("99999999999999999999", NULL, 10);     /* success paths do NOT reset errno: clear it first */
    printf("strtol: v=%ld errno=%d (%s)\n", v, errno, strerror(errno));
}

/* ---- 2. an error enum plus a single cleanup path ---- */
typedef enum { OK = 0, E_NOMEM, E_IO, E_PARSE } status_t;
static const char *status_str(status_t s) {
    static const char *names[] = { "ok", "out of memory", "I/O error", "parse error" };
    return s <= E_PARSE ? names[s] : "unknown";
}

static status_t load_number(const char *path, long *out) {
    status_t rc = OK;
    FILE *f = NULL;
    char *line = NULL;
    size_t cap = 0;

    f = fopen(path, "r");
    if (!f) { rc = E_IO; goto out; }
    if (getline(&line, &cap, f) < 0) { rc = E_IO; goto out; }
    char *end;
    errno = 0;
    long v = strtol(line, &end, 10);
    if (end == line || errno == ERANGE) { rc = E_PARSE; goto out; }
    *out = v;
out:
    free(line);                      /* free(NULL) and the NULL check on f make the cleanup unconditional */
    if (f) fclose(f);
    return rc;
}

/* ---- 3. variadic functions ---- */
static double average(int count, ...) {
    va_list ap;
    va_start(ap, count);
    double sum = 0;
    for (int i = 0; i < count; i++) sum += va_arg(ap, double);     /* arguments must be passed as double! */
    va_end(ap);
    return count ? sum / count : 0;
}

static void errorf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void errorf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fputs("error: ", stdout);
    vprintf(fmt, ap);
    fputc('\n', stdout);
    va_end(ap);
}

int main(void) {
    errno_demo();

    FILE *f = fopen("/tmp/errors_demo_number.txt", "w");
    fputs("  1234 trailing\n", f);
    fclose(f);
    long n = 0;
    status_t rc = load_number("/tmp/errors_demo_number.txt", &n);
    printf("load_number -> %s, n=%ld\n", status_str(rc), n);
    rc = load_number("/no/such/file", &n);
    printf("load_number(/no/such/file) -> %s\n", status_str(rc));

    printf("average(3, 1.0, 2.0, 6.0) = %g\n", average(3, 1.0, 2.0, 6.0));
    errorf("code %d at %s", 7, "startup");
    return 0;
}
