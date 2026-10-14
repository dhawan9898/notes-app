#include <stdio.h>
#include <string.h>

typedef enum { RED, GREEN, BLUE } Color;        /* names for small whole numbers: RED=0, GREEN=1, BLUE=2 */

typedef struct {                                /* typedef: "Pixel" instead of "struct pixel" everywhere */
    int x, y;
    Color color;
} Pixel;

static const char *color_name(Color c) {
    switch (c) {                                /* the compiler warns if you forget an enum value */
        case RED:   return "red";
        case GREEN: return "green";
        case BLUE:  return "blue";
    }
    return "unknown";
}

int main(void) {
    Pixel p = {.x = 5, .y = 7, .color = GREEN};
    printf("pixel at (%d, %d) is %s (stored as the number %d)\n", p.x, p.y, color_name(p.color), p.color);

    Pixel screen[2] = {{0, 0, RED}, {1, 0, BLUE}};
    for (size_t i = 0; i < sizeof screen / sizeof screen[0]; i++)
        printf("screen[%zu] = %s\n", i, color_name(screen[i].color));

    Pixel copy;
    memcpy(&copy, &p, sizeof p);                /* a struct is plain bytes; memcpy copies all of them */
    copy.color = BLUE;
    printf("original is still %s, copy is %s\n", color_name(p.color), color_name(copy.color));
    return 0;
}
