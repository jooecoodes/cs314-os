# Activity 4 — Inter-Process Communication: Shared Memory

## The program, piece by piece

The program does five things in order: **creates a shared memory segment**, **attaches it**, **forks**, the **child writes into it**, the **parent waits and reads**. Each snippet is one step.

**Why this exists at all:** in Activity 3, every byte of a pipe went through the kernel — a copy in, a copy out. Shared memory skips that entirely: the kernel maps **the same physical RAM** into both processes, and after the mapping is done, reads and writes are just ordinary memory access. No syscalls, no copies. Fastest IPC on the system — and also the most dangerous, because the kernel steps aside and provides **no synchronization**. More on that in snippet 6.

---

### Snippet 1 — compute a key

```c
key_t key = ftok("shmfile", 65);
if (key == (key_t)-1) { perror("ftok (did you run 'touch shmfile'?)"); return 1; }
```

**What it means in plain words:** *"Turn this file path and this number into a unique key that other processes can also compute."*

**Why a key?** Both processes need to agree on *which* segment of shared memory to use. `ftok` ("file to key") computes a key from a file path and a small integer. Any process that calls `ftok` with the **same path and same integer** gets the **same key** — so both processes end up at the same segment.

**The critical prerequisite:** `ftok` calls `stat()` on the path internally. **The file must already exist.** If it doesn't, `ftok` returns `-1`, the key is garbage, and `shmget` fails — nothing works. This is why the workflow includes:

```bash
touch shmfile
```

The handout's code **never mentions this**, and it's the #1 reason students' shared memory programs fail with a mysterious error. Put this in your observations.

**What's the `65`?** It's `'A'` in ASCII — an arbitrary byte. Any nonzero byte works, as long as **both processes use the same one**. Since both call `ftok("shmfile", 65)` in this program, they compute the same key.

---

### Snippet 2 — create or fetch the segment

```c
int shmid = shmget(key, 1024, 0666 | IPC_CREAT);
if (shmid < 0) { perror("shmget"); return 1; }
```

**What it means in plain words:** *"Kernel, using this key, give me a shared memory segment of 1024 bytes. Create it if it doesn't exist yet."*

**The three arguments:**

- `key` — which segment to look for (computed by `ftok`)
- `1024` — the size in bytes
- `0666 | IPC_CREAT` — permissions (everyone can read/write) plus "create if it doesn't exist"

**What comes back:** an integer called the **shmid** (segment ID) — something like `32769`. This is **not a pointer**. It's just an identifier, like a file descriptor for shared memory. You'll pass it to the next calls.

**Failure check:** `-1`, same convention as always. `perror("shmget")` prints the reason — often `No space left on device` if the system is out of shared memory slots, or `Permission denied`.

---

### Snippet 3 — attach the segment to this process

```c
char *str = shmat(shmid, NULL, 0);
if (str == (char *)-1) { perror("shmat"); return 1; }
```

**What it means in plain words:** *"Kernel, take that segment and map it into my address space. Give me a pointer to it that I can use like any other memory."*

**The "mapping" idea:** every process has a **page table** — a map that translates "address `0x7f001000` in this process" → "physical RAM frame 48213." Normally, two processes' page tables point at totally separate RAM.

Shared memory works by making **both processes' page tables point at the same physical frames**:

```
Parent's page table:              Child's page table:
  0x7f001000 → frame 48213          0x7f501000 → frame 48213
                    ▲                              ▲
                    └──────────┬───────────────────┘
                               │
                     same physical RAM frame
```

So when the parent writes to its pointer, the child sees the change at **its** pointer — same physical bytes, different addresses.

**The three arguments:**

- `shmid` — which segment to attach
- `NULL` — let the kernel choose **where** to map it (you almost never want to pick the address yourself)
- `0` — flags (no special options)

**What comes back:** a **real `char *` pointer** — usable like any other C pointer. You can `strcpy` into it, read from it, index into it. No syscalls needed for those operations; it's just memory access from here on.

**Failure returns `(char *)-1`** — an unusual-looking `-1` cast to a pointer. That's the shmat convention. Check it.

