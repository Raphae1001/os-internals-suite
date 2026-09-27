/*
 * Operating Systems - Exercise 2
 *
 * User-level threads implementation.
 * Round-robin scheduling using SIGVTALRM.
 */

#include "uthreads.h"

/*  Global state                                                       */

static thread_t threads[MAX_THREAD_NUM];
static char     stacks[MAX_THREAD_NUM][STACK_SIZE];  /* stacks[0] unused (main uses process stack) */
static int      blocked_flag[MAX_THREAD_NUM];        /* 1 if thread was explicitly blocked */

/* Ready queue: circular FIFO of TIDs, capacity MAX_THREAD_NUM. */
static int ready_queue[MAX_THREAD_NUM];
static int ready_head  = 0;
static int ready_tail  = 0;
static int ready_count = 0;

static int running_tid    = -1;
static int total_quantums =  0;
static int quantum_usecs  =  0;

static sigset_t vtalrm_mask;

/*  Ready queue helpers                                                */

static void ready_enqueue(int tid)
{
    ready_queue[ready_tail] = tid;
    ready_tail = (ready_tail + 1) % MAX_THREAD_NUM;
    ready_count++;
}

static int ready_dequeue(void)
{
    if (ready_count == 0) return -1;
    int tid = ready_queue[ready_head];
    ready_head = (ready_head + 1) % MAX_THREAD_NUM;
    ready_count--;
    return tid;
}

/* Remove a specific TID from anywhere in the queue. */
static void ready_remove(int tid)
{
    int tmp[MAX_THREAD_NUM];
    int n = 0;
    for (int i = 0; i < ready_count; i++)
    {
        int idx = (ready_head + i) % MAX_THREAD_NUM;
        if (ready_queue[idx] != tid)
            tmp[n++] = ready_queue[idx];
    }
    ready_head  = 0;
    ready_tail  = n;
    ready_count = n;
    for (int i = 0; i < n; i++)
        ready_queue[i] = tmp[i];
}

/*  Signal masking                                                     */

static void block_timer(void)
{
    if (sigprocmask(SIG_BLOCK, &vtalrm_mask, NULL) != 0)
    {
        fprintf(stderr, "system error: masking failed\n");
        exit(1);
    }
}

static void unblock_timer(void)
{
    if (sigprocmask(SIG_UNBLOCK, &vtalrm_mask, NULL) != 0)
    {
        fprintf(stderr, "system error: masking failed\n");
        exit(1);
    }
}

/*  Virtual timer                                                      */

/* Arm a fresh one-shot quantum. Re-called on every context switch. */
static void reset_timer(void)
{
    struct itimerval tv;
    tv.it_value.tv_sec     = quantum_usecs / 1000000;
    tv.it_value.tv_usec    = quantum_usecs % 1000000;
    tv.it_interval.tv_sec  = 0;
    tv.it_interval.tv_usec = 0;
    if (setitimer(ITIMER_VIRTUAL, &tv, NULL) != 0)
    {
        fprintf(stderr, "system error: setitimer failed\n");
        exit(1);
    }
}

/*  Sleep wakeup                                                       */

/* Wake sleeping threads whose sleep period has expired. */
static void wake_sleeping_threads(void)
{
    for (int i = 0; i < MAX_THREAD_NUM; i++)
    {
        thread_t *t = &threads[i];
        if (t->state != THREAD_BLOCKED || t->sleep_until == 0)
            continue;

        if (t->sleep_until <= total_quantums)
        {
            t->sleep_until = 0;
            if (!blocked_flag[i])
            {
                t->state = THREAD_READY;
                ready_enqueue(i);
            }
            /* if still explicitly blocked, stays BLOCKED until resumed */
        }
    }
}

/*  Context switching                                                  */

void setup_thread(int tid, char *stack, thread_entry_point entry_point)
{
    setup_jmpbuff(&threads[tid].env, stack, STACK_SIZE, entry_point);
}

/*
 * Save current thread, restore next thread.
 * sigsetjmp returns 0 on the save path (we jump away to next).
 * It returns non-zero when this thread is later resumed.
 */
void context_switch(thread_t *current, thread_t *next)
{
    if (sigsetjmp(current->env, 1) == 0)
        siglongjmp(next->env, 1);
}

