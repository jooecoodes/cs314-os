# Activity 3 — Inter-Process Communication: Pipes

## The program, piece by piece

The program does four things: **creates a tube**, **forks**, the **child reads from the tube**, the **parent writes into the tube**. Each snippet is one step.

**Why we need this at all:** after `fork`, the parent and child have **separate memory**. If the parent sets `int x = 42;`, the child gets a *copy* — changing it in one process does not affect the other. So how do two separate processes share data? A pipe is one answer: a **tube in kernel memory** that both processes can reach through file descriptors.

---

### Snippet 1 — create the tube

```c
int fd[2];
if (pipe(fd) < 0) { perror("pipe"); return 1; }
```

**What it means in plain words:** *"Kernel, make me a pipe. Give me two file descriptors — one for each end."*

**The two ends:**

- `fd[0]` — the **read** end. You call `read(fd[0], ...)` on it.
- `fd[1]` — the **write** end. You call `write(fd[1], ...)` on it.

**A memory trick:** *0 looks like an "o" for "open the door and take in"* → read. *1 looks like a pen* → write. Or just remember: **reading comes before writing in a conversation** — you listen first, then reply.

**Why `int fd[2]`?** Because `pipe` needs to return **two** values (two fds). C can't return two values from a function, so you pass in an array of two ints and the function fills both slots.

**The big idea:** these are **ordinary file descriptors**, the same kind you got from `open()` in Activity 1. After `pipe` returns, you use `read`, `write`, and `close` on them exactly as you did for files. The kernel doesn't care that the other side is a tube instead of a disk file — the interface is identical. That's the "everything is a file" idea in Unix.

**Failure check:** `-1` on failure, `errno` set. Same convention as always.

---

### Snippet 2 — fork

```c
pid_t pid = fork();
if (pid < 0) { perror("fork"); return 1; }
```

**What it means in plain words:** *"Make a copy of me, so we have two processes that can talk to each other through the pipe."*

**Important consequence:** `fork` copies **everything**, including the file descriptor table. So **both the parent and the child have both ends of the pipe open.** The parent has `fd[0]` and `fd[1]`. The child also has `fd[0]` and `fd[1]`. Same tube, four references.

**The picture:**

```
BEFORE fork — one process, both ends:

   fd[0] ──┐
           ├──► pipe
   fd[1] ──┘

AFTER fork — two processes, each has both ends:

   PARENT                    CHILD
   fd[0] ──┐                fd[0] ──┐
           ├──► pipe                 ├──► pipe   ← SAME pipe
   fd[1] ──┘                fd[1] ──┘
```

That "four references to one tube" is the source of the #1 mistake in pipe programming. More on that in the next snippet.

---

### Snippet 3 — child: close the unused end

```c
if (pid == 0) {
    if (close(fd[1]) < 0) { perror("close (child write end)"); _exit(1); }
```

**What it means in plain words:** *"I'm the child. I only want to read, so I'm closing my copy of the write end. I don't need it."*

**This is the most important line in the program.** Let me explain why carefully.

**The rule for pipes:**

> *`read` on a pipe returns `0` (EOF) only when *all* write ends are closed.**

Why? Because the kernel can't know the writer is done until every reference to the write end is gone. If even one process still has a copy of the write end open, the kernel assumes more data might come.

**What would happen without this close:** the parent writes its message and then closes *its* copy of `fd[1]`. The parent thinks "I'm done." But the child **still has a copy of `fd[1]` open** — because `fork` copied it. The kernel sees: "there's still a write end alive — the child's copy!" So the kernel does **not** send EOF. The child's `read` blocks **forever**, waiting for more data that will never come.

**With this close:** the child closes its own copy first. Now there's only **one** write end in the whole system — the parent's. When the parent later closes it, that's the *last* one, and EOF finally fires. The child's `read` wakes up and returns `0`.

**The key sentence for the report:** *each process must close the end it isn't using, otherwise `read` never sees EOF and hangs forever.*

---

### Snippet 4 — child: read from the pipe

```c
    char    buf[128];
    ssize_t n = read(fd[0], buf, sizeof(buf) - 1);
    if (n < 0) { perror("read"); _exit(1); }
    buf[n] = '\0';
```

**What it means in plain words:** *"Kernel, give me up to 127 bytes from the read end of the pipe. Block until there's something to give me, or until all writers are gone."*

**The blocking behavior:** `read` on an empty pipe **blocks**. The process goes to sleep. It wakes when either:

