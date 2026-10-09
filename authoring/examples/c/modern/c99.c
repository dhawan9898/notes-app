#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct opts { int width, height; const char *title; bool fullscreen; };

static void show(struct opts o) { printf("%dx%d \"%s\" fullscreen=%d\n", o.width, o.height, o.title ? o.title : "(none)", o.fullscreen); }

struct packet { unsigned len; unsigned char data[]; };   /* flexible array member */

int main(void) {
    show((struct opts){ .width = 800, .height = 600, .title = "demo" });         /* designated init + compound literal */
    int primes[] = { [2] = 5, [5] = 13, 17 };                                   /* indices can be designated; gaps are zero */
    for (size_t i = 0; i < sizeof primes / sizeof *primes; i++) printf("%d ", primes[i]);
    puts("");

    struct packet *p = malloc(sizeof *p + 5);
    p->len = 5; memcpy(p->data, "hello", 5);
    printf("packet len=%u sizeof(struct packet)=%zu data=%.*s\n", p->len, sizeof *p, (int)p->len, p->data);
    free(p);

    int n = 3;
    int vla[n];                                                                 /* VLA: stack allocation, size unchecked */
    for (int i = 0; i < n; i++) vla[i] = i * i;
    printf("VLA sizeof=%zu (runtime value)\n", sizeof vla);
    return 0;
}
