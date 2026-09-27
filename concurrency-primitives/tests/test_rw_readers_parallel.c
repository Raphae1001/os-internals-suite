#include "rw_lock.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

#define NB 8

static rwlock lk;
static atomic_int readers_inside;
static atomic_int peak_inside;

static void *reader(void *p)
{
    int x;
    int m;

    (void)p;
    rwlock_acquire_read(&lk);
    x = atomic_fetch_add(&readers_inside, 1) + 1;
    for (;;) {
        m = atomic_load(&peak_inside);
        if (x <= m) {
            break;
        }
        if (atomic_compare_exchange_weak(&peak_inside, &m, x)) {
            break;
        }
    }
    for (volatile int i = 0; i < 200000; i++) {
    }
    atomic_fetch_sub(&readers_inside, 1);
    rwlock_release_read(&lk);
    return NULL;
}

int main(void)
{
    pthread_t t[NB];
    int i;

    rwlock_init(&lk);
    atomic_init(&readers_inside, 0);
    atomic_init(&peak_inside, 0);

    for (i = 0; i < NB; i++) {
        if (pthread_create(&t[i], NULL, reader, NULL) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            return 1;
        }
    }

    for (i = 0; i < NB; i++) {
        if (pthread_join(t[i], NULL) != 0) {
            fprintf(stderr, "pthread_join failed\n");
            return 1;
        }
    }

    if (atomic_load(&peak_inside) < 2) {
        fprintf(stderr, "not enough concurrent readers (peak=%d)\n", atomic_load(&peak_inside));
        return 1;
    }

    return 0;
}