- Someone writes into the pipe → gets the data.
- **All** write ends close → returns `0` (EOF).

That's the entire mechanism behind pipes: writes wake readers, closing the last writer signals EOF.

**Why `sizeof(buf) - 1`?** `buf` is 128 bytes. Leaving one byte free for the terminator. Same habit as in Activity 1.

**Why `buf[n] = '\0'`?** `read` gives you **raw bytes** — no terminator. If you pass `buf` to `printf("%s", ...)` without adding `'\0'`, `printf` reads past the valid data. Same bug as Activity 1, same fix.

**If `n == 0`:** that's EOF — all writers have gone away and there's no more data. The parent didn't write anything? Or it finished writing and closed. Either way, we're done.

---

### Snippet 5 — child: print and exit

```c
    printf("child read: \"%s\"\n", buf);
    if (close(fd[0]) < 0) { perror("close (child read end)"); _exit(1); }
    _exit(0);
}
```

**What it means in plain words:** *"Print what I read, close my remaining end of the pipe, and exit."*

**Why close `fd[0]`?** Hygiene. The process is about to exit, so the kernel would close it anyway — but explicit is better. It makes the code's intent clear and it's good practice.

**Why `_exit`, not `exit`?** Same reason as Activity 2: `exit` flushes inherited stdio buffers, which could cause the parent's buffered output to print twice. `_exit` skips that. Always `_exit` in a forked child.

---

### Snippet 6 — parent: close the unused end

```c
    if (close(fd[0]) < 0) { perror("close (parent read end)"); return 1; }
```

**What it means in plain words:** *"I'm the parent. I only want to write, so I'm closing my copy of the read end."*

**Why?** Same rule as the child's close — but note this is the *mirror image*. The parent closes the read end, the child closes the write end. After both closes:

```
AFTER the closes (the correct setup):

   PARENT                    CHILD
   (closed)  ──┐            fd[0] ──┐
               ├──► pipe             ├──► pipe
   fd[1] ──────┘            (closed)─┘

   Parent only writes.      Child only reads.
```

Two ends open in total. One writer, one reader. Clean.

**Is this close strictly necessary?** For this simple program, not critical — the parent never calls `read`, so leaving `fd[0]` open doesn't cause a hang here. **But it's still correct practice**, and in more complex programs a stray read end can prevent a writer in *another* process from ever seeing "no readers left." **Always close what you don't use.** It's a discipline worth stating in the observations.

---

### Snippet 7 — parent: write the message

```c
    const char *msg = "Hello from parent";
    size_t      len = strlen(msg);
    ssize_t     done = 0;

    while ((size_t)done < len) {
        ssize_t n = write(fd[1], msg + done, len - (size_t)done);
        if (n < 0) { perror("write"); close(fd[1]); return 1; }
        done += n;
    }
```

**What it means in plain words:** *"Kernel, take this message and put it into the pipe. Keep going until all of it is in."*

**The loop is the same shape as in Activity 1** — because `write` can be partial. On a pipe this is especially likely if the pipe's buffer is nearly full (default buffer size on Linux is **64 KiB**). A partial write returns the number it actually moved, and the loop continues with the rest.

**Why `strlen` and not `sizeof`?** Here's the bug from the handout. The reference code used:

```c
write(pipefd[1], message, sizeof(message));
```

`message` was declared `const char *message` — a **pointer**. `sizeof(message)` is the size of a pointer, which is **8 bytes** on a 64-bit machine — not the length of the string. So the reference code would write only **8 bytes**: `"Hello fr"` and then stop. The child would print a truncated message and the parent would think it succeeded.

**The fix:** `strlen(msg)` counts characters until `'\0'`, giving **17** for `"Hello from parent"`. The strace in the next section confirms 17 bytes were written, not 8.

---

### Snippet 8 — parent: close the write end

```c
    if (close(fd[1]) < 0) { perror("close (parent write end)"); return 1; }
```

**What it means in plain words:** *"I'm done writing. Close the write end."*

**This is what allows EOF to fire.** When this `close` returns, the last write end in the system goes away (the child already closed its copy in snippet 3). The kernel signals EOF on the pipe, and the child's `read` returns 0. Without this close, the child would block forever.

**Combine this with snippet 3 and you have the whole EOF rule:**

