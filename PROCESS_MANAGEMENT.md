# Activity 2 — Process Management

## The program, piece by piece

The program does three things: **makes a copy of itself**, the copy **becomes `ls`**, and the original **waits for the copy to finish**. Each snippet below is one step.

---

### Snippet 1 — make a copy

```c
pid_t pid = fork();
```

**What it means in plain words:** *"Kernel, make a copy of me. Now there are two of us running the same code from this point forward."*

**The key idea:** `fork()` **returns twice** — once in the original (the **parent**), once in the copy (the **child**). They each get a **different value back**:

- **Parent** gets the **child's PID** — a real number, like `1234`. Meaning: *"You're the original. Your copy's name tag is 1234."*
- **Child** gets *`0`**. Meaning: *"You're the copy."*
- If fork fails, it returns *`-1`** (rare, but possible — like out of memory).

**Why `0` for the child?** It's not a PID. It's a **role signal** — a code word meaning "you're the new one." PID 0 is reserved by the kernel itself, so `0` can never collide with a real PID. Every child of every fork gets `0`. No exceptions.

**After this line, there are two processes**, each with its own copy of every variable. If the child changes `pid`, the parent doesn't see the change — they have separate memory now.

---

### Snippet 2 — check for fork failure

```c
if (pid < 0) { perror("fork"); return 1; }
```

**What it means in plain words:** *"If fork returned -1, something went wrong — print the reason and stop."*

**Same rule as always:** `-1` means failure, and `errno` explains why. `perror("fork")` prints something like `fork: Resource temporarily unavailable` (if the system is out of process slots or memory).

---

### Snippet 3 — figure out which one I am

```c
if (pid == 0) {
    /* child code here */
}
/* parent code here */
```

**What it means in plain words:** *"If my answer was 0, I'm the child — run the child branch. Otherwise, I'm the parent — skip to the parent branch."*

**The important thing to understand:** this `if` is **not executed once.** It's executed **twice** — once by the parent, once by the child, each in its own process. The parent's `pid` is `1234` (a positive number), so the parent **skips** the if-block and runs the parent code. The child's `pid` is `0`, so the child **enters** the if-block and runs the child code.

**So both branches of the program run.** Just not in the same process. The child runs one side, the parent runs the other. That's the whole trick of fork-based programming.

**The picture:**

```
                     fork()
                       │
          ┌────────────┴────────────┐
          │                         │
      PARENT                     CHILD
      pid = 1234                 pid = 0
          │                         │
      if (pid == 0)?             if (pid == 0)?
         false → skip               true → enter
          │                         │
      run parent code            run child code
```

---

### Snippet 4 — child announces itself

```c
printf("child : pid=%d ppid=%d\n", getpid(), getppid());
```

**What it means in plain words:** *"Print my real PID and my parent's real PID."*

**Why both `getpid` and `getppid`?**

- `getpid()` returns **this process's real PID** — the actual name tag the kernel assigned.
- `getppid()` returns **this process's parent's PID** — who created me.

**Notice:** the child **does** have a real PID. The `0` from `fork` was not the child's PID — the child's real PID comes from `getpid()`. The `0` was just the "you're the copy" signal. This is worth pointing out in the report; it's a common confusion.

---

### Snippet 5 — the child becomes `ls`

```c
execlp("ls", "ls", "-l", (char *)NULL);
```

**What it means in plain words:** *"Throw away my own code and data. Load `ls` into my body. Run `ls` with the argument `-l`. I am no longer me — I am `ls`."*

**What `exec` actually does:** it **replaces the entire process image**. Same PID (still `1234+1`), but the code, the memory, the stack — all of it gets thrown away and replaced with the new program's code. From that moment, the process **is** `ls`. It starts from `ls`'s `main`, not from where `execlp` was called.

**The letters in `execlp`:**

- *`exec`** — the family name
- *`l`** — arguments are passed as a **list** (one by one)
- *`p`** — search *`PATH`** for the program, so `"ls"` is enough — no need for `"/bin/ls"`

**Why `(char *)NULL` at the end?** The argument list must end with a **null pointer** so the kernel knows where the list stops. Without it, `exec` would keep reading memory looking for the end — undefined behavior. The cast `(char *)NULL` is just to silence a warning about the type.

**The critical fact:** `exec` **never returns on success.** If the process successfully becomes `ls`, then "your program" no longer exists — there is nothing left to return to. The line after `exec` is only reached if `exec` **failed**.

---

### Snippet 6 — what happens if `exec` fails

```c
perror("execlp");
_exit(1);
```

**What it means in plain words:** *"If we got here, `exec` failed. Print the reason and exit."*

**This code only runs on failure.** That's why it's immediately after the `exec` call — not a fallback, but the **only** path where anything after `exec` exists at all.

**Why `_exit` and not `exit`?** `exit()` flushes buffered output inherited from the parent. If the parent had printed something to a buffer and the child inherits that buffer, `exit()` in the child would flush it — printing the same text twice. `_exit()` skips the flush and terminates cleanly. Small detail, real bug — worth mentioning in observations.

