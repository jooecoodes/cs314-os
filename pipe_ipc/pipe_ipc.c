/* pipe_ipc.c
 * Demonstrates pipe() + fork() for one-way IPC.
 * Compile: gcc -Wall -Wextra -o pipe_ipc pipe_ipc.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
    int fd[2];                        /* fd[0]=read, fd[1]=write */

    if (pipe(fd) < 0) { perror("pipe"); return 1; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        /* ---------- CHILD: reader ---------- */
        if (close(fd[1]) < 0) { perror("close (child write end)"); _exit(1); }

        char    buf[128];
        ssize_t n = read(fd[0], buf, sizeof(buf) - 1);
        if (n < 0) { perror("read"); _exit(1); }
        buf[n] = '\0';

        printf("child read: \"%s\"\n", buf);

        if (close(fd[0]) < 0) { perror("close (child read end)"); _exit(1); }
        _exit(0);
    }

    /* ---------- PARENT: writer ---------- */
    if (close(fd[0]) < 0) { perror("close (parent read end)"); return 1; }

    const char *msg = "Hello from parent";
    size_t      len = strlen(msg);    /* strlen, NOT sizeof */
    ssize_t     done = 0;

    while ((size_t)done < len) {
        ssize_t n = write(fd[1], msg + done, len - (size_t)done);
        if (n < 0) { perror("write"); close(fd[1]); return 1; }
        done += n;
    }
    if (close(fd[1]) < 0) { perror("close (parent write end)"); return 1; }

    if (waitpid(pid, NULL, 0) < 0) { perror("waitpid"); return 1; }
    printf("parent: child finished\n");
    return 0;
}
