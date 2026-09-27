#include "cond_var.h"

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdatomic.h>

#define NB 16
#define ROUNDS 500

static ticket_lock ext_lock;
static condition_variable cv;
static atomic_int released;
static atomic_int round_id; /* sync workers between broadcast rounds */

static void *worker(void *p)
{
    (void)p;
    int k;
    int g;

    for (k = 0; k < ROUNDS; k++) {
        g = atomic_load(&round_id);

        ticketlock_acquire(&ext_lock);
        condition_variable_wait(&cv, &ext_lock);
        atomic_fetch_add(&released, 1);
        ticketlock_release(&ext_lock);

        while (atomic_load(&round_id) == g) {
            sched_yield();
        }
    }

    return NULL;
}

int main(void)
{
    pthread_t t[NB];
    int i;
    int r;
    int guard;

    ticketlock_init(&ext_lock);
    condition_variable_init(&cv);
    atomic_init(&released, 0);
    atomic_init(&round_id, 0);

    for (i = 0; i < NB; i++) {
        if (pthread_create(&t[i], NULL, worker, NULL) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            return 1;
        }
    }

    for (r = 0; r < ROUNDS; r++) {
        atomic_store(&released, 0);
        guard = 0;

        while (atomic_load(&released) < NB) {
            condition_variable_broadcast(&cv);
            sched_yield();
            guard++;
            if (guard > 100000000) {
                fprintf(stderr, "timeout waiting for round %d\n", r);
                return 1;
            }
        }

        atomic_fetch_add(&round_id, 1);
    }

    for (i = 0; i < NB; i++) {
        if (pthread_join(t[i], NULL) != 0) {
            fprintf(stderr, "pthread_join failed\n");
            return 1;
        }
    }

    return 0;
}
