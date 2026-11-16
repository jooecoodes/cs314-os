/* file_ops.c
 * Demonstrates open/read/write/close with correct partial-I/O handling.
 * Compile: gcc -Wall -Wextra -o file_ops file_ops.c
 */
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *path = "example.txt";

    /* ---------- write ---------- */
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open (write)"); return 1; }

    const char *msg  = "Hello, System Calls!";
    size_t      len  = strlen(msg);
    ssize_t     done = 0;

    while ((size_t)done < len) {
        ssize_t n = write(fd, msg + done, len - (size_t)done);
        if (n < 0) { perror("write"); close(fd); return 1; }
        done += n;
    }
    if (close(fd) < 0) { perror("close (write)"); return 1; }

    /* ---------- read ---------- */
    fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open (read)"); return 1; }

    char    buf[128];
    ssize_t total = 0, n;

    while ((n = read(fd, buf + total,
                     (size_t)(sizeof(buf) - 1) - (size_t)total)) > 0)
        total += n;
    if (n < 0) { perror("read"); close(fd); return 1; }

    buf[total] = '\0';          /* read() does not terminate for you */
    printf("Read from file: %s\n", buf);

    if (close(fd) < 0) { perror("close (read)"); return 1; }
    return 0;
}
