#include "rw_lock.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

#define NB_R 12
#define NB_W 4
#define IT_R 8000
#define IT_W 2000

static rwlock lk;
static atomic_int writer_acquires[NB_W];

static void *reader(void *p)
{
    int i;

    (void)p;
    for (i = 0; i < IT_R; i++) {
        rwlock_acquire_read(&lk);
        for (volatile int j = 0; j < 50; j++) {
        }
        rwlock_release_read(&lk);
    }
    return NULL;
}

static void *writer(void *p)
{
    int id = *(int *)p;
    int i;

    for (i = 0; i < IT_W; i++) {
        rwlock_acquire_write(&lk);
        atomic_fetch_add(&writer_acquires[id], 1);
        for (volatile int j = 0; j < 200; j++) {
        }
        rwlock_release_write(&lk);
    }
    return NULL;
}

int main(void)
{
    pthread_t tr[NB_R];
    pthread_t tw[NB_W];
    static int ids[NB_W];
    int i;
    int w;

    rwlock_init(&lk);
    for (w = 0; w < NB_W; w++) {
        atomic_init(&writer_acquires[w], 0);
        ids[w] = w;
    }

    for (i = 0; i < NB_R; i++) {
        if (pthread_create(&tr[i], NULL, reader, NULL) != 0) {
            fprintf(stderr, "pthread_create (reader) failed\n");
            return 1;
        }
    }

    for (w = 0; w < NB_W; w++) {
        if (pthread_create(&tw[w], NULL, writer, &ids[w]) != 0) {
            fprintf(stderr, "pthread_create (writer) failed\n");
            return 1;
        }
    }

    for (i = 0; i < NB_R; i++) {
        if (pthread_join(tr[i], NULL) != 0) {
            fprintf(stderr, "pthread_join (reader) failed\n");
            return 1;
        }
    }

    for (w = 0; w < NB_W; w++) {
        if (pthread_join(tw[w], NULL) != 0) {
            fprintf(stderr, "pthread_join (writer) failed\n");
            return 1;
        }
    }

    for (w = 0; w < NB_W; w++) {
        if (atomic_load(&writer_acquires[w]) < 100) {
            fprintf(stderr, "writer %d got too few write acquisitions (%d)\n", w,
                    atomic_load(&writer_acquires[w]));
            return 1;
        }
    }

    return 0;
}
