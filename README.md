# os-internals-suite

Three systems-programming components built in C, implementing the core mechanisms an operating system provides — from the primitives up — without relying on the OS APIs that normally hide this work: no `pthread_mutex`/`sem_t`, no `pthread`, no filesystem syscalls beyond raw block I/O.

Built as part of the Operating Systems course at Reichman University (all three graded 100/100). Each subdirectory is a self-contained, independently buildable project with its own README, implementation, and test suite.

## Components

### [`concurrency-primitives/`](./concurrency-primitives) — synchronization from the ground up
A ticket-lock spinlock, counting semaphore, condition variable, and writer-preference reader-writer lock — each layer built on the one below it. Proves FIFO acquisition order and bounded waiting rather than assuming them.

### [`user-level-threads/`](./user-level-threads) — a preemptive thread scheduler in userspace
A user-level threading library with round-robin preemptive scheduling, driven by `SIGVTALRM` and manual context switching (`sigsetjmp`/`siglongjmp`). Each thread owns its own stack; preemption, blocking, and sleep are all implemented without any OS thread API. Full test suite (101 tests) passing.

### [`virtual-filesystem/`](./virtual-filesystem) — "OnlyFiles", a block-based filesystem
A 10 MB virtual disk with a superblock, block bitmap, and inode table, addressed entirely through raw `open`/`lseek`/`read`/`write` on a flat file — no dynamic memory allocation, no OS filesystem calls. Implements create/delete/read/write/list over direct-block inodes.

## Why these three together

Each one reimplements a different layer of what an OS normally does for you — safe concurrent access to shared state, scheduling execution across competing threads, and persisting data to a block device — and each had to be verified correct under real contention or real I/O, not just compiled. Together they cover the mechanisms most full-stack/product-focused engineering work never has to touch directly.

## Building

Every component builds standalone with GCC (`-std=gnu17`); see each subdirectory's README for exact commands. No external dependencies beyond the C standard library and POSIX (`-lpthread` for `concurrency-primitives`).
