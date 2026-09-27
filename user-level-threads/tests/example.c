#include "jump.h"

#define SECOND 1000000
#ifdef __aarch64__
#define STACK_SIZE 65536
#else
#define STACK_SIZE 4096
#endif

char stack0[STACK_SIZE];
char stack1[STACK_SIZE];

sigjmp_buf env[2];
int current_thread = -1;

/* ── Context switching (shared) ──────────────────────────────────── */

void jump_to_thread(int tid)
{
    current_thread = tid;
    siglongjmp(env[tid], 1);
}

void yield(void)
{
    int prev = current_thread;
    fprintf(stderr, "yield: thread %d saving context\n",
            current_thread);
    int ret_val = sigsetjmp(env[current_thread], 1);
    if (ret_val == 0)
        jump_to_thread(1 - current_thread);
    fprintf(stderr, "yield: thread %d resumed (was %d)\n",
            current_thread, 1 - current_thread);
}

/* ── Thread functions ────────────────────────────────────────────── */

void thread0(void)
{
    int counter = 0;
    while (1)
    {
        counter++;
        printf("Thread 0: counter = %d\n", counter);
        fflush(stdout);
        if (counter % 3 == 0)
        {
            printf("Thread 0 yielding...\n");
            fflush(stdout);
            yield();
        }
        usleep(SECOND);
    }
}

void thread1(void)
{
    int counter = 0;
    while (1)
    {
        counter++;
        printf("Thread 1: counter = %d\n", counter);
        fflush(stdout);
        if (counter % 5 == 0)
        {
            printf("Thread 1 yielding...\n");
            fflush(stdout);
            yield();
        }
        usleep(SECOND);
    }
}

/* ── Main ────────────────────────────────────────────────────────── */

void setup(void)
{
    setup_jmpbuff(&env[0], stack0, STACK_SIZE, thread0);
    setup_jmpbuff(&env[1], stack1, STACK_SIZE, thread1);
}

int main(void)
{
    setup();
    jump_to_thread(0);
    return 0;
}
