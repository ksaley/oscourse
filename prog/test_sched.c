/* Test for EEVDF scheduler fairness */
#include <inc/lib.h>

#ifdef JOS_PROG
void (*volatile sys_yield)(void);
void (*volatile sys_sched_setparam)(uint64_t weight);
#endif

volatile int counter = 0;

void
umain(int argc, char **argv) {
    int i;
    
    /* Set different weights for different processes */
    /* This test should be run with multiple processes */
    /* Process 0: weight 1024 (default) */
    /* Process 1: weight 2048 (double) */
    /* Process 2: weight 512 (half) */
    
    for (i = 0; i < 1000; i++) {
        counter++;
        sys_yield();
    }
    
    sys_exit();
}

