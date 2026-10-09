#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* VULNERABLE: the filename is pasted into a shell command line. */
static void list_bad(const char *name) {
    char cmd[256];
    snprintf(cmd, sizeof cmd, "ls -d %s 2>/dev/null", name);
    system(cmd);
}

/* BETTER: no shell. Arguments are passed as an argv array, so metacharacters are just characters. */
static void list_good(const char *name) {
    pid_t pid = fork();
    if (pid == 0) {
        execlp("ls", "ls", "-d", "--", name, (char *)NULL);     /* "--" also stops option injection like "-rf" */
        _exit(127);
    }
    int st; waitpid(pid, &st, 0);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);                    /* keep our output ordered with the children's */
    const char *attacker_input = "/tmp ; echo INJECTED-COMMAND-RAN";
    puts("--- system() with attacker input:");
    list_bad(attacker_input);
    puts("--- execlp() with the same input:");
    list_good(attacker_input);
    return 0;
}
