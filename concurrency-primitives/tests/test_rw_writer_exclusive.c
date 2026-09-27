#include "rw_lock.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

#define NB 8

static rwlock lk;
static atomic_int readers_in_cs;
static atomic_int writers_in_cs;

static void *writer(void *p)
{
    (void)p;
    rwlock_acquire_write(&lk);
    if (atomic_load(&readers_in_cs) != 0) {
        fprintf(stderr, "bug: reader active during write lock\n");
        atomic_store(&writers_in_cs, -1);
        rwlock_release_write(&lk);
        return NULL;
    }
    if (atomic_load(&writers_in_cs) != 0) {
        fprintf(stderr, "bug: overlapping writers\n");
        atomic_store(&writers_in_cs, -1);
        rwlock_release_write(&lk);
        return NULL;
    }
    atomic_store(&writers_in_cs, 1);
    for (volatile int i = 0; i < 500000; i++) {
    }
    if (atomic_load(&readers_in_cs) != 0) {
        fprintf(stderr, "bug: reader entered mid write section\n");
        atomic_store(&writers_in_cs, -1);
    }
    atomic_store(&writers_in_cs, 0);
    rwlock_release_write(&lk);
    return NULL;
}

static void *reader(void *p)
{
    int k;

    (void)p;
    for (k = 0; k < 2000; k++) {
        rwlock_acquire_read(&lk);
        atomic_fetch_add(&readers_in_cs, 1);
        for (volatile int i = 0; i < 1000; i++) {
        }
        atomic_fetch_sub(&readers_in_cs, 1);
        rwlock_release_read(&lk);
    }
    return NULL;
}

int main(void)
{
    pthread_t w;
    pthread_t r[NB];
    int i;

    rwlock_init(&lk);
    atomic_init(&readers_in_cs, 0);
    atomic_init(&writers_in_cs, 0);

    if (pthread_create(&w, NULL, writer, NULL) != 0) {
        fprintf(stderr, "pthread_create (writer) failed\n");
        return 1;
    }

    for (i = 0; i < NB; i++) {
        if (pthread_create(&r[i], NULL, reader, NULL) != 0) {
            fprintf(stderr, "pthread_create (reader) failed\n");
            return 1;
        }
    }

    if (pthread_join(w, NULL) != 0) {
        fprintf(stderr, "pthread_join (writer) failed\n");
        return 1;
    }

    for (i = 0; i < NB; i++) {
        if (pthread_join(r[i], NULL) != 0) {
            fprintf(stderr, "pthread_join (reader) failed\n");
            return 1;
        }
    }

    if (atomic_load(&writers_in_cs) < 0) {
        return 1;
    }

    return 0;
}
