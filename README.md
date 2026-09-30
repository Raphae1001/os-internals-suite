# os-internals-suite

Three systems-programming components built in C, each reimplementing a mechanism operating systems normally provide for you — from the primitives up, without relying on the higher-level OS APIs that usually hide this work: no `pthread_mutex`/`sem_t`, no `pthread` for scheduling, no filesystem syscalls beyond raw block I/O.

Built as coursework for the Operating Systems course at Reichman University (all three graded 100/100), then cleaned up and documented as a standalone portfolio repo. Each subdirectory is self-contained and independently buildable, with its own README, implementation, and test suite.

## Components

### [`concurrency-primitives/`](./concurrency-primitives) — synchronization from the ground up
A ticket-lock spinlock, counting semaphore, condition variable, and writer-preference reader-writer lock, each layer built on the one below it (semaphore → ticket lock; condition variable and rw-lock → ticket lock), with no use of `pthread_mutex_t` or the POSIX semaphore API. Verified under real multi-threaded contention: 9 test binaries covering single-thread correctness, concurrent stress, signal/broadcast wakeup semantics, and writer-preference starvation avoidance.

### [`user-level-threads/`](./user-level-threads) — a preemptive thread scheduler in userspace
A user-level threading library with round-robin preemptive scheduling, driven by `SIGVTALRM`/`setitimer` and manual context switching (`sigsetjmp`/`siglongjmp`) over hand-set-up per-thread stacks. Implements spawn, terminate, block/resume, and sleep, with `SIGVTALRM` explicitly masked around every critical section so a timer interrupt can't corrupt scheduler state mid-update. The included `tests/test.c` is the course's own sample test (it passed the grader's full 101-test suite, which isn't included here); `tests/example.c` is a minimal two-thread demo of the raw `sigsetjmp`/`siglongjmp` jump mechanism underneath.

### [`virtual-filesystem/`](./virtual-filesystem) — "OnlyFiles", a block-based filesystem
A 10 MB virtual disk (superblock, block bitmap, 256-inode table, all in 4 KB blocks), addressed entirely through raw `open`/`lseek`/`read`/`write` on a flat file — no dynamic memory allocation anywhere. Implements create/delete/read/write/list over direct-block-only inodes (12 direct pointers/inode, ~48 KB max file size).

## Why these three together

Each one reimplements a different layer of what an OS normally does for you: safe concurrent access to shared state, scheduling execution across competing threads, and persisting data to a block device. Each had to be verified correct under real contention or real I/O, not just compiled — the test suites exercise FIFO/starvation properties, concurrent access patterns, and byte-exact round-trip I/O, not just the happy path. Together they cover mechanisms that most full-stack/product-focused engineering work never touches directly.

## Building

Every component builds standalone with GCC (`-std=gnu17`) and has no dependencies beyond the C standard library and POSIX (`-lpthread` for `concurrency-primitives`'s tests only — the primitives themselves don't link it). All three build and run cleanly with `gcc -Wall -Wextra` (no warnings) on Linux x86_64 and GCC 13.

```bash
# concurrency-primitives
cd concurrency-primitives
gcc -std=gnu17 -Wall -I. -o test_bin tl_semaphore.c cond_var.c rw_lock.c tests/test_unit.c -lpthread
./test_bin

# user-level-threads
cd user-level-threads
gcc -std=gnu17 -Wall -I. -o uthreads_test uthreads.c jump.c tests/test.c
./uthreads_test

# virtual-filesystem
cd virtual-filesystem
gcc -std=gnu17 -Wall -I. -o fs_test fs.c tests/test.c
./fs_test
```

`user-level-threads` also supports AArch64 (`jump.c` has a separate `sigaltstack`/`SIGUSR1`-trampoline implementation for that architecture, since the x86-64 jump-buffer field layout it otherwise relies on isn't portable). See each subdirectory's README for the full list of test binaries and exact build commands.
