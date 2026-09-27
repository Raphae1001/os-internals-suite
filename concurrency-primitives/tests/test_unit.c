#include "tl_semaphore.h"

/* simple single-thread sanity check */

int main(void)
{
    semaphore s;
    int start = 2;

    semaphore_init(&s, start);

    semaphore_wait(&s);
    semaphore_wait(&s);

    semaphore_signal(&s);
    semaphore_signal(&s);
    semaphore_signal(&s);

    semaphore_wait(&s);
    semaphore_wait(&s);
    semaphore_wait(&s);

    semaphore_signal(&s);

    return 0;
}
