#ifndef RW_LOCK_H
#define RW_LOCK_H

/* PQ3: reader-writer lock, writer preference (per handout) */

#include "tl_semaphore.h"

typedef struct rwlock {
    ticket_lock m;
    int active_readers;
    int active_writer;
    int waiting_writers;
} rwlock;

void rwlock_init(rwlock *lock);
void rwlock_acquire_read(rwlock *lock);
void rwlock_release_read(rwlock *lock);
void rwlock_acquire_write(rwlock *lock);
void rwlock_release_write(rwlock *lock);

#endif
