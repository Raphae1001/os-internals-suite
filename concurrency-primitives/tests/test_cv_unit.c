#include "cond_var.h"

#include <stdio.h>

int main(void)
{
    condition_variable cv;

    condition_variable_init(&cv);
    condition_variable_signal(&cv);
    condition_variable_broadcast(&cv);

    return 0;
}
