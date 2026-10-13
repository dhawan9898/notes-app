// The string-function traps: strncpy, snprintf's return value, strtok, memcpy vs memmove, and isalpha(char).
#include <ctype.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char dst[8]; memset(dst, 'X', sizeof dst);
    strncpy(dst, "overlong text", sizeof dst);                       // copies 8 bytes, writes NO terminator
    printf("strncpy into char[8]: terminated within the buffer? %s\n", memchr(dst, '\0', sizeof dst) ? "yes" : "NO");

    char small[8];
    int would_need = snprintf(small, sizeof small, "%s-%d", "overlong", 12345);
    printf("snprintf into char[8]: stored \"%s\", returned %d (the length it WOULD have needed; truncated: %s)\n", small, would_need, would_need >= (int)sizeof small ? "yes" : "no");

    char line[] = "a,b,,c";
    printf("strtok on \"a,b,,c\": ");
    for (char *t = strtok(line, ","); t; t = strtok(NULL, ",")) printf("[%s]", t);
    printf("   <- the empty field vanished and the original string was modified\n");
    char copy[] = "a,b,,c";
    printf("strsep on the same input: ");
    char *cursor = copy, *tok;
    while ((tok = strsep(&cursor, ",")) != NULL) printf("[%s]", tok);
    printf("   <- strsep (BSD/glibc) keeps empty fields\n");

    char buf[] = "0123456789";
    memmove(buf + 2, buf, 6);                                       // overlapping regions: memmove is defined, memcpy is not
    printf("memmove(buf+2, buf, 6) on \"0123456789\": %s\n", buf);

    signed char ch = (signed char)0xE9;                             // 'e acute' in Latin-1: negative as a signed char
    printf("isalpha((unsigned char)0xE9) in the C locale: %d   (passing a negative char instead is undefined behavior)\n", isalpha((unsigned char)ch) != 0);
    printf("strlen is O(n): strlen(\"\") = %zu, strlen(\"hello\") = %zu; sizeof(\"hello\") = %zu (includes the terminator)\n", strlen(""), strlen("hello"), sizeof("hello"));
    return 0;
}
