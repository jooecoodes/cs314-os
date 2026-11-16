# Activity 1 — File Operations

## The program, piece by piece

The program does six things in order: open for writing, write, close, open for reading, read, close. Each piece below is one of those steps.

---

### Snippet 1 — open the file for writing

```c
int fd = open("example.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
if (fd < 0) { perror("open"); return 1; }
```

**What it means in plain words:** *"Kernel, please open `example.txt` for me. I want to write to it. Create it if it doesn't exist. Empty it if it does."*

**The details:**

- `O_WRONLY` = write only
- `O_CREAT` = create if missing
- `O_TRUNC` = empty it if it already exists
- `0644` = permissions (owner can read/write, everyone else can read)

The three flags are glued together with `|` (bitwise OR) because each one is a single bit in a number. Combining them means "all three at once."

**What comes back:** a small number — the **file descriptor** — or `-1` on failure. The `if (fd < 0)` line is the standard check for failure. `perror("open")` prints a human message like `open: Permission denied` if it fails.

---

### Snippet 2 — write the message

```c
const char *msg = "Hello, System Calls!";
size_t      len = strlen(msg);
ssize_t     done = 0;

while ((size_t)done < len) {
    ssize_t n = write(fd, msg + done, len - (size_t)done);
    if (n < 0) { perror("write"); close(fd); return 1; }
    done += n;
}
```

**What it means in plain words:** *"Kernel, take these bytes and put them in the file. Keep doing it until every byte has gone through."*

**Why the loop?** Because `write` is allowed to write **fewer bytes than you asked for**. It returns how many it actually wrote. If it wrote 10 out of 20, you have 10 left to go. The loop keeps calling `write` on the remaining part until `done` equals `len`.

**The tricky-looking bits:**

- `msg + done` — a pointer that starts at the beginning of the message and moves forward each time
- `len - done` — how many bytes are still left to write
- `done += n` — count what just got written

**The bug in the handout:** it used `write(fd, message, 18)` — hardcoding `18`. The actual message is 20 characters. That's wrong. `strlen(msg)` gives the correct length automatically, even if you change the message later.

---

### Snippet 3 — close the file

```c
if (close(fd) < 0) { perror("close"); return 1; }
```

**What it means in plain words:** *"Kernel, I'm done with this file. You can free my slot."*

Closing does two things: it tells the kernel you're finished, and it **frees the fd number** so the next `open` can reuse it. We'll see that reuse in a moment.

---

### Snippet 4 — open the file for reading

```c
fd = open("example.txt", O_RDONLY);
if (fd < 0) { perror("open"); return 1; }
```

**What it means in plain words:** *"Kernel, open the same file again — but this time I only want to read it."*

`O_RDONLY` = read only. No `O_CREAT`, no `O_TRUNC`, because the file already exists and we don't want to empty it.

**Notice:** we reuse the variable `fd`. The old fd was closed, so its slot is free. The new `open` will probably return the **same number** as before. That's not a coincidence — the kernel hands out the lowest free slot.

---

### Snippet 5 — read the whole file

```c
char    buf[128];
ssize_t total = 0, n;

while ((n = read(fd, buf + total, (size_t)(sizeof(buf) - 1) - (size_t)total)) > 0)
    total += n;
if (n < 0) { perror("read"); close(fd); return 1; }
```

**What it means in plain words:** *"Kernel, keep giving me bytes from the file until there are none left. Put them in `buf`, one after another."*

**How the loop stops:** `read` returns a number. Positive = bytes read. *`0` = end of file.** Negative = error. The loop runs `while (... > 0)`, so it stops the moment `read` returns `0`. That's the whole mechanism.

**Why the loop here too?** Same reason as `write` — `read` can give you fewer bytes than you asked for. It doesn't mean anything's broken; it just means "here's what was available right now."

**The buffer size:** `buf` is 128 bytes, and each read asks for `sizeof(buf) - 1 = 127`. Why leave one byte empty? Because we're about to add a string terminator, and it needs a spot to live. More on that in the next snippet.

---

### Snippet 6 — terminate the string and print it

```c
buf[total] = '\0';
printf("Read from file: %s\n", buf);
```

**What it means in plain words:** *"Put a null terminator at the end of what we read, then print it."*

**Why the terminator?** `read` copies raw bytes. It does **not** add a `'\0'` for you. If you hand a buffer to `printf("%s", ...)` without a terminator, `printf` has no way to know where the data ends — it just keeps reading memory until it happens to hit a zero byte. That's why we set `buf[total] = '\0'` — `total` is how many bytes we actually read, so the terminator goes right after them.

**This is a common bug.** Forget the terminator, get garbage after your text. Worth mentioning in the report.