**At this point**, the parent process has a pointer to the 1024-byte segment. Setup done. **All subsequent accesses to `str` are ordinary memory reads and writes.** No syscalls.

---

### Snippet 4 — fork

```c
pid_t pid = fork();
if (pid < 0) { perror("fork"); return 1; }
```

**What it means in plain words:** *"Make a copy of me. The copy inherits the shared memory mapping."*

**Important subtlety:** `fork` copies the **page table** of the parent. Since `str` is already mapped, the **child inherits the mapping** — it can use `str` immediately after fork, no extra `shmat` needed. Both processes point at the **same physical RAM**.

**Contrast with `exec`:** if the child called `exec` instead of running on inherited code, the mapping would be **discarded** — `exec` replaces the process image. The child would have to call `shmat` again. Since we only `fork` (no `exec`), the mapping carries over.

That's why the child code in this program doesn't need a `shmat` — it's already inherited.

---

### Snippet 5 — child writes to shared memory

```c
if (pid == 0) {
    /* CHILD */
    strcpy(str, "Hello from child process");
```

**What it means in plain words:** *"I'm the child. Copy my message into the shared memory segment."*

**The big idea:** this is a **plain memory write.** `strcpy` is a userspace library function (in the C standard library — not a syscall). It just copies bytes from the child's own stack buffer into the shared segment. **No kernel involvement.**

The bytes physically change in the shared physical RAM frame. When the parent reads `str`, it reads the same bytes — same physical memory, different virtual address.

**Compare to Activity 3:** there, every byte went through a `write` syscall (copy into kernel) and a `read` syscall (copy back out). Here, **no syscalls** — just memory access. That's the speed advantage of shared memory.

**The danger in this simplicity:** the kernel doesn't know when the child wrote. It doesn't know when the parent reads. It doesn't synchronize anything. That's the next observation's territory — but keep it in mind while reading.

---

### Snippet 6 — child detaches and exits

```c
    if (shmdt(str) < 0) { perror("shmdt (child)"); _exit(1); }
    _exit(0);
}
```

**What it means in plain words:** *"Unmap the segment from my address space, then exit."*

**What `shmdt` does:** it **detaches** the segment — removes the mapping from this process's page table. After this, `str` is invalid in this process. The physical RAM is **not** freed — other processes still attached keep it alive.

**Is this strictly necessary?** No — when the process exits, the kernel auto-detaches it. But **explicit is better**: it makes intent clear, and it's what the man page recommends. The handout omits it in the child, which is sloppy — mention it in observations.

*`_exit` not `exit`, again** — same reason as Activities 2 and 3. `exit` would flush inherited stdio buffers; `_exit` skips that.

---

### Snippet 7 — parent: wait for the child

```c
if (waitpid(pid, NULL, 0) < 0) { perror("waitpid"); return 1; }
```

**This is the most important line in the program.** Let's be precise about **why**.

**What it does mechanically:** blocks until the child exits, then reaps it. Same as in Activities 2 and 3.

**What it does in this program specifically — the synchronization:** shared memory gives **zero synchronization**. If the parent reads `str` **before** the child writes, it reads garbage (or zeroes, or leftover data from a previous run). The result depends on which process happens to be scheduled first — a **race condition**.

`waitpid` eliminates the race in this program by ensuring:

