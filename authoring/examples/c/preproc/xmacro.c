#include <stdio.h>
#include <string.h>

/* X-macro: define the list ONCE, generate enum, names and handlers from it. */
#define COLORS(X) \
    X(RED,   0xFF0000) \
    X(GREEN, 0x00FF00) \
    X(BLUE,  0x0000FF)

enum color {
#define X(name, rgb) COLOR_##name,
    COLORS(X)
#undef X
    COLOR_COUNT
};

static const char *color_name[] = {
#define X(name, rgb) [COLOR_##name] = #name,       /* # stringifies, ## pastes tokens */
    COLORS(X)
#undef X
};

static const unsigned color_rgb[] = {
#define X(name, rgb) [COLOR_##name] = rgb,
    COLORS(X)
#undef X
};

#define LOG(level, fmt, ...) fprintf(stderr, "[%s] %s:%d %s(): " fmt "\n", level, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define TYPE_NAME(x) _Generic((x), int: "int", long: "long", double: "double", char *: "char*", const char *: "const char*", default: "other")

#define STATIC_ASSERT_SIZE(T, n) _Static_assert(sizeof(T) == (n), #T " must be " #n " bytes")
STATIC_ASSERT_SIZE(int, 4);

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

int main(void) {
    for (int c = 0; c < COLOR_COUNT; c++) printf("%d %-5s #%06X\n", c, color_name[c], color_rgb[c]);
    printf("ARRAY_LEN(color_name) = %zu\n", ARRAY_LEN(color_name));
    LOG("INFO", "plain message");
    LOG("WARN", "value=%d name=%s", 42, color_name[COLOR_BLUE]);
    printf("%s %s %s %s\n", TYPE_NAME(1), TYPE_NAME(1L), TYPE_NAME(2.5), TYPE_NAME("str"));
    return 0;
}
