# Makefile — project root
# Compiles each activity's source into ./bin/<name>.

CC      = gcc
CFLAGS  = -Wall -Wextra -O2
BIN     = bin
TARGETS = $(BIN)/file_ops $(BIN)/proc_mgmt $(BIN)/pipe_ipc $(BIN)/shm_ipc

all: $(TARGETS)

$(BIN):
	mkdir -p $(BIN)

$(BIN)/file_ops:  file_ops/file_ops.c   | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN)/proc_mgmt: proc_mgmt/proc_mgmt.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN)/pipe_ipc:  pipe_ipc/pipe_ipc.c   | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN)/shm_ipc:   shm_ipc/shm_ipc.c     | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

run-shm: $(BIN)/shm_ipc
	touch shmfile
	./$(BIN)/shm_ipc

clean:
	rm -rf $(BIN) example.txt shmfile

.PHONY: all run-shm clean
