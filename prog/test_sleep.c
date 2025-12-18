/* Test for sleep system call */
#include <inc/lib.h>

#ifdef JOS_PROG
void (*volatile sys_sleep)(uint64_t usec);
#endif

volatile int wake_count = 0;

void
umain(int argc, char **argv) {
    /* Sleep for 1 second (1000000 microseconds) */
    sys_sleep(1000000);
    wake_count = 1;
    
    /* Sleep for 0.5 seconds */
    sys_sleep(500000);
    wake_count = 2;
    
    sys_exit();
}

