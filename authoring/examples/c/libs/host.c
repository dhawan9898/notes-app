#include <dlfcn.h>
#include <stdio.h>
int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "./plugin.so";
    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);          /* load at run time */
    if (!h) { fprintf(stderr, "dlopen failed: %s\n", dlerror()); return 1; }
    int (*run)(int) = (int (*)(int))dlsym(h, "plugin_run");  /* look up a symbol by name */
    if (!run) { fprintf(stderr, "dlsym failed: %s\n", dlerror()); dlclose(h); return 1; }
    printf("result: %d\n", run(21));
    dlclose(h);
    return 0;
}
