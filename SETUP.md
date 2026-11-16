# Setup

How to get the Docker environment running. Once you're inside the container, see [`DEMO.md`](DEMO.md) for the commands to build and run each activity.

---

## Prerequisites

- **Docker installed and running.**
  - Linux (Arch, Ubuntu, etc.): install `docker` from your package manager, then `sudo systemctl start docker`. Consider adding yourself to the `docker` group: `sudo usermod -aG docker $USER` (then log out and back in).
  - macOS: install **Docker Desktop** from <https://www.docker.com/products/docker-desktop/>.
  - Windows: install **Docker Desktop** and make sure the **WSL 2 backend** is enabled (default). WSL 2 runs a real Linux kernel in a lightweight VM — that's what makes the syscalls in this lab behave identically to Linux.

- **This repository cloned locally.** All commands below assume you are in the **project root** — the directory containing `Dockerfile`, `Makefile`, and the `file_ops/`, `pipe_ipc/`, `proc_mgmt/`, `shm_ipc/` folders.

- **No `gcc` required on your host.** The container provides it.

---

## The Dockerfile

Already in the repo root:

```dockerfile
FROM debian:stable-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        gcc \
        libc6-dev \
        strace \
        make \
        util-linux \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /lab

CMD ["/bin/bash"]
```

Notes:
- `gcc` + `libc6-dev` — compiler and C standard library headers.
- `strace` — to capture syscall evidence for the report.
- `make` — to run the project Makefile.
- `util-linux` — provides `ipcs` / `ipcrm`, needed for the shared memory activity.

No source is copied into the image. The project directory is **bind-mounted** at run time, so edits on the host are visible immediately inside the container.

---

## Build the image

From the project root:

```bash
docker build -t syscall-lab .
```

`-t syscall-lab` names the image. You only need to do this once (or again after you change the `Dockerfile`).

Verify it exists:

```bash
docker images | grep syscall-lab
```

---

## Run the container

Each command below mounts the **current directory** as `/lab` inside the container, so the source and `bin/` directory are shared between host and container. Use the one for your shell.

### Linux (bash / zsh / fish on Arch, Ubuntu, etc.)

```bash
docker run -it --rm -v "$PWD":/lab syscall-lab
```

### macOS (bash / zsh)

```bash
docker run -it --rm -v "$PWD":/lab syscall-lab
```

### Windows — PowerShell

```powershell
docker run -it --rm -v "${PWD}:/lab" syscall-lab
```

### Windows — Git Bash

```bash
MSYS_NO_PATHCONV=1 docker run -it --rm -v "$PWD":/lab syscall-lab
```

(`MSYS_NO_PATHCONV=1` stops Git Bash from rewriting the path before passing it to Docker.)

### Windows — WSL 2 shell (Ubuntu, etc.)

```bash
docker run -it --rm -v "$PWD":/lab syscall-lab
```

(Works exactly like the Linux case, since WSL 2 **is** Linux.)

**Flags explained:**

| Flag | Meaning |
|---|---|
| `-it` | interactive + TTY — gives you a usable shell |
| `--rm` | delete the container when you exit (image stays) |
| `-v "$PWD":/lab` | bind-mount the current host directory at `/lab` in the container |

You'll land in a root shell at `/lab`.

---

## What to do inside the container

**See [`DEMO.md`](DEMO.md).** It has the `make`, `./bin/...`, and `make clean` commands for each activity.

Short version: run `make`, then `./bin/file_ops`, `./bin/proc_mgmt`, `./bin/pipe_ipc`, and `make run-shm`.

---

## Exit the container

```bash
exit
```

`--rm` means the container is destroyed on exit. Your source and `bin/` stay on the host (they live in the bind mount), so nothing is lost.

---

## Clean up

Remove the build artifacts on the host:

```bash
make clean
```

Remove the Docker image (optional — you can rebuild later with `docker build -t syscall-lab .`):

```bash
docker rmi syscall-lab
```

---

## Troubleshooting

**`docker: command not found`**
Docker isn't installed or isn't on `PATH`. See Prerequisites.

**`Cannot connect to the Docker daemon`**
The daemon isn't running. Linux: `sudo systemctl start docker`. macOS / Windows: open Docker Desktop.

**`permission denied while trying to connect to the Docker daemon socket` (Linux)**
You're not in the `docker` group. Either prefix with `sudo`, or run `sudo usermod -aG docker $USER` and re-login.

**Windows: `-v "${PWD}:/lab"` fails with a path error**
Make sure you're in **PowerShell** (not cmd.exe) and that Docker Desktop's **WSL 2 backend** is enabled. If the path contains spaces, the quotes around `${PWD}` are important.

**Inside the container: `ipcs: command not found`**
You built the image before `util-linux` was added to the Dockerfile. Rebuild: `docker build -t syscall-lab .`

**The `shm_ipc` program fails with `ftok: No such file or directory`**
`ftok` requires the file `shmfile` to exist in the current working directory. The Makefile's `run-shm` target creates it via `touch shmfile`; if you run `./bin/shm_ipc` directly, `touch shmfile` first.

**`make: *** No rule to make target 'bin/file_ops'`**
You're not in the project root. `cd` to the directory containing the `Makefile`.
