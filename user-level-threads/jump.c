#include "jump.h"

/* ── Architecture-specific thread setup ──────────────────────────── */

#ifdef __x86_64__

#define JB_SP 6
#define JB_PC 7

address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor %%fs:0x30, %0\n"
                "rol $0x11, %0\n"
                : "=g" (ret)
                : "0" (addr));
    return ret;
}

void setup_jmpbuff(sigjmp_buf *buff, char *stack, uint64_t stack_size, thread_entry_point entry_point)
{
    address_t sp = (address_t)stack + stack_size - sizeof(address_t);
    address_t pc = (address_t)entry_point;
    sigsetjmp(*buff, 1);
    (*buff)->__jmpbuf[JB_SP] = translate_address(sp);
    (*buff)->__jmpbuf[JB_PC] = translate_address(pc);
    sigemptyset(&(*buff)->__saved_mask);
}

#elif __aarch64__

static sigjmp_buf       *pending_env;
static thread_entry_point pending_entry;
static sigjmp_buf        setup_buf;

static void trampoline(int sig)
{
    (void)sig;

    volatile thread_entry_point my_entry = pending_entry;

    /*
    * savemask = 0: do NOT save the signal mask here, because SIGUSR1
    * is blocked inside this handler; we don't want to restore that
    * poisoned mask when the thread starts.
    */
    if (sigsetjmp(*pending_env, 0) == 0) {
        siglongjmp(setup_buf, 1);
        /* NOTREACHED */
    }

    my_entry();
}

void setup_jmpbuff(sigjmp_buf *buff, char *stack, uint64_t stack_size, thread_entry_point entry_point)
{
    stack_t ss, old_ss;
    struct sigaction sa, old_sa;

    ss.ss_sp    = stack;
    ss.ss_size  = stack_size;
    ss.ss_flags = 0;
    if (sigaltstack(&ss, &old_ss) != 0) {
        exit(1);
    }

    sa.sa_handler = trampoline;
    sa.sa_flags   = SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, &old_sa);

    pending_env   = buff;
    pending_entry = entry_point;

    /*
    * sigsetjmp here (on the MAIN stack) so the trampoline can
    * siglongjmp back once it has saved the thread context.
    */
    if(sigsetjmp(setup_buf, 1) == 0)
    {
        raise(SIGUSR1);
        /* raise() never returns: trampoline siglongjmps to setup_buf */
        exit(1);
    }

    sigaction(SIGUSR1, &old_sa, NULL);
    sigaltstack(&old_ss, NULL);
}

#else
#error "Unsupported architecture"
#endif