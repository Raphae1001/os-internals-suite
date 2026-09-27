#include "cond_var.h"

#include <sched.h>
#include <stdatomic.h>
#include <stddef.h>

struct WaitNode {
    atomic_int triggered;
    struct WaitNode *next;
};

static void enqueue(condition_variable *cv, struct WaitNode *node)
{
    node->next = NULL;
    if (cv->tail == NULL) {
        cv->head = node;
        cv->tail = node;
    } else {
        cv->tail->next = node;
        cv->tail = node;
    }
}

static struct WaitNode *pop_one(condition_variable *cv)
{
    struct WaitNode *n = cv->head;
    if (n == NULL) {
        return NULL;
    }
    cv->head = n->next;
    if (cv->head == NULL) {
        cv->tail = NULL;
    }
    n->next = NULL;
    return n;
}

void condition_variable_init(condition_variable *cv)
{
    ticketlock_init(&cv->qlock);
    cv->head = NULL;
    cv->tail = NULL;
}

void condition_variable_wait(condition_variable *cv, ticket_lock *ext_lock)
{
    struct WaitNode node;

    atomic_init(&node.triggered, 0);

    ticketlock_acquire(&cv->qlock);
    enqueue(cv, &node);
    ticketlock_release(&cv->qlock);

    ticketlock_release(ext_lock);

    while (atomic_load(&node.triggered) == 0) {
        sched_yield();
    }

    ticketlock_acquire(ext_lock);
}

void condition_variable_signal(condition_variable *cv)
{
    struct WaitNode *n;

    ticketlock_acquire(&cv->qlock);
    n = pop_one(cv);
    ticketlock_release(&cv->qlock);

    if (n != NULL) {
        atomic_store(&n->triggered, 1);
    }
}

void condition_variable_broadcast(condition_variable *cv)
{
    struct WaitNode *pending;
    struct WaitNode *tmp;

    ticketlock_acquire(&cv->qlock);
    pending = cv->head;
    cv->head = NULL;
    cv->tail = NULL;
    ticketlock_release(&cv->qlock);

    while (pending != NULL) {
        tmp = pending->next;
        atomic_store(&pending->triggered, 1);
        pending = tmp;
    }
}