---

### Snippet 7 — close again

```c
if (close(fd) < 0) { perror("close"); return 1; }
```

Same as snippet 3. Done with the file, free the slot, exit cleanly.

---

## Compilation

```bash
$ gcc -Wall -Wextra -O2 -o bin/file_ops file_ops/file_ops.c
```

No output. The `-Wall -Wextra` flags turn warnings on, so silence means the compiler found nothing to complain about.

---

## Output

```bash
$ ./bin/file_ops
Read from file: Hello, System Calls!
```

```bash
$ cat example.txt
Hello, System Calls!
```

**Screenshot:** *(insert terminal screenshot here)*

---

## Syscalls Involved

| Call in the code | What the kernel runs |
|---|---|
| `open(...)` | `openat(2)` |
| `write(...)` | `write(2)` |
| `read(...)` | `read(2)` |
| `close(...)` | `close(2)` |

**The return values you check for:**

- `open` → fd ≥ 0 on success, −1 on failure
- `read` → bytes read on success, **0 at EOF**, −1 on error
- `write` → bytes written (may be fewer than asked), −1 on error
- `close` → 0 on success, −1 on error

**Trimmed `strace`:**

```bash
$ strace -e trace=openat,read,write,close ./bin/file_ops
openat(AT_FDCWD, "example.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644) = 3
write(3, "Hello, System Calls!", 20) = 20
close(3) = 0
openat(AT_FDCWD, "example.txt", O_RDONLY) = 3
read(3, "Hello, System Calls!", 127) = 20
read(3, "", 107) = 0
close(3) = 0
write(1, "Read from file: Hello, System Calls!\n", 37) = 37
```

**What to notice:**

- The syscall is *`openat`**, not `open`. The `open()` you write is a wrapper; the kernel sees `openat`.
- *`AT_FDCWD`** means "look for the path from the current directory." That's how `"example.txt"` finds the file.
- **fd `3`** appears twice. The first `close(3)` freed it, and the second `open` reused it.
- *`read(3, "", 107) = 0`** — the second read got 0 bytes. That's EOF, and it's why the loop stopped.
- **The final `write(1, ...)` came from `printf`**, not from our code. `printf` buffered the string and only flushed it at program exit. That's why there's one `write`, not one per character.

---

## Observations

**Observation 1 — the file descriptor is just a small number, and it gets reused.**
I expected `open()` to return some kind of object. Instead it returns an integer. It came back as `3` both times because 0, 1, and 2 are taken by stdin, stdout, and stderr. After `close(3)`, the slot was free, so the second `open` reused it. The strace shows this clearly: `= 3` on both `openat` calls with a `close(3)` in between. So an fd is a slot number in a kernel table, not a pointer.

**Observation 2 — `read` and `write` can move fewer bytes than asked.**
The handout's original code assumed `write(fd, msg, 18)` would write all 18 bytes at once. That's not guaranteed. The syscall returns how many bytes it moved, which may be fewer. To test this, I gave the read loop a tiny 5-byte buffer: it returned 5, 5, 5, 5, then 0. The 0 is EOF, which ends the loop. Both loops in the final code handle partial transfers correctly.

**Observation 3 — `read` does not add a `'\0'` for you.**
`read` copies raw bytes but leaves no terminator. Passing the buffer to `printf` without adding `'\0'` makes `printf` read past the valid data and print garbage. That's why the code sets `buf[total] = '\0'` after the loop. The buffer is 128 bytes and reads are capped at 127, so there's always one byte for the terminator.

**Observation 4 — `printf` is a library function, not a system call, and it buffers.**
The strace shows a single `write(1, "...", 37)` at the very end. Not one `write` per `printf`. That's because `printf` collects characters in a userspace buffer and only flushes when the buffer fills or the program exits. So calling `printf` does **not** mean the kernel is involved. This is the key difference between a library function and a system call — the topic of Discussion section 9.2.

**Observation 5 — the handout had a bug: a hardcoded length.**
The reference code used `write(fd, message, 18)` — a hardcoded length. The actual message `"Hello, System Calls!"` is 20 characters. The hardcoded value would silently truncate the message. The correct form is `strlen(message)`, which gives 20. The strace confirms the fix: `write(3, "Hello, System Calls!", 20) = 20`.

---

## What to capture in the screenshot

**Frame 1:** compile + run + `cat example.txt`
**Frame 2:** the strace output

Two frames are fine. Don't try to cram everything into one.

---

## The list to carry

- `read`/`write` can be partial — always loop
- `read` does not add `'\0'` — you must
- the file descriptor is a small reusable number
- `printf` buffers; coded `18` is wrong — `strlen` is correct
