/* proc_mgmt.c
 * Demonstrates fork(), execlp(), and waitpid().
 * Compile: gcc -Wall -Wextra -o proc_mgmt proc_mgmt.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>          /* REQUIRED for waitpid / WIFEXITED */

int main(void)
{
    pid_t pid = fork();

    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        /* ---------- CHILD ---------- */
        printf("child : pid=%d ppid=%d\n", getpid(), getppid());

        execlp("ls", "ls", "-l", (char *)NULL);
        /* reached ONLY if exec failed */
        perror("execlp");
        _exit(1);              /* _exit, not exit: avoid double-flushing */
    }

    /* ---------- PARENT ---------- */
    printf("parent: pid=%d child=%d\n", getpid(), pid);

    int status;
    if (waitpid(pid, &status, 0) < 0) { perror("waitpid"); return 1; }

    if (WIFEXITED(status))
        printf("parent: child exited with code %d\n", WEXITSTATUS(status));
    else
        printf("parent: child terminated abnormally\n");

    return 0;
}
