/* Make and reap a zombie: the child exits at once, the parent has not called wait() yet. */
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void show(pid_t pid, const char *when) {                       /* name, state and parent from /proc/PID/status */
    char path[64], line[256], name[64] = "?", state[64] = "?", ppid[32] = "?";
    snprintf(path, sizeof path, "/proc/%d/status", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f) { printf("%-22s /proc/%d does not exist: no such process\n", when, (int)pid); return; }
    while (fgets(line, sizeof line, f)) {
        if (!strncmp(line, "Name:", 5)) sscanf(line + 5, " %63s", name);
        else if (!strncmp(line, "State:", 6)) { sscanf(line + 6, " %63[^\n]", state); }
        else if (!strncmp(line, "PPid:", 5)) sscanf(line + 5, " %31s", ppid);
    }
    fclose(f);
    printf("%-22s pid %d, parent %s, name %s, state %s\n", when, (int)pid, ppid, name, state);
}

int main(void) {
    pid_t pid = fork();
    if (pid == 0) _exit(0);                                             /* child exits immediately */
    usleep(300000);                                                     /* the parent has not called wait() */
    show(pid, "before waitpid:");
    waitpid(pid, NULL, 0);                                              /* reap it */
    show(pid, "after waitpid:");
    return 0;
}
