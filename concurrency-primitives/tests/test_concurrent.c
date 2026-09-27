#include "tl_semaphore.h"

#include <pthread.h>
#include <stdio.h>

#define NB 8

static semaphore sem;

static void *th(void *p)
{
    (void)p;
    semaphore_wait(&sem);
    return NULL;
}

int main(void)
{
    pthread_t t[NB];
    int i;

    semaphore_init(&sem, 0);

    for (i = 0; i < NB; i++) {
        if (pthread_create(&t[i], NULL, th, NULL) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            return 1;
        }
    }

    for (i = 0; i < NB; i++) {
        semaphore_signal(&sem);
    }

    for (i = 0; i < NB; i++) {
        if (pthread_join(t[i], NULL) != 0) {
            fprintf(stderr, "pthread_join failed\n");
            return 1;
        }
    }

    return 0;
}
