/* Test for mutex synchronization */
#include <inc/lib.h>

#ifdef JOS_PROG
void (*volatile sys_mutex_init)(void *m);
void (*volatile sys_mutex_lock)(void *m);
void (*volatile sys_mutex_unlock)(void *m);
int (*volatile sys_mutex_trylock)(void *m);
void (*volatile sys_yield)(void);
#endif

/* Shared mutex (should be in shared memory in real implementation) */
volatile struct {
    int locked;
    void *owner;
    void *wait_queue;
} test_mutex;

volatile int shared_counter = 0;

void
umain(int argc, char **argv) {
    int i;
    
    /* Initialize mutex */
    sys_mutex_init((void *)&test_mutex);
    
    /* Lock and increment counter */
    for (i = 0; i < 100; i++) {
        sys_mutex_lock((void *)&test_mutex);
        shared_counter++;
        sys_mutex_unlock((void *)&test_mutex);
        sys_yield();
    }
    
    sys_exit();
}

