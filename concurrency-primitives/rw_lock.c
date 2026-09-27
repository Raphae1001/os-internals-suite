#include "rw_lock.h"

#include <sched.h>

void rwlock_init(rwlock *lock)
{
    ticketlock_init(&lock->m);
    lock->active_readers = 0;
    lock->active_writer = 0;
    lock->waiting_writers = 0;
}

void rwlock_acquire_read(rwlock *lock)
{
    while (1) {
        ticketlock_acquire(&lock->m);
        /* if a writer is waiting, new readers must not enter */
        if (lock->active_writer == 0 && lock->waiting_writers == 0) {
            lock->active_readers++;
            ticketlock_release(&lock->m);
            return;
        }
        ticketlock_release(&lock->m);
        sched_yield();
    }
}

void rwlock_release_read(rwlock *lock)
{
    ticketlock_acquire(&lock->m);
    lock->active_readers--;
    ticketlock_release(&lock->m);
}

void rwlock_acquire_write(rwlock *lock)
{
    ticketlock_acquire(&lock->m);
    lock->waiting_writers++;
    while (lock->active_readers > 0 || lock->active_writer > 0) {
        ticketlock_release(&lock->m);
        sched_yield();
        ticketlock_acquire(&lock->m);
    }
    lock->waiting_writers--;
    lock->active_writer = 1;
    ticketlock_release(&lock->m);
}

void rwlock_release_write(rwlock *lock)
{
    ticketlock_acquire(&lock->m);
    lock->active_writer = 0;
    ticketlock_release(&lock->m);
}
