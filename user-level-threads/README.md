# preemptive-user-threads-c

A **user-level threading library** in C, implementing preemptive round-robin scheduling entirely in userspace — no OS thread APIs (`pthread`, `clone`) are used. Each thread gets its own stack and execution context, switched via `sigsetjmp`/`siglongjmp`, with preemption driven by a virtual-timer signal (`SIGVTALRM`).

Built as part of the Operating Systems course at Reichman University; the full test suite (101 unit tests, provided by the course grader) passed at 100/100. This repository contains the implementation I wrote plus the course-provided context switching support code; course instructions are not included.

## What it implements

- **Thread lifecycle**: `uthread_spawn`, `uthread_terminate`, `uthread_block`, `uthread_resume`, `uthread_sleep`
- **Preemptive round-robin scheduling**: a fixed time quantum (configured in microseconds) is enforced via `setitimer(ITIMER_VIRTUAL, ...)` and `SIGVTALRM`; a thread that doesn't yield voluntarily is preempted automatically
- **Context switching**: each thread's execution state (stack pointer, program counter, signal mask) is saved and restored with `sigsetjmp`/`siglongjmp`, using manual stack setup (`setup_thread`) rather than any library-provided coroutine mechanism
- **Signal-safety**: `SIGVTALRM` is explicitly blocked/unblocked around every critical section that touches scheduler state, to avoid a timer interrupt corrupting the ready queue mid-update
- **State tracking**: total quantums elapsed, and per-thread quantum count, exposed via `uthread_get_total_quantums` / `uthread_get_quantums`

## Structure

```
uthreads.h / uthreads.c   Public API + full scheduler implementation
jump.c / jump.h           Low-level context setup (sigsetjmp/siglongjmp, arch-specific stack pointer mangling; x86-64 and AArch64) — course-provided support code
tests/test.c              Course-provided sample test (scheduling order, sleep/wake)
tests/example.c           Minimal two-thread cooperative demo showing the underlying jump mechanism directly
```

## Building & running

```bash
gcc -std=gnu17 -Wall -I. -o uthreads_test uthreads.c jump.c tests/test.c
./uthreads_test
```

## Why this is a meaningful systems project

Implementing preemptive scheduling in userspace forces you to deal with the same core problems a kernel scheduler deals with — safe context switching, a ready queue that must stay consistent even when interrupted mid-mutation by an async signal, and manual stack/register management — without any of the OS-provided safety nets a normal multithreaded program relies on. It's a direct, hands-on way to understand what `pthread` is actually doing underneath.
