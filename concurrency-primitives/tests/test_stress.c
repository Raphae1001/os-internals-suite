#include "tl_semaphore.h"

#include <pthread.h>
#include <stdio.h>

#define NB 16
#define LOOP 10000

static semaphore sem;

static void *th(void *p)
{
    (void)p;
    int k;
    for (k = 0; k < LOOP; k++) {
        semaphore_wait(&sem);
        semaphore_signal(&sem);
    }
    return NULL;
}

int main(void)
{
    pthread_t t[NB];
    int i;
    int final_val;

    semaphore_init(&sem, 1);

    for (i = 0; i < NB; i++) {
        if (pthread_create(&t[i], NULL, th, NULL) != 0) {
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

    /* assignment asks for an exact final semaphore value */
    ticketlock_acquire(&sem.lock);
    final_val = sem.value;
    ticketlock_release(&sem.lock);

    if (final_val != 1) {
        fprintf(stderr, "bad final value: %d (expected 1)\n", final_val);
        return 1;
    }

    return 0;
}
