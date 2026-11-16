# IPC & Process Examples

Simple C programs demonstrating common Unix concepts:

| Binary       | Topic                          |
|--------------|--------------------------------|
| `file_ops`   | Basic file I/O                 |
| `proc_mgmt`  | Process creation / management  |
| `pipe_ipc`   | Inter-process communication via pipes |
| `shm_ipc`    | Shared-memory IPC              |

## Build

```bash
make                    # compiles all four into ./bin/
```

## Run the examples

```bash
./bin/file_ops
./bin/proc_mgmt
./bin/pipe_ipc
```

## Shared-memory example

```bash
make run-shm            # touches shmfile, runs ./bin/shm_ipc
ipcs -m                 # verify no leaked segment
```

`ipcs -m` lists active shared-memory segments. After a clean run you should see none left behind by this program.

## Clean up

```bash
make clean              # removes ./bin/, example.txt, shmfile
```
