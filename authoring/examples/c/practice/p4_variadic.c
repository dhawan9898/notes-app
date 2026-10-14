#include <stdarg.h>
#include <stdio.h>

static void log_msg(const char *level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void log_msg(const char *level, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "[%s] ", level);
    vfprintf(stdout, fmt, ap);                 /* forward the arguments to the v-version */
    fputc('\n', stdout);
    va_end(ap);
}

int main(void) {
    log_msg("info", "started with %d workers", 4);
    log_msg("warn", "disk %s is %d%% full", "/data", 91);
    return 0;
}