---

### Snippet 7 — parent announces itself

```c
printf("parent: pid=%d child=%d\n", getpid(), pid);
```

**What it means in plain words:** *"Print my real PID and my child's real PID."*

Here `pid` (the variable) holds the child's **real PID** — because the parent received a positive number from `fork`. `getpid()` returns the parent's own PID. So this line prints both, and you can match them up with the child's line from snippet 4.

**If both lines print correctly**, you can see:

- child line: `child : pid=1235 ppid=1234`
- parent line: `parent: pid=1234 child=1235`

The child's `ppid` matches the parent's `pid`, and the parent's `child` matches the child's `pid`. Consistent — proof that fork worked as described.

---

### Snippet 8 — wait for the child

```c
int status;
if (waitpid(pid, &status, 0) < 0) { perror("waitpid"); return 1; }
```

**What it means in plain words:** *"Kernel, block me here until that specific child finishes. Then give me its exit status."*

**What `waitpid` does:**

- `pid` — which child to wait for. (Use `-1` for "any child.")
- `&status` — a pointer to an int. The kernel **writes the child's exit information into it**. That's why it's passed by address — the function needs to write into your variable.
- `0` — no special flags. (You can use `WNOHANG` to mean "don't block, just check.")

**Why this matters — the zombie problem:** when a child finishes, it doesn't vanish. It becomes a **zombie** — a frozen leftover holding only its exit status. It stays a zombie until the parent calls `wait`/`waitpid` to **reap** it. `waitpid` collects the status and frees the child's slot in the kernel's process table.

**The dangerous case:** a **long-running parent** that keeps forking children and never reaps them. Over days, zombies pile up and consume process table slots. That's the real-world bug. (If the parent dies, the zombie gets adopted by `init`/`systemd` and reaped automatically — so it's only a problem for parents that keep running.)

**One more role `waitpid` plays here:** it **blocks until the child finishes**. That makes it a **synchronization point** — the parent knows for certain the child is done before it prints its "child exited" line.

---

### Snippet 9 — read the exit status

```c
if (WIFEXITED(status))
    printf("parent: child exited with code %d\n", WEXITSTATUS(status));
else
    printf("parent: child terminated abnormally\n");
```

**What it means in plain words:** *"The status integer is packed with details. Unpack it to find out how the child ended."*

**Why unpack?** The `status` value isn't just the exit code. It's a bit-packed integer that also records **whether the child was killed by a signal**. A child killed by a signal (like `SIGSEGV` from a crash) has no normal exit code — the parent should treat that differently.

**The two macros:**

- *`WIFEXITED(status)`** — true if the child exited **normally** (returned from `main` or called `_exit`).
- *`WEXITSTATUS(status)`** — if so, returns the actual exit code (0–255).
- *`WIFSIGNALED(status)`** — true if the child was **killed by a signal** (not shown here, but worth knowing).

For our `ls -l` child, `WIFEXITED` is true and `WEXITSTATUS` is `0` — `ls` succeeded.

---

## Compilation

```bash
$ gcc -Wall -Wextra -O2 -o bin/proc_mgmt proc_mgmt/proc_mgmt.c
```

No output. Warnings all silent.

**Note on the handout:** the reference code called `wait(NULL)` but **forgot to include `<sys/wait.h>`**. Without that header, `wait` and `WIFEXITED` are undeclared — modern gcc will warn, and the code may fail to link. The submitted code includes it.

---

## Output

```bash
$ ./bin/proc_mgmt
child : pid=4312 ppid=4311
parent: pid=4311 child=4312
total 12
-rw-r--r-- 1 user user  1234 Nov 16 10:42 file_ops.c
...
parent: child exited with code 0
```

**What you should see:**

1. The child's line (with its real PID and the parent's PID)
2. The parent's line (with its real PID and the child's PID)
3. The output of `ls -l` from the child (now `ls`, not our program)
4. The parent's final "child exited" line

The PIDs will differ each run — that's normal, the kernel assigns them fresh.

**Ordering caveat:** the child's line and parent's line **may print in either order.** Two separate processes are writing to the same terminal — the kernel doesn't guarantee who gets there first. This is not a bug. It's the actual nature of concurrency. Mention this in your observations.

**Screenshot:** *(insert terminal screenshot here)*

---

## Syscalls Involved

| Call in the code | What the kernel runs |
|---|---|
| `fork()` | `clone(2)` |
| `execlp(...)` | `execve(2)` |
| `waitpid(...)` | `wait4(2)` |
| `getpid()` | `getpid(2)` |
| `getppid()` | `getppid(2)` |

**Return conventions:**

- `fork` → **0 in child**, **child's PID in parent**, **−1** on failure
- `execlp` → **never returns on success**; only returns on failure (with `−1` and `errno`)
- `waitpid` → **child's PID** on success, **−1** on error, **0** if `WNOHANG` was used and child hasn't exited

