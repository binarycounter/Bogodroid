#include <signal.h>
#include <string.h>
#include <cstdio>
#include "platform.h"

//All of this is awful


// #ifdef sa_handler
// #undef sa_handler
// #endif

// #ifdef sa_sigaction
// #undef sa_sigaction
// #endif

typedef void (*bionic_sighandler_t)(int);

struct bionic_sigaction {
    int sa_flags;
    union {
        bionic_sighandler_t bionic_sa_handler;
        void (*bionic_sa_sigaction)(int, siginfo_t*, void*);
    };
    unsigned long sa_mask;  /* Bionic sigset_t: 8 bytes */ 
};

extern "C" ABI_ATTR int sigemptyset_impl(uint64_t *bionic_set)
{
    printf("empty set %p\n",bionic_set);
    *bionic_set = 0;
    return 0;
}

extern "C" ABI_ATTR int sigfillset_impl(uint64_t *bionic_set)
{
    printf("fill set %p\n",bionic_set);
    *bionic_set = 0xffffffffffffffff;
    return 0;
}

extern "C" ABI_ATTR int sigaction_impl(int signum, const bionic_sigaction *bionic_act, bionic_sigaction *bionic_oldact)
{
    printf("sigaction %d %p %p\n",signum,bionic_act,bionic_oldact);
    // return 0;

    struct sigaction glibc_act = {0};
    struct sigaction glibc_oldact = {0};
    int ret = -1;

    // Convert Bionic -> GLibc if act is specified
    if (bionic_act) {
        printf("handler %p\n",bionic_act->bionic_sa_handler);
        printf("handler address %p\n",bionic_act->bionic_sa_sigaction);
        glibc_act.sa_flags = bionic_act->sa_flags;

        // Ugly hack to get pthread_stop_world.c in il2cpp working
        if(signum==0x18)
            glibc_act.sa_flags = glibc_act.sa_flags | SA_RESTART;
        if(signum==0x1e)
            glibc_act.sa_flags = glibc_act.sa_flags | SA_RESTART | SA_SIGINFO;

        if(glibc_act.sa_flags & SA_SIGINFO)
            glibc_act.sa_sigaction = bionic_act->bionic_sa_sigaction;
        else
            glibc_act.sa_handler = bionic_act->bionic_sa_handler;
        // Convert 32-bit mask to GLibc's sigset_t
        sigset_t mask;
        sigemptyset(&mask);
        for (int i = 0; i < 64; i++) {
            if (bionic_act->sa_mask & (1U << i)) {
                sigaddset(&mask, i + 1);
            }
        }
        glibc_act.sa_mask = mask;

        printf("----------------------\n");
        printf("sigaction struct:\n");
        printf("  sa_handler: %p\n", (void *)glibc_act.sa_handler);
        printf("  sa_sigaction: %p\n", (void *)glibc_act.sa_sigaction);
        printf("  sa_mask: { ");
        for (int i = 1; i < NSIG; i++) {
        if (sigismember(&glibc_act.sa_mask, i)) {
            printf("%d ", i);
        }
    }
        printf("}\n");
        printf("  sa_mask (inverse): { ");
        for (int i = 1; i < NSIG; i++) {
        if (!sigismember(&glibc_act.sa_mask, i)) {
            printf("%d ", i);
        }
    }
        printf("}\n");
        printf("  sa_flags: 0x%x\n", glibc_act.sa_flags);
        printf("  sa_restorer: %p\n", (void *)glibc_act.sa_restorer);
        printf("----------------------\n");
    }

    // Call actual GLibc sigaction
    ret = sigaction(signum, 
                   bionic_act ? &glibc_act : NULL, 
                   bionic_oldact ? &glibc_oldact : NULL);

    // Convert GLibc -> Bionic if oldact is specified
    if (bionic_oldact && ret == 0) {
        memset(bionic_oldact, 0, sizeof(*bionic_oldact));
        bionic_oldact->sa_flags = glibc_oldact.sa_flags;
        bionic_oldact->bionic_sa_sigaction = glibc_oldact.sa_sigaction;
        
        // Convert sigset_t back to 32-bit mask
        uint32_t mask = 0;
        for (int i = 0; i < 64; i++) {
            int sig = i + 1;
            mask |= sigismember(&glibc_oldact.sa_mask, sig) ? (1U << i) : 0;
        }
        bionic_oldact->sa_mask = mask;
    }

    return ret;
}


extern "C" ABI_ATTR int sigaddset_impl(uint64_t *set, int signo) {
    if (signo <= 0 || signo > 64) {
        return -1;  
    }
    *set |= (1ULL << (signo - 1));  // Set the bit corresponding to the signal
    return 0;  
}

extern "C" ABI_ATTR int sigdelset_impl(uint64_t *set, int signo) {
    if (signo <= 0 || signo > 64) {
        return -1;  
    }
    *set &= ~(1ULL << (signo - 1)); 
    return 0; 
}