- Child closes its copy of `fd[1]` → 2 write ends remain
- Parent closes its copy of `fd[1]` → **0 write ends remain** → EOF fires
- Child's `read` returns 0 → child finishes

**If either close is missing, EOF never fires and the child hangs.**

---

### Snippet 9 — parent: wait and finish

```c
    if (waitpid(pid, NULL, 0) < 0) { perror("waitpid"); return 1; }
    printf("parent: child finished\n");
```

**What it means in plain words:** *"Block until the child exits, then print a final line."*

**Two roles again:** `waitpid` reaps the child (prevents zombies), and it also acts as a **synchronization point** — the parent's "child finished" line only prints after the child is genuinely done.

**Order matters here:** the parent **writes before it waits**, not the other way around. What if it were reversed? The parent would call `waitpid` first, blocking until the child finishes. But the child is blocked in `read`, waiting for the parent's data. Nobody writes. **Deadlock.** Both processes asleep forever.

**The correct sequence:**

1. Parent writes → child's `read` wakes and gets data.
2. Parent closes `fd[1]` → EOF fires.
3. Parent calls `waitpid` → blocks.
4. Child reads, prints, exits.
5. `waitpid` returns. Parent finishes.

**Write first, wait after.** Good habit. Mention it in the report — it's a real pattern, not just a style choice.

---

## Compilation

```bash
$ gcc -Wall -Wextra -O2 -o bin/pipe_ipc pipe_ipc/pipe_ipc.c
```

No output. Silent warnings.

---

## Output

```bash
$ ./bin/pipe_ipc
child read: "Hello from parent"
parent: child finished
```

**What to notice:** 17 characters between the quotes — `"Hello from parent"` — not truncated. That's the proof the `strlen` fix works. With the handout's `sizeof(message)` bug, you'd see `"Hello fr"` (only 8 bytes).

**Screenshot:** *(insert terminal screenshot here)*

---

## Syscalls Involved

| Call in the code | What the kernel runs |
|---|---|
| `pipe(fd)` | `pipe2(2)` |
| `read(fd[0], ...)` | `read(2)` |
| `write(fd[1], ...)` | `write(2)` |
| `close(fd[i])` | `close(2)` |
| `fork()` | `clone(2)` |
| `waitpid(...)` | `wait4(2)` |

**Return conventions:**

- `pipe` → `0` on success, `−1` on failure
- `read` → bytes read, **0 at EOF**, **−1** on error
- `write` → bytes written (may be fewer), **−1** on error
- `close` → `0` on success, **−1** on failure

**Notice something important:** once the pipe is created, the `read` and `write` are **ordinary syscalls**. No pipe-specific variants. That's the beauty of "everything is a file" — the same two calls work on files, pipes, sockets, terminals. The **file descriptor** tells the kernel which kind of object to act on.

**Trimmed `strace`:**

```bash
$ strace -f -e trace=pipe2,clone,read,write,close,wait4 ./bin/pipe_ipc
pipe2([3, 4], 0) = 0
        ^ create a pipe; the kernel returns fds 3 (read) and 4 (write)
clone(child_stack=NULL, flags=CLONE_CHILD_CLEARTID|...) = 4313
        ^ this is fork(); child PID is 4313
close(4) = 0
        ^ CHILD closes its write end (fd 4)
close(3) = 0
        ^ PARENT closes its read end (fd 3)
write(4, "Hello from parent", 17) = 17
        ^ PARENT writes 17 bytes — note: 17, not 8 (the sizeof bug would print 8)
close(4) = 0
        ^ PARENT closes its write end → last writer gone
read(3, "Hello from parent", 127) = 17
        ^ CHILD's read wakes up with the 17 bytes
close(3) = 0
        ^ CHILD closes its read end
wait4(4313, NULL, 0, NULL) = 4313
        ^ PARENT blocks until the child exits, then reaps it
```

**Four things to notice:**

1. *`pipe2` not `pipe`.** The libc wrapper `pipe()` calls `pipe2(2)` with flags 0. Same as `open`→`openat` and `fork`→`clone`.
2. **The pipe's fds come back as `[3, 4]`.** Slots 3 and 4 were free. If a program already had fds 3, 4, 5 open, `pipe` would return `[6, 7]` — the lowest free slots, just like `open`.
3. **The parent wrote 17 bytes.** Confirming `strlen` did its job. The handout's `sizeof(pointer)` would have written 8 — the strace would show `write(4, "Hello fr", 8)`.
4. **The child's read woke up after the parent's close.** That's the EOF mechanism in action. Without the close, the child would block forever and the whole program would hang — you'd see `read(3, ...` with no return value in the strace.

