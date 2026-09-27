# concurrency-primitives-c

A small concurrency library built from the ground up in C: a **ticket-lock spinlock**, a **counting semaphore**, a **condition variable**, and a **writer-preference reader-writer lock** — each layer implemented on top of the one below it, with no use of `pthread_mutex_t` or the POSIX semaphore API.

Built as part of the Operating Systems course at Reichman University (graded 100/100). This repository contains only the implementation and test files I wrote; course-provided instructions are not included.

## Why build this instead of using `pthread_mutex`/`sem_t`?

The point of the exercise — and the reason it's worth showing — is understanding *how* these primitives work under contention, not just calling them. Each layer had to be proven correct against a specific property:

- **Ticket lock**: guarantees FIFO acquisition order and bounded waiting (no starvation), unlike a plain test-and-set spinlock.
- **Semaphore**: built directly on the ticket lock, not on top of an OS semaphore.
- **Condition variable**: implements the classic `wait(lock)` / `signal()` / `broadcast()` interface with a FIFO wait queue, avoiding the lost-wakeup and spurious-wakeup pitfalls.
- **Reader-writer lock**: writer-preference policy (a waiting writer blocks new readers from acquiring, to avoid writer starvation under heavy read load) — the harder of the two standard RW-lock policies to get right.

## Structure

```
tl_semaphore.h / .c   Ticket lock + counting semaphore
cond_var.h / .c       Condition variable (built on the ticket lock)
rw_lock.h / .c        Writer-preference reader-writer lock
tests/                Unit, concurrent, and stress tests for each component
```

## Building & running the tests

```bash
gcc -std=gnu17 -Wall -I. -o test_bin tl_semaphore.c cond_var.c rw_lock.c tests/<test_file>.c -lpthread
./test_bin
```

Each test in `tests/` is self-contained; e.g.:

```bash
gcc -std=gnu17 -Wall -I. -o rw_stress tl_semaphore.c cond_var.c rw_lock.c tests/test_rw_writer_preference_stress.c -lpthread
./rw_stress
```

## Test coverage

- `test_unit.c`, `test_concurrent.c`, `test_stress.c` — ticket lock / semaphore correctness under single- and multi-threaded contention
- `test_cv_unit.c`, `test_cv_signal.c`, `test_cv_broadcast_stress.c` — condition variable wait/signal/broadcast semantics, including broadcast stress with many waiters
- `test_rw_readers_parallel.c`, `test_rw_writer_exclusive.c`, `test_rw_writer_preference_stress.c` — concurrent readers, writer mutual exclusion, and writer-preference under load

All tests pass (verified with GCC 13, `-std=gnu17`, Linux x86_64).
