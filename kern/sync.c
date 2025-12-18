#include <inc/assert.h>
#include <inc/string.h>
#include <kern/env.h>
#include <kern/sync.h>
#include <kern/sched.h>

/* Initialize a mutex */
void
mutex_init(struct Mutex *m) {
    if (!m) return;
    m->locked = 0;
    m->owner = NULL;
    m->wait_queue = NULL;
}

/* Try to acquire a mutex without blocking */
int
mutex_trylock(struct Mutex *m) {
    if (!m) return -1;
    
    /* Use atomic compare-and-swap */
    int expected = 0;
    if (__atomic_compare_exchange_n(&m->locked, &expected, 1, 0, 
                                    __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        m->owner = curenv;
        return 0; /* Success */
    }
    return -1; /* Locked */
}

/* Acquire a mutex, blocking if necessary */
void
mutex_lock(struct Mutex *m) {
    if (!m || !curenv) return;
    
    /* Try to acquire the lock */
    if (mutex_trylock(m) == 0) {
        return; /* Success */
    }
    
    /* Lock is held, add to wait queue and block */
    struct Env *prev = NULL;
    struct Env *curr = m->wait_queue;
    
    /* Find end of queue */
    while (curr != NULL) {
        prev = curr;
        curr = curr->env_wait_link;
    }
    
    /* Add to end of queue */
    if (prev) {
        prev->env_wait_link = curenv;
    } else {
        m->wait_queue = curenv;
    }
    curenv->env_wait_link = NULL;
    
    /* Remove from runnable queue and block */
    sched_eevdf_dequeue(curenv);
    curenv->env_status = ENV_NOT_RUNNABLE;
    
    /* Yield to scheduler */
    sched_yield();
}

/* Release a mutex and wake up waiting processes */
void
mutex_unlock(struct Mutex *m) {
    if (!m || !curenv) return;
    
    /* Check if we own the lock */
    if (m->owner != curenv || m->locked == 0) {
        panic("mutex_unlock: not owner or already unlocked");
    }
    
    /* Release the lock */
    m->locked = 0;
    m->owner = NULL;
    
    /* Wake up the first waiting process */
    if (m->wait_queue != NULL) {
        struct Env *next = m->wait_queue;
        m->wait_queue = next->env_wait_link;
        next->env_wait_link = NULL;
        
        /* Make it runnable */
        next->env_status = ENV_RUNNABLE;
        sched_eevdf_enqueue(next);
    }
}

/* Initialize a condition variable */
void
condvar_init(struct CondVar *cv, struct Mutex *m) {
    if (!cv) return;
    cv->mutex = m;
    cv->wait_queue = NULL;
}

/* Wait on a condition variable */
void
condvar_wait(struct CondVar *cv) {
    if (!cv || !cv->mutex || !curenv) return;
    
    struct Mutex *m = cv->mutex;
    
    /* Must hold the mutex */
    if (m->owner != curenv || m->locked == 0) {
        panic("condvar_wait: must hold mutex");
    }
    
    /* Add to condition variable wait queue */
    struct Env *prev = NULL;
    struct Env *curr = cv->wait_queue;
    
    while (curr != NULL) {
        prev = curr;
        curr = curr->env_wait_link;
    }
    
    if (prev) {
        prev->env_wait_link = curenv;
    } else {
        cv->wait_queue = curenv;
    }
    curenv->env_wait_link = NULL;
    
    /* Release mutex and block */
    m->locked = 0;
    m->owner = NULL;
    
    /* Wake up mutex waiters */
    if (m->wait_queue != NULL) {
        struct Env *next = m->wait_queue;
        m->wait_queue = next->env_wait_link;
        next->env_wait_link = NULL;
        next->env_status = ENV_RUNNABLE;
        sched_eevdf_enqueue(next);
    }
    
    /* Remove from runnable and block */
    sched_eevdf_dequeue(curenv);
    curenv->env_status = ENV_NOT_RUNNABLE;
    
    /* Yield to scheduler */
    sched_yield();
    
    /* When we wake up, we need to re-acquire the mutex */
    mutex_lock(m);
}

/* Signal a condition variable (wake one waiter) */
void
condvar_signal(struct CondVar *cv) {
    if (!cv || !cv->mutex || !curenv) return;
    
    struct Mutex *m = cv->mutex;
    
    /* Must hold the mutex */
    if (m->owner != curenv || m->locked == 0) {
        panic("condvar_signal: must hold mutex");
    }
    
    /* Wake up the first waiter */
    if (cv->wait_queue != NULL) {
        struct Env *next = cv->wait_queue;
        cv->wait_queue = next->env_wait_link;
        next->env_wait_link = NULL;
        
        /* Make it runnable */
        next->env_status = ENV_RUNNABLE;
        sched_eevdf_enqueue(next);
    }
}

/* Broadcast a condition variable (wake all waiters) */
void
condvar_broadcast(struct CondVar *cv) {
    if (!cv || !cv->mutex || !curenv) return;
    
    struct Mutex *m = cv->mutex;
    
    /* Must hold the mutex */
    if (m->owner != curenv || m->locked == 0) {
        panic("condvar_broadcast: must hold mutex");
    }
    
    /* Wake up all waiters */
    while (cv->wait_queue != NULL) {
        struct Env *next = cv->wait_queue;
        cv->wait_queue = next->env_wait_link;
        next->env_wait_link = NULL;
        
        next->env_status = ENV_RUNNABLE;
        sched_eevdf_enqueue(next);
    }
}

/* System call handlers for mutex operations */
void
csys_mutex_init(struct Trapframe *tf) {
    if (!curenv) panic("curenv = NULL");
    struct Mutex *m = (struct Mutex *)tf->tf_regs.reg_rdi;
    mutex_init(m);
    curenv->env_tf.tf_regs.reg_rax = 0;
}

void
csys_mutex_lock(struct Trapframe *tf) {
    if (!curenv) panic("curenv = NULL");
    struct Mutex *m = (struct Mutex *)tf->tf_regs.reg_rdi;
    mutex_lock(m);
    curenv->env_tf.tf_regs.reg_rax = 0;
}

void
csys_mutex_unlock(struct Trapframe *tf) {
    if (!curenv) panic("curenv = NULL");
    struct Mutex *m = (struct Mutex *)tf->tf_regs.reg_rdi;
    mutex_unlock(m);
    curenv->env_tf.tf_regs.reg_rax = 0;
}

void
csys_mutex_trylock(struct Trapframe *tf) {
    if (!curenv) panic("curenv = NULL");
    struct Mutex *m = (struct Mutex *)tf->tf_regs.reg_rdi;
    int result = mutex_trylock(m);
    curenv->env_tf.tf_regs.reg_rax = result;
}

/* System call handlers for condition variable operations */
void
csys_condvar_init(struct Trapframe *tf) {
    if (!curenv) panic("curenv = NULL");
    struct CondVar *cv = (struct CondVar *)tf->tf_regs.reg_rdi;
    struct Mutex *m = (struct Mutex *)tf->tf_regs.reg_rsi;
    condvar_init(cv, m);
    curenv->env_tf.tf_regs.reg_rax = 0;
}

void
csys_condvar_wait(struct Trapframe *tf) {
    if (!curenv) panic("curenv = NULL");
    struct CondVar *cv = (struct CondVar *)tf->tf_regs.reg_rdi;
    condvar_wait(cv);
    curenv->env_tf.tf_regs.reg_rax = 0;
}

void
csys_condvar_signal(struct Trapframe *tf) {
    if (!curenv) panic("curenv = NULL");
    struct CondVar *cv = (struct CondVar *)tf->tf_regs.reg_rdi;
    condvar_signal(cv);
    curenv->env_tf.tf_regs.reg_rax = 0;
}

void
csys_condvar_broadcast(struct Trapframe *tf) {
    if (!curenv) panic("curenv = NULL");
    struct CondVar *cv = (struct CondVar *)tf->tf_regs.reg_rdi;
    condvar_broadcast(cv);
    curenv->env_tf.tf_regs.reg_rax = 0;
}

