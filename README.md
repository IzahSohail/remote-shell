## Remote Shell (Client/Server)

A minimal multi-client remote shell that accepts textual commands from clients over TCP, schedules them, executes on the server, and streams results back.

### Features
- **Concurrent clients**: Each client is handled in its own thread.
- **Task scheduler**: Shell commands are queued and executed with a simple, fair approach that prioritizes shell tasks.
- **Pipes and redirection**: Supports `|`, `<`, `>`, `2>`, and `2>&1`, including combinations across multiple commands.
- **Streaming output**: Server streams command output to the client in real time; completion is marked by `__TASK_DONE__`.
- **Built-in demo task**: `demo N` simulates a CPU burst with N iterations, streaming one line per second.

### Architecture Overview
- `src/server.c`: TCP server, accepts clients, parses input, enqueues tasks.
- `src/scheduler.c`: In-memory task queue and scheduler loop; executes shell commands and the demo task; streams results.
- `src/executor.c`: Execution helpers implementing pipes and redirections.
- `src/parser.c`: Tokenization with double-quote support for arguments.
- `src/myshell.c`: Simple client sending commands and printing streamed results until completion marks.
- `src/demo.c`: Standalone demo program; the server also simulates `demo` via the scheduler without invoking this binary.

### Protocol
- Clients send a single line command per request.
- Server streams command output as produced.
- When a task completes, server sends the marker: `__TASK_DONE__`.
- Special command: `exit` disconnects the client and clears its queued tasks.

### Supported Commands
- `exit`: Disconnects the client.
- `demo N` or `./demo N`: Enqueues a simulated task with N iterations; emits lines like `Demo i/N-1` once per second.
- Any external command resolvable by `execvp`, including:
  - Simple: `ls`, `pwd`, `echo hello`, `cat file`
  - Pipes: `cat file | grep x`, `ls -l | wc -l`, multi-stage pipelines
  - Redirection: `< in.txt`, `> out.txt`, `2> err.txt`, `2>&1`
  - Pipes + redirections combined

Notes:
- Shell built-ins (e.g., `cd`, `export`, `alias`) are not implemented and will not behave as in an interactive shell.

### Build
Requirements: POSIX toolchain (clang/gcc, make), pthreads.

```bash
make
```

Artifacts (ignored by Git): `server`, `myshell`, `demo`, objects in `obj/`.

### Run
1) Start the server (default port 8081):
```bash
./server
```

2) In another terminal, start the client and connect:
```bash
./myshell
```

You should see a prompt `$ `. Type commands and press Enter.

### Quick Test Matrix
- Basic external commands
  - `echo hello` → `hello`
  - `pwd` → server working directory
  - `ls` → server directory listing
- Pipes
  - `printf "alpha\nfoo\n" | grep foo` → `foo`
  - `ls -1 | wc -l` → count of entries
- Redirections
  - `echo hi > out.txt` → no client output; `cat out.txt` shows `hi`
  - `grep x < out.txt` → empty output (if `x` not present)
  - `ls nofile 2> err.txt` → no client output; `cat err.txt` shows error message
- Combined
  - `cat < out.txt | wc -c > count.txt` → no client output; `cat count.txt` shows a number
- Demo
  - `demo 3` → `Demo 0/2`, `Demo 1/2`, `Demo 2/2`
- Disconnect
  - `exit` → client disconnects

### Configuration
- Port is defined in `src/server.c` as `#define PORT 8081`.
- Increase `BUFFER_SIZE` in `src/server.c`/`src/scheduler.c` if needed for larger outputs.

### Development
- Clean artifacts:
```bash
make clean
```

- Coding guidelines:
  - Avoid shell built-ins in commands; prefer external programs.
  - Quote arguments with spaces using double quotes, e.g., `echo "hello world"`.

### Repository Layout
```
include/      Public headers
src/          Server, client, scheduler, executor, parser, demo
Makefile      Build targets
.gitignore    Ignore list for binaries and artifacts
```

### Limitations and Notes
- No job control or interactive TTY allocation.
- Commands execute in child processes via `execvp`; environment and CWD are the server process’s.
- Built-ins like `cd` affect only a spawned child and thus have no persistent effect.