/*
 * Run next_tid: update its counters, reset the timer, then switch.
 * Caller must have already incremented total_quantums and updated
 * the current thread's state. SIGVTALRM must be blocked on entry.
 */
static void switch_to_next_counted(thread_t *current, int next_tid)
{
    thread_t *next = &threads[next_tid];

    next->quantums++;
    next->state = THREAD_RUNNING;
    running_tid = next_tid;

    reset_timer();

    if (sigsetjmp(current->env, 1) == 0)
    {
        unblock_timer();
        siglongjmp(next->env, 1);
    }
    /* resumed: current is running again */
    unblock_timer();
}

/*  Timer signal handler                                               */

/* Timer interrupt handler. */
void timer_handler(int signum)
{
    (void)signum;
    block_timer();

    thread_t *current = &threads[running_tid];

    total_quantums++;
    wake_sleeping_threads();

    current->state = THREAD_READY;
    ready_enqueue(running_tid);

    int next_tid = ready_dequeue();

    if (next_tid == -1)
    {
        /* Only one thread alive; just continue running it. */
        current->state = THREAD_RUNNING;
        current->quantums++;
        reset_timer();
        unblock_timer();
        return;
    }

    switch_to_next_counted(current, next_tid);
}

/*  Voluntary scheduler entry                                          */

/*
 * Called when the running thread voluntarily gives up the CPU
 * (block / sleep / terminate self). Caller must have already set the
 * current thread's state away from RUNNING, and SIGVTALRM must be blocked.
 */
void schedule_next(void)
{
    thread_t *current = &threads[running_tid];

    total_quantums++;
    wake_sleeping_threads();

    int next_tid = ready_dequeue();
    if (next_tid == -1)
        return;  /* no runnable threads; should not happen in valid programs */

    switch_to_next_counted(current, next_tid);
}

/*  Public API                                                         */

int uthread_init(int quantum_usecs_arg)
{
    if (quantum_usecs_arg <= 0)
    {
        fprintf(stderr, "thread library error: quantum_usecs must be positive\n");
        return -1;
    }

    quantum_usecs = quantum_usecs_arg;

    for (int i = 0; i < MAX_THREAD_NUM; i++)
    {
        threads[i].tid         = i;
        threads[i].state       = THREAD_UNUSED;
        threads[i].quantums    = 0;
        threads[i].sleep_until = 0;
        threads[i].entry       = NULL;
        blocked_flag[i]        = 0;
    }

    ready_head  = 0;
    ready_tail  = 0;
    ready_count = 0;

    sigemptyset(&vtalrm_mask);
    sigaddset(&vtalrm_mask, SIGVTALRM);

    /* Set up main thread (tid 0). It is already running; no stack setup needed. */
    threads[0].state    = THREAD_RUNNING;
    threads[0].quantums = 1;
    running_tid         = 0;
    total_quantums      = 1;

    struct sigaction sa;
    sa.sa_handler = timer_handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGVTALRM);
    sa.sa_flags = 0;
    if (sigaction(SIGVTALRM, &sa, NULL) != 0)
    {
        fprintf(stderr, "system error: sigaction failed\n");
        exit(1);
    }

    reset_timer();
    return 0;
}

int uthread_spawn(thread_entry_point entry_point)
{
    if (entry_point == NULL)
    {
        fprintf(stderr, "thread library error: entry_point is NULL\n");
        return -1;
    }

    block_timer();

    /* Find the smallest available TID (>= 1; 0 is reserved for main). */
    int tid = -1;
    for (int i = 1; i < MAX_THREAD_NUM; i++)
    {
        if (threads[i].state == THREAD_UNUSED)
        {
            tid = i;
            break;
        }
    }

    if (tid == -1)
    {
        fprintf(stderr, "thread library error: max thread count reached\n");
        unblock_timer();
        return -1;
    }

    threads[tid].tid         = tid;
    threads[tid].state       = THREAD_READY;
    threads[tid].quantums    = 0;
    threads[tid].sleep_until = 0;
    threads[tid].entry       = entry_point;
    blocked_flag[tid]        = 0;

    setup_jmpbuff(&threads[tid].env, stacks[tid], STACK_SIZE, entry_point);
    ready_enqueue(tid);

    unblock_timer();
    return tid;
}