**Trimmed `strace`:**

```bash
$ strace -f -e trace=clone,execve,wait4 ./bin/proc_mgmt
clone(child_stack=NULL, flags=CLONE_CHILD_CLEARTID|...) = 4312
        ^ this is fork(); the kernel creates a new process, PID 4312
        ^ the parent's next syscall will be wait4, the child's will be execve
execve("/usr/bin/ls", ["ls", "-l"], 0x7fff...) = 0
        ^ the CHILD has replaced itself with /bin/ls
        ^ 0 here means "the new program is now running" — exec succeeded
wait4(4312, [{WIFEXITED(s) && WEXITSTATUS(s) == 0}], 0, NULL) = 4312
        ^ the PARENT blocked here until the child exited
        ^ returned 4312 (the child's PID) with status "exited normally, code 0"
```

**Three things to notice:**

1. *`clone` is fork.** Modern Linux implements `fork` via `clone(2)` with flags that mean "copy everything." So `strace` shows `clone`, not `fork`.
2. *`execve` replaced the child.** After `clone`, the child's next important syscall is `execve("/usr/bin/ls", ...)`. The child is now `ls`. The line after `execlp` in the source was never reached — because `execve` succeeded.
3. *`wait4` is `waitpid`.** The `4` is the version — the kernel syscall is `wait4(2)`. It blocked until the child exited, then returned the child's PID.

**Why `-f` in the strace command?** Because we need to see the child's syscalls too. Without `-f`, strace only follows the original process — you'd see `clone` but not the child's `execve`.

---

## Observations

**Observation 1 — `fork` returns twice, with different values in parent and child.**
I confirmed this by having both processes print `getpid()` and the value `fork` returned. The child printed `pid=4312 ppid=4311` — meaning the child's real PID is 4312 and its parent is 4311. The parent printed `pid=4311 child=4312` — its own PID is 4311 and the child it created is 4312. The cross-consistency proves fork behaved exactly as documented: parent got 4312 (a real PID), child got 0 (but its real PID is 4312, retrieved via `getpid`).

**Observation 2 — the output order between parent and child is not guaranteed.**
Running the program several times showed the child line and parent line can appear in either order. This is not a bug. Two separate processes are writing to the same terminal, and the kernel makes no promise about which runs first. To make the ordering deterministic, the program would need synchronization (a pipe, or a semaphore) — not something `fork` provides.

**Observation 3 — `exec` never returns on success.**
The line `perror("execlp"); _exit(1);` after `execlp` was never reached. This is by design: on success, the child has become `ls` and "our program" no longer exists to continue past the call. The strace confirms: after `execve("/usr/bin/ls", ...)`, the child's syscalls become those of `ls` (file opens, directory reads, writes to stdout). We only reach that code if `exec` fails — e.g. if the program name doesn't exist.

**Observation 4 — `waitpid` prevents zombies and provides synchronization.**
Without the `waitpid` call, the child would finish and become a zombie — a frozen leftover holding its exit status, waiting for the parent to collect it. With `waitpid`, the parent blocks until the child exits, then the kernel cleans up the child's process table entry. The strace confirms: `wait4(4312, ...)` blocked and returned 4312, meaning the child was reaped. Note also that `waitpid` acts as a synchronization point here — the parent's "child exited" line only prints after the child is genuinely done.

**Observation 5 — the handout had two issues: a missing header and `exit` vs `_exit`.**
The reference code called `wait(NULL)` without including `<sys/wait.h>`. On modern gcc, `wait`, `WIFEXITED`, and `WEXITSTATUS` are all undeclared — the compiler warns and the program may fail to link. Adding the header fixes it. Separately, the reference code used `exit(1)` in the child's `exec` failure path. That flushes inherited stdio buffers, which can cause the parent's buffered output to print twice. The submitted code uses `_exit(1)`, which skips the flush and terminates cleanly.

---

## What to capture in the screenshot

**Frame 1:** compile command and its empty output, then `./bin/proc_mgmt` and its full output (child line, parent line, `ls -l` listing, "child exited" line).

**Frame 2 (optional, for the zombie demo):** comment out `waitpid`, recompile, run, then `ps -el | grep Z` showing a zombie — see Activity (h) when we get there.

**Frame 3:** the `strace -f -e trace=clone,execve,wait4` output.

---

## The list to carry

- `fork` returns twice: 0 in child, child's PID in parent
- the child **does** have a real PID — retrievable via `getpid()`
- process output ordering between parent and child is non-deterministic
- `exec` never returns on success — code after it only runs on failure
- a child not reaped becomes a **zombie** until the parent calls `wait`/`waitpid`
- `waitpid` also acts as a **synchronization point**
- the handout missed `<sys/wait.h>` — doesn't compile cleanly without it
- `exit()` vs `_exit()` in a forked child — the former can double-flush buffered output