- The child **wrote** to `str` and **then exited** (this is the code's order).
- `waitpid` returns only **after** the child exits.
- Therefore, when the parent runs the next line, the child is done writing.

**Timeline:**

```
time ─────────────────────────────────────────────────────────►
CHILD:   strcpy(str, "...") ──► exit(0)
                                   │
                                   │ bytes now in RAM
                                   ▼
PARENT:  ──── waitpid(pid, ...) ── wakes up here
                                   │
                                   ▼
                             printf("%s", str)   ← safe
```

**The general lesson for the report:** `waitpid` here isn't just for zombie reaping (though it does that too). It's **doing synchronization duty** — providing a happens-before relationship between the child's write and the parent's read. **Shared memory itself provides no such thing.** In a general program (multiple writers, ongoing communication), `waitpid` isn't enough — you'd need a **mutex or semaphore placed in (or alongside) the shared segment**. That's beyond the lab scope, but stating it shows you understand why the simple version works and what its limits are.

---

### Snippet 8 — parent reads from shared memory

```c
printf("parent read: \"%s\"\n", str);
```

**What it means in plain words:** *"Print whatever's in the shared segment now."*

**Why this is safe here:** because `waitpid` completed just above, we know the child wrote to `str`. So `str` contains `"Hello from child process"`.

**Plain memory read** — no syscall. `printf` reads from `str` byte by byte; those bytes come from the shared RAM frame the child wrote to.

**What would happen without `waitpid`:** the parent might read `str` before the child wrote. It would print an empty string (segment is zero-initialized on creation) or, worse, leftover data from a previous run — if `IPC_RMID` wasn't called and the segment still exists. Both are "correct" from the kernel's point of view (you asked for bytes, it gave you bytes) — the problem is **your program is wrong**, not the kernel.

---

### Snippet 9 — parent detaches

```c
if (shmdt(str) < 0) { perror("shmdt (parent)"); return 1; }
```

**What it means in plain words:** *"Unmap the segment from my address space."*

**Same as snippet 6**, but now in the parent. After this, `str` is invalid in the parent — **but the physical RAM still exists**. Whether it gets freed depends on snippet 10.

---

### Snippet 10 — parent marks the segment for deletion

```c
if (shmctl(shmid, IPC_RMID, NULL) < 0) { perror("shmctl IPC_RMID"); return 1; }
```

**What it means in plain words:** *"Kernel, mark this segment for deletion. When no process is attached any more, free the RAM."*

*`shmctl` is "shared memory control"** — the administrative call for a segment. `IPC_RMID` is the specific command: "remove."

**The subtlety worth knowing:** `IPC_RMID` **doesn't delete immediately.** It marks the segment for deletion. The kernel actually **frees the RAM when the last attached process detaches.** This is **reference counting** — that's how a segment doesn't vanish out from under a still-attached process.

So the sequence here is:

- Parent calls `IPC_RMID` → segment **marked** for deletion
- Child is already gone (detached in snippet 6)
- Parent detaches (snippet 9) → now **zero** attached processes
- Kernel **frees the physical RAM**

**What if you forget `IPC_RMID`?** The segment **leaks**. It stays in the kernel's shared memory table **forever** — even after every process using it has exited — because nothing has told the kernel it's done.

Check with:

```bash
ipcs -m
```

You'll see rows with `nattch` (attached process count) and size. If `nattch` is `0` and there's no `dest` flag, **the segment is orphaned**: nobody's using it, but it's still consuming RAM.

Clean up manually:

```bash
ipcrm -m <shmid>
```

**This is worth demonstrating in the report:** comment out the `shmctl` line, run the program twice, then `ipcs -m` → two leaked segments. Clean them with `ipcrm`. That screenshot tells the story better than any paragraph.

**Docker caveat (since you're running in a container):** each container has its own **IPC namespace**. So `ipcs -m` inside the container shows **only that container's** segments — not the host's. The leak demonstration works, but you must **capture the screenshot before exiting the container**. When `docker run --rm` exits, the container's IPC namespace is destroyed and every segment in it is freed — so the leak doesn't persist across container runs. That's fine for the report; just don't expect to see the leak after `exit`.

---

## Compilation

```bash
$ gcc -Wall -Wextra -O2 -o bin/shm_ipc shm_ipc/shm_ipc.c
```

No output. Warnings silent.

**Note the prerequisite:** the program requires `shmfile` in the current directory. The `Makefile`'s `run-shm` target handles this:

```makefile
run-shm: $(BIN)/shm_ipc
	touch shmfile
	./$(BIN)/shm_ipc
```

---

## Output

```bash
$ touch shmfile
$ ./bin/shm_ipc
parent read: "Hello from child process"
```

**Verification that cleanup happened:**

```bash
$ ipcs -m
------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status
```

Empty (or no entries for this program) — the segment was freed.

**Screenshot:** *(insert terminal screenshot here — include `ipcs -m` before and after to show the segment lifecycle)*

---

## Syscalls Involved

| Call in the code | What the kernel runs |
|---|---|
| `ftok("shmfile", 65)` | `stat(2)` on the path — **a library function, not a syscall itself** |
| `shmget(...)` | `ipc(2)` with sub-op `SHMGET` |
| `shmat(...)` | `ipc(2)` with sub-op `SHMAT` |
| `shmdt(...)` | `ipc(2)` with sub-op `SHMDT` |
| `shmctl(..., IPC_RMID, ...)` | `ipc(2)` with sub-op `SHMCTL` |
| `fork()` | `clone(2)` |
| `waitpid(...)` | `wait4(2)` |
| *`strcpy(str, ...)` and `printf("%s", str)`** | **No syscall at all** — plain memory access |

**That last row is the point of the whole activity.** Compare to Activity 3, where every byte went through `write` and `read` syscalls. Here, after the initial mapping, **moving data is a memory operation — zero syscalls.**

**Return conventions:**

- `shmget` → **shmid ≥ 0** on success, **−1** on failure
- `shmat` → **pointer** on success, *`(char *)-1`** on failure
- `shmdt` → **0** on success, **−1** on failure
- `shmctl` → **0** on success, **−1** on failure

**Trimmed `strace`:**

```bash
$ strace -f -e trace=ipc,clone,wait4 ./bin/shm_ipc
ipc(IPC_SHMGET, ...) = 5
        ^ shmget created a segment; the shmid is 5
ipc(IPC_SHMAT, ...) = 0x7f3c80001000
        ^ shmat mapped it; the pointer is 0x7f3c80001000
        ^ NOTE: this is a PTRACE-visible marker. Real strace output shows ipc sub-ops.
clone(child_stack=NULL, flags=CLONE_CHILD_CLEARTID|...) = 4313
        ^ fork created the child
        ^ NOTE: between clone and the next syscall, the child does:
        ^     strcpy(str, "Hello from child process")   ← NO SYSCALLS
        ^     shmdt(str)                                ← ipc SHMDT
        ^     exit(0)
        ^ And the parent's next syscalls are:
ipc(IPC_SHMDT, ...) = 0
        ^ child detached
ipc(IPC_SHMDT, ...) = 0
        ^ parent detached
ipc(IPC_SHMCTL, ..., IPC_RMID, ...) = 0
        ^ parent marked the segment for deletion
wait4(4313, NULL, 0, NULL) = 4313
        ^ parent reaped the child
```

**Two things to notice:**

1. *`ipc(2)` is one syscall multiplexing all System V IPC operations.** The sub-op (SHMGET, SHMAT, SHMDT, SHMCTL, and the semaphore and message-queue operations) is encoded in the first argument. That's a design artifact from the 1980s — a single "IPC multiplexer" syscall. Modern strace decodes these into the readable forms above.

2. **Between `clone` and the child's `shmdt`, no syscalls appear for the actual write.** The child's `strcpy` is pure memory copying — zero trap to the kernel. **That's the speed.** That's why databases and browsers use shared memory for large data.

**Why `-f`:** to follow the child's syscalls too. Without it, you'd miss the child's `shmdt` and the exit order.

**Optional: `strace -c -f ./bin/shm_ipc`** gives a **summary count** by syscall. It typically shows only a handful of syscalls total — most of the program's runtime is spent in userspace memory operations, not kernel transitions. That's a great one-line evidence for the "shared memory is fastest IPC" claim.

---

## Observations

**Observation 1 — `ftok` requires the file to exist; otherwise the entire program fails.**
`ftok` internally calls `stat` on the path. If the file doesn't exist, it returns `-1` and the subsequent `shmget` fails — nothing else works. I confirmed this by running the program in a directory without `shmfile`: the very first `perror` fired. The fix is `touch shmfile`, and the submitted Makefile's `run-shm` target does this automatically. **The handout never mentions this**, which is a real usability trap.

**Observation 2 — plain `strcpy` and `printf` on the shared segment involve no syscalls.**
The strace between the child's `clone` and its `shmdt` shows no `write` or `read` — because `strcpy` is a userspace memory copy and `printf` only flushes at exit. That's the fundamental performance advantage of shared memory over pipes: **data transfer becomes a memory operation, not a syscall**. To make the contrast vivid, compare with Activity 3's strace: every byte there appeared inside a `write` and `read` syscall.

**Observation 3 — shared memory provides no synchronization; `waitpid` is doing that job here.**
The `waitpid(pid, NULL, 0)` call is not just for zombie reaping — it's **synchronization**. It guarantees the child has written before the parent reads. Without it, the parent might read the segment before the child writes, giving garbage (or empty) output — a race condition. I verified this by commenting out `waitpid` and running the program several times: some runs printed correct output, others printed empty output, depending on scheduling. **The correct general solution** is to place a **semaphore or mutex in (or alongside) the shared segment** and lock/unlock it around the read and write. `waitpid` works here only because the child writes exactly once and then exits — a very simple pattern. This is outside the lab's scope but worth noting as the standard practice.

**Observation 4 — the mapping is inherited across `fork`, so the child does not call `shmat` again.**
The child uses the same `str` pointer as the parent because `fork` copies the page table, including the shared mapping. If the child were created via `exec` instead, the mapping would be discarded and the child would need its own `shmat`. The program demonstrates this by having the child write to `str` immediately after fork, without any additional attach call.

**Observation 5 — `IPC_RMID` marks the segment for deletion; forgetting it leaks RAM, and `ipcs -m` shows it.**
`IPC_RMID` uses **reference counting**: it marks the segment "to delete," but the physical RAM is only freed when the last attached process detaches. In this program, the child detaches first (snippet 6), then the parent's detach (snippet 9) drops the reference count to zero, and the kernel frees the RAM. I verified the leak by commenting out the `shmctl` line, running the program twice, and observing `ipcs -m` show two orphaned segments (`nattch = 0`, no `dest` flag). The cleanup command is `ipcrm -m <shmid>`. **In Docker, this demonstration must be captured inside the container's session** — when `--rm` destroys the container, its IPC namespace and all segments in it are freed.

---

## What to capture in the screenshot

**Frame 1:** the prerequisite and run:

```bash
$ touch shmfile
$ ./bin/shm_ipc
parent read: "Hello from child process"
```

**Frame 2:** the segment lifecycle:

```bash
$ ipcs -m          # before — empty or no relevant entries
$ ./bin/shm_ipc    # runs, cleans up via IPC_RMID
$ ipcs -m          # after — still empty (cleanup worked)
```

**Frame 3 (leak demo):** comment out `shmctl(..., IPC_RMID, ...)`, rebuild, run twice, then:

```bash
$ ipcs -m          # two leaked segments
$ ipcrm -m <shmid>  # clean up
$ ipcrm -m <shmid>
$ ipcs -m          # empty again
```

**Capture frame 3 before exiting the container.**

**Frame 4:** the `strace -f -e trace=ipc,clone,wait4` output, with a note pointing at the absence of `read`/`write` syscalls around the `strcpy`.

---

## The list to carry 

- shared memory = two page tables pointing at the **same physical RAM**
- `shmget` creates/fetches a segment; `shmat` maps it; `shmdt` unmaps it
- after `shmat`, **read/write are plain memory operations — no syscalls** — that's the speed advantage over pipes
- `ftok` **requires the file to exist** — `touch shmfile` first; the handout omits this
- **no synchronization** is provided by shared memory — `waitpid` is doing that duty here; real code needs a semaphore or mutex
- `IPC_RMID` **marks for deletion**; actual free happens when the last process detaches (reference counting)
- forgetting `IPC_RMID` **leaks** the segment — visible in `ipcs -m`, cleaned with `ipcrm -m`
- the child inherits the mapping through `fork`, so it doesn't need its own `shmat` (unless `exec` intervenes)
- the handout's child does not call `shmdt` — worth noting as a hygiene issue
- all four System V shared memory calls go through *`ipc(2)`** with a sub-op code
- **Docker caveat:** container has its own IPC namespace — `ipcs -m` shows only its own segments; the leak demo must be captured before `docker run --rm` exits