int uthread_terminate(int tid)
{
    if (tid < 0 || tid >= MAX_THREAD_NUM ||
        threads[tid].state == THREAD_UNUSED)
    {
        fprintf(stderr, "thread library error: invalid tid %d\n", tid);
        return -1;
    }

    block_timer();

    if (tid == 0)
    {
        unblock_timer();
        exit(0);
    }

    /* Free the slot. */
    threads[tid].state       = THREAD_UNUSED;
    threads[tid].quantums    = 0;
    threads[tid].sleep_until = 0;
    threads[tid].entry       = NULL;
    blocked_flag[tid]        = 0;

    ready_remove(tid);

    if (tid == running_tid)
    {
        /* Self-termination: jump directly to the next thread. */
        total_quantums++;
        wake_sleeping_threads();

        int next_tid = ready_dequeue();
        if (next_tid == -1)
        {
            unblock_timer();
            exit(0);
        }

        thread_t *next = &threads[next_tid];
        next->quantums++;
        next->state = THREAD_RUNNING;
        running_tid = next_tid;

        reset_timer();
        unblock_timer();
        siglongjmp(next->env, 1);
        /* NOTREACHED */
    }

    unblock_timer();
    return 0;
}

int uthread_block(int tid)
{
    if (tid == 0)
    {
        fprintf(stderr, "thread library error: cannot block main thread\n");
        return -1;
    }
    if (tid < 0 || tid >= MAX_THREAD_NUM ||
        threads[tid].state == THREAD_UNUSED)
    {
        fprintf(stderr, "thread library error: invalid tid %d\n", tid);
        return -1;
    }

    block_timer();

    blocked_flag[tid] = 1;

    if (threads[tid].state == THREAD_BLOCKED)
    {
        /* Already blocked (could be sleeping); no further action needed. */
        unblock_timer();
        return 0;
    }

    if (threads[tid].state == THREAD_READY)
    {
        ready_remove(tid);
        threads[tid].state = THREAD_BLOCKED;
        unblock_timer();
        return 0;
    }

    if (threads[tid].state == THREAD_RUNNING)
    {
        /* Blocking ourselves: yield to the next thread. */
        threads[tid].state = THREAD_BLOCKED;
        schedule_next();
        return 0;
    }

    unblock_timer();
    return 0;
}

int uthread_resume(int tid)
{
    if (tid < 0 || tid >= MAX_THREAD_NUM ||
        threads[tid].state == THREAD_UNUSED)
    {
        fprintf(stderr, "thread library error: invalid tid %d\n", tid);
        return -1;
    }

    block_timer();

    blocked_flag[tid] = 0;

    if (threads[tid].state == THREAD_BLOCKED)
    {
        /*
         * Only make it READY if the sleep has also expired (or it was not
         * sleeping at all). If sleep_until > 0, the thread stays BLOCKED
         * until wake_sleeping_threads() clears the sleep.
         */
        if (threads[tid].sleep_until == 0)
        {
            threads[tid].state = THREAD_READY;
            ready_enqueue(tid);
        }
    }
    /* RUNNING or READY: no effect */

    unblock_timer();
    return 0;
}

int uthread_sleep(int num_quantums)
{
    if (running_tid == 0)
    {
        fprintf(stderr, "thread library error: main thread cannot sleep\n");
        return -1;
    }
    if (num_quantums <= 0)
    {
        fprintf(stderr, "thread library error: num_quantums must be positive\n");
        return -1;
    }

    block_timer();

    thread_t *self = &threads[running_tid];

    /* Current quantum is not counted toward the sleep duration. */
    self->sleep_until = total_quantums + num_quantums + 1;
    self->state       = THREAD_BLOCKED;
    /* blocked_flag stays 0: only the sleep is holding this thread back */

    schedule_next();

    /* execution resumes here after the sleep expires and we're scheduled */
    return 0;
}

int uthread_get_tid(void)
{
    return running_tid;
}

int uthread_get_total_quantums(void)
{
    return total_quantums;
}

int uthread_get_quantums(int tid)
{
    if (tid < 0 || tid >= MAX_THREAD_NUM ||
        threads[tid].state == THREAD_UNUSED)
    {
        fprintf(stderr, "thread library error: invalid tid %d\n", tid);
        return -1;
    }
    return threads[tid].quantums;
}