**Why `-f`:** to follow the child's syscalls too. Without it, you'd only see the parent's `clone`, `write`, `close`, `wait4` — missing the child's `close(4)` and `read`, which are exactly the interesting bits.

**The order between parent and child syscalls in the strace may differ between runs.** The kernel's scheduler decides who runs first. This is fine — the blocking behavior guarantees correctness regardless of order.

---

## Observations

**Observation 1 — closing the unused pipe end is mandatory, not optional.**
The rule is: `read` on a pipe returns EOF (0) **only when all write ends are closed**. Since `fork` copies the fd table, both processes have both ends after `fork`. If the child keeps its copy of `fd[1]` open, the parent's `close(fd[1])` doesn't trigger EOF — the kernel still sees the child's copy. The child's `read` then blocks forever. I verified this by removing the child's `close(fd[1])` in a test copy: the program hung indefinitely. Restoring the close fixed it. The submitted code closes the unused end in **both** processes: child closes `fd[1]`, parent closes `fd[0]`.

**Observation 2 — `write` on a pipe can be partial, and larger writes can block.**
The loop around `write` (same pattern as Activity 1) handles partial writes. On a pipe this matters more than on a file: if the pipe's buffer is nearly full (default 64 KiB on Linux), `write` returns the number of bytes it actually moved rather than blocking to write everything. For a 17-byte message this never happens, but the loop is correct for any size. A related property: a writer that sends more than 64 KiB without a reader draining the pipe will **block**. If two processes each tried to write more than the buffer without reading, **both would deadlock**. Mention this as a warning: pipes are not safe for that pattern.

**Observation 3 — the handout's `sizeof(message)` is a bug: it sends 8 bytes, not 17.**
The reference code declared `const char *message` and called `write(pipefd[1], message, sizeof(message))`. Since `message` is a **pointer**, `sizeof(message)` is the pointer's size — 8 on x86-64 — not the string's length. The strace confirms the fix: `write(4, "Hello from parent", 17) = 17`. With the handout's version the strace would show `write(4, "Hello fr", 8)`. The child would have printed a truncated message. Same class of bug as the hardcoded `18` in Activity 1 — passing a size that doesn't match the actual data length.

**Observation 4 — `read` on a pipe blocks until data arrives or all writers are gone.**
In the trace, the child's `read(3, ...)` did not return until the parent had written and closed. This is the kernel's blocking behavior: a `read` on an empty pipe with at least one writer alive sleeps the process. It wakes on two conditions only — data available, or EOF (all writers gone). This behavior is what makes pipes work naturally without polling: the reader doesn't waste CPU spinning, and the writer doesn't have to signal "I'm done" except by closing the last write end.

**Observation 5 — write-before-wait, not wait-before-write.**
In the submitted code the parent does `write`, `close`, then `waitpid`. If the order were reversed — `waitpid` first — the parent would block waiting for the child, the child would block in `read` waiting for data, and both would deadlock. Running the version with the reversed order confirmed the hang. This is worth stating because "wait for the child" sounds like the natural first thing to do; in pipe-based programs it's the **last** thing.

---

## What to capture in the screenshot

**Frame 1:** compile command (empty output) + `./bin/pipe_ipc` and its two-line output.

**Frame 2:** the `strace -f -e trace=pipe2,clone,read,write,close,wait4` output. Point at `= 17` on the write.

**Frame 3 (optional, the hang demo):** comment out the child's `close(fd[1])`, recompile, run — it hangs. Screenshot the terminal stuck (with `Ctrl-C` pending). Then restore and show it works. Two screenshots side by side tell the whole story.

---

## The list to carry

- a pipe is a kernel tube; `pipe(fd)` gives `fd[0]` (read) and `fd[1]` (write)
- after `fork`, **both** processes have **both** ends — fds are copied
- **each process must close the end it doesn't use** — otherwise `read` never sees EOF and hangs
- `read` on a pipe blocks until data or all writers close (EOF)
- `write` on a pipe can be partial; large writes block when the buffer is full
- pipes are **one-way** — two-way needs two pipes
- the handout used `sizeof(pointer)` instead of `strlen` — writes 8 bytes, not 17
- parent must **write before it waits**, or deadlock
- `pipe` → `pipe2(2)` under the hood
