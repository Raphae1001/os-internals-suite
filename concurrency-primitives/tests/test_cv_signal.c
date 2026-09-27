#include "cond_var.h"

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdatomic.h>

#define NB 8

static ticket_lock ext_lock;
static condition_variable cv;
static atomic_int released;

static void *worker(void *p)
{
    (void)p;
    ticketlock_acquire(&ext_lock);
    condition_variable_wait(&cv, &ext_lock);
    atomic_fetch_add(&released, 1);
    ticketlock_release(&ext_lock);
    return NULL;
}

int main(void)
{
    pthread_t t[NB];
    int i;
    int target;

    ticketlock_init(&ext_lock);
    condition_variable_init(&cv);
    atomic_init(&released, 0);

    for (i = 0; i < NB; i++) {
        if (pthread_create(&t[i], NULL, worker, NULL) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            return 1;
        }
    }

    /* each signal wakes exactly one waiter */
    for (target = 1; target <= NB; target++) {
        while (atomic_load(&released) < target) {
            condition_variable_signal(&cv);
            sched_yield();
        }
    }

    for (i = 0; i < NB; i++) {
        if (pthread_join(t[i], NULL) != 0) {
            fprintf(stderr, "pthread_join failed\n");
            return 1;
        }
    }

    if (atomic_load(&released) != NB) {
        fprintf(stderr, "released count mismatch\n");
        return 1;
    }

    return 0;
}
