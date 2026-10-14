#include <stdio.h>
#include <ctype.h>
#include <string.h>

int main(void) {
    char name[16] = "Ada";                        /* room for 15 letters plus the final '\0' */
    printf("\"%s\" has length %zu but occupies %zu bytes\n", name, strlen(name), sizeof name);

    if (strcmp(name, "Ada") == 0) printf("equal: strcmp returns 0 when strings match\n");

    char greeting[32];
    snprintf(greeting, sizeof greeting, "Hello, %s!", name);   /* bounded: cannot write past 32 bytes */
    printf("%s\n", greeting);

    const char *long_text = "much too long";
    char small[6];
    int needed = snprintf(small, sizeof small, "%s", long_text);
    printf("small holds \"%s\" (cut off); the full text needed %d characters\n", small, needed);

    for (size_t i = 0; name[i] != '\0'; i++) name[i] = (char)toupper((unsigned char)name[i]);
    printf("upper-cased in place: %s\n", name);
    return 0;
}
