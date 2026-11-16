/* shm_ipc.c
 * Demonstrates System V shared memory (shmget/shmat/shmdt/shmctl).
 * Compile : gcc -Wall -Wextra -o shm_ipc shm_ipc.c
 * Prepare : touch shmfile        (required by ftok)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

int main(void)
{
    key_t key = ftok("shmfile", 65);
    if (key == (key_t)-1) { perror("ftok (did you run 'touch shmfile'?)"); return 1; }

    int shmid = shmget(key, 1024, 0666 | IPC_CREAT);
    if (shmid < 0) { perror("shmget"); return 1; }

    char *str = shmat(shmid, NULL, 0);
    if (str == (char *)-1) { perror("shmat"); return 1; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        /* ---------- CHILD ---------- */
        /* Mapping is inherited across fork(), so no shmat needed here. */
        strcpy(str, "Hello from child process");
        if (shmdt(str) < 0) { perror("shmdt (child)"); _exit(1); }
        _exit(0);
    }

    /* ---------- PARENT ---------- */
    if (waitpid(pid, NULL, 0) < 0) { perror("waitpid"); return 1; }
    /* waitpid is the SYNCHRONISATION here: child has definitely written. */

    printf("parent read: \"%s\"\n", str);

    if (shmdt(str) < 0) { perror("shmdt (parent)"); return 1; }
    if (shmctl(shmid, IPC_RMID, NULL) < 0) { perror("shmctl IPC_RMID"); return 1; }

    return 0;
}
