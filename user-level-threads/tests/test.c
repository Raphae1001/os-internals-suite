#include <stdio.h>
#include <unistd.h>
#include "uthreads.h"

#define SCHED_LENGTH 10

int scheduling[SCHED_LENGTH] = {-1};
int idx = 0;

void uthread_function(void)
{
    scheduling[idx++] = uthread_get_tid();
    kill(getpid(), SIGVTALRM);
    scheduling[idx++] = uthread_get_tid();
    uthread_terminate(uthread_get_tid());
}

void uthread_function2(void)
{
    scheduling[idx++] = uthread_get_tid();
    uthread_sleep(4);
    scheduling[idx++] = uthread_get_tid();
    uthread_terminate(uthread_get_tid());
}

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;
    uthread_init(100000);
    uthread_spawn(uthread_function);
    uthread_spawn(uthread_function2);
    while(1)
    {
        if(idx >= SCHED_LENGTH)
        {
            break;
        }
        scheduling[idx++] = uthread_get_tid();
        kill(getpid(), SIGVTALRM); // context switch
    }

    int expected_scheduling[SCHED_LENGTH] = {0, 1, 2, 0, 1, 0, 0, 2, 0, 0};

    for(int i = 0; i < SCHED_LENGTH; i++)
    {
        if(scheduling[i] != expected_scheduling[i])
        {
            fprintf(stderr, "Failed! Expected scheduling: ");
            for(int j = 0; j < SCHED_LENGTH; j++)
            {
                fprintf(stderr, "%d ", expected_scheduling[j]);
            }
            fprintf(stderr, ", got: ");
            for(int j = 0; j < SCHED_LENGTH; j++)
            {
                if(scheduling[j] == -1)
                {
                    fprintf(stderr, "? ");
                }
                else
                {
                    fprintf(stderr, "%d ", scheduling[j]);
                }
            }
            fprintf(stderr, ".");
            return 1;
        }
    }
    printf("Passed!\n");
    return 0;
}
