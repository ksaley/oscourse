/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_SYNC_H
#define JOS_KERN_SYNC_H
#ifndef JOS_KERNEL
#error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/types.h>
#include <kern/env.h>

/* Mutex structure */
struct Mutex {
    volatile int locked;        /* 0 = unlocked, 1 = locked */
    struct Env *owner;         /* Owner environment (if locked) */
    struct Env *wait_queue;    /* Queue of waiting environments */
};

/* Condition variable structure */
struct CondVar {
    struct Mutex *mutex;       /* Associated mutex */
    struct Env *wait_queue;    /* Queue of waiting environments */
};

/* Mutex operations */
void mutex_init(struct Mutex *m);
void mutex_lock(struct Mutex *m);
void mutex_unlock(struct Mutex *m);
int mutex_trylock(struct Mutex *m);

/* Condition variable operations */
void condvar_init(struct CondVar *cv, struct Mutex *m);
void condvar_wait(struct CondVar *cv);
void condvar_signal(struct CondVar *cv);
void condvar_broadcast(struct CondVar *cv);

/* System call handlers */
void csys_mutex_init(struct Trapframe *tf);
void csys_mutex_lock(struct Trapframe *tf);
void csys_mutex_unlock(struct Trapframe *tf);
void csys_mutex_trylock(struct Trapframe *tf);
void csys_condvar_init(struct Trapframe *tf);
void csys_condvar_wait(struct Trapframe *tf);
void csys_condvar_signal(struct Trapframe *tf);
void csys_condvar_broadcast(struct Trapframe *tf);

#endif /* !JOS_KERN_SYNC_H */

