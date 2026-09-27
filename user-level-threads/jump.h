#ifndef JUMP_H
#define JUMP_H

/*
* Simplified cooperative threads demo using sigsetjmp / siglongjmp.
* Two threads are set up with separate stacks, and they switch control
* by saving and restoring execution contexts.
*
* Supports both x86-64 and AArch64 (Linux / glibc).
*/

#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

typedef unsigned long address_t;
typedef void (*thread_entry_point)(void);

void setup_jmpbuff(sigjmp_buf *buff, char *stack, uint64_t stack_size, thread_entry_point entry_point);

#endif