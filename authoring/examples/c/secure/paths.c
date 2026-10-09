#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BASE "/tmp/secure_demo_root"

/* Naive check: string prefix test on the user-supplied path. Defeated by "..", symlinks. */
static int naive_ok(const char *user) { return strstr(user, "..") == NULL; }

/* Canonicalize, then require the result to stay inside BASE. */
static int contained(const char *user, char *out) {
    char joined[PATH_MAX], real[PATH_MAX];
    snprintf(joined, sizeof joined, "%s/%s", BASE, user);
    if (!realpath(joined, real)) return -errno;
    size_t n = strlen(BASE);
    if (strncmp(real, BASE, n) != 0 || (real[n] != '/' && real[n] != '\0')) return -EACCES;   /* "/tmp/secure_demo_root2" must not match */
    strcpy(out, real);
    return 0;
}

int main(void) {
    mkdir(BASE, 0700); mkdir(BASE "2", 0700);
    FILE *f = fopen(BASE "/public.txt", "w"); fputs("public\n", f); fclose(f);
    f = fopen(BASE "2/secret.txt", "w"); fputs("secret\n", f); fclose(f);
    unlink(BASE "/escape"); symlink("/etc", BASE "/escape");               /* a link inside BASE pointing outside */

    const char *tests[] = { "public.txt", "../secure_demo_root2/secret.txt", "escape/passwd", "../../etc/passwd" };
    char out[PATH_MAX];
    for (size_t i = 0; i < sizeof tests / sizeof *tests; i++) {
        int rc = contained(tests[i], out);
        printf("%-34s naive=%-5s contained=%s\n", tests[i], naive_ok(tests[i]) ? "ALLOW" : "deny",
               rc == 0 ? "ALLOW" : rc == -EACCES ? "deny (escapes base)" : "deny (error)");
    }
    unlink(BASE "/escape"); unlink(BASE "/public.txt"); unlink(BASE "2/secret.txt"); rmdir(BASE); rmdir(BASE "2");
    return 0;
}
