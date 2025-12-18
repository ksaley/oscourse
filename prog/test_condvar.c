/* Test for condition variable */
#include <inc/lib.h>

#ifdef JOS_PROG
void (*volatile sys_mutex_init)(void *m);
void (*volatile sys_mutex_lock)(void *m);
void (*volatile sys_mutex_unlock)(void *m);
void (*volatile sys_condvar_init)(void *cv, void *m);
void (*volatile sys_condvar_wait)(void *cv);
void (*volatile sys_condvar_signal)(void *cv);
void (*volatile sys_condvar_broadcast)(void *cv);
#endif

/* Shared synchronization objects */
volatile struct {
    int locked;
    void *owner;
    void *wait_queue;
} test_mutex;

volatile struct {
    void *mutex;
    void *wait_queue;
} test_condvar;

volatile int condition = 0;

void
umain(int argc, char **argv) {
    /* Initialize mutex and condition variable */
    sys_mutex_init((void *)&test_mutex);
    sys_condvar_init((void *)&test_condvar, (void *)&test_mutex);
    
    /* Wait for condition */
    sys_mutex_lock((void *)&test_mutex);
    while (condition == 0) {
        sys_condvar_wait((void *)&test_condvar);
    }
    sys_mutex_unlock((void *)&test_mutex);
    
    /* Signal condition */
    sys_mutex_lock((void *)&test_mutex);
    condition = 1;
    sys_condvar_signal((void *)&test_condvar);
    sys_mutex_unlock((void *)&test_mutex);
    
    sys_exit();
}

