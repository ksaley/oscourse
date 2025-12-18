#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/env.h>
#include <kern/monitor.h>
#include <kern/sched.h>
#include <kern/tsc.h>
#include <kern/timer.h>

struct Taskstate cpu_ts;
_Noreturn void sched_halt(void);

/* EEVDF scheduler state */
static struct Env *sched_runnable_head = NULL;
static uint64_t sched_min_vruntime = 0;
static uint64_t sched_total_weight = 0;

/* Get CPU frequency for time calculations */
static uint64_t
get_cpu_freq(void) {
    static uint64_t cpu_freq = 0;
    if (!cpu_freq && timer_for_schedule) {
        cpu_freq = timer_for_schedule->get_cpu_freq();
    }
    return cpu_freq ? cpu_freq : 2500000000ULL; /* Default 2.5 GHz */
}

/* Initialize EEVDF scheduler */
void
sched_eevdf_init(void) {
    sched_runnable_head = NULL;
    sched_min_vruntime = 0;
    sched_total_weight = 0;
}

/* Calculate virtual deadline for a process */
uint64_t
sched_eevdf_calc_vdeadline(struct Env *env, uint64_t vruntime) {
    if (env->env_weight == 0) {
        return vruntime;
    }
    /* Virtual deadline = vruntime + (slice_size / weight) */
    /* We use a default slice size of 1ms in virtual time units */
    uint64_t slice_vtime = 1000000ULL; /* 1ms in microseconds */
    return vruntime + (slice_vtime * 1024) / env->env_weight;
}

/* Update virtual runtime for a process after it has run */
void
sched_eevdf_update_vruntime(struct Env *env, uint64_t runtime) {
    if (env->env_weight == 0) {
        return;
    }
    /* Update vruntime: vruntime += (actual_runtime * total_weight) / weight */
    uint64_t vtime_delta = (runtime * sched_total_weight) / env->env_weight;
    env->env_vruntime += vtime_delta;
    
    /* Update minimum vruntime */
    if (env->env_vruntime < sched_min_vruntime) {
        sched_min_vruntime = env->env_vruntime;
    }
}

/* Enqueue a runnable environment into the scheduler queue */
void
sched_eevdf_enqueue(struct Env *env) {
    if (!env || env->env_status != ENV_RUNNABLE) {
        return;
    }

    /* Calculate virtual deadline */
    uint64_t vruntime = env->env_vruntime;
    if (vruntime < sched_min_vruntime) {
        vruntime = sched_min_vruntime;
        env->env_vruntime = vruntime;
    }
    env->env_vdeadline = sched_eevdf_calc_vdeadline(env, vruntime);

    /* Insert into sorted list by virtual deadline */
    struct Env **prev = &sched_runnable_head;
    struct Env *curr = sched_runnable_head;
    
    while (curr != NULL && curr->env_vdeadline <= env->env_vdeadline) {
        prev = &curr->env_sched_link;
        curr = curr->env_sched_link;
    }
    
    env->env_sched_link = curr;
    *prev = env;
    
    /* Update total weight */
    sched_total_weight += env->env_weight;
}

/* Dequeue an environment from the scheduler queue */
void
sched_eevdf_dequeue(struct Env *env) {
    if (!env) {
        return;
    }

    /* Remove from list */
    struct Env **prev = &sched_runnable_head;
    struct Env *curr = sched_runnable_head;
    
    while (curr != NULL && curr != env) {
        prev = &curr->env_sched_link;
        curr = curr->env_sched_link;
    }
    
    if (curr == env) {
        *prev = env->env_sched_link;
        env->env_sched_link = NULL;
        sched_total_weight -= env->env_weight;
    }
}

/* Pick the next environment to run (earliest eligible virtual deadline) */
struct Env *
sched_eevdf_pick_next(void) {
    /* Find the earliest eligible process */
    struct Env *best = NULL;
    uint64_t best_deadline = UINT64_MAX;
    
    for (struct Env *env = sched_runnable_head; env != NULL; env = env->env_sched_link) {
        if (env->env_status != ENV_RUNNABLE) {
            continue;
        }
        
        /* Check if process is eligible (vruntime <= min_vruntime + latency) */
        uint64_t latency_vtime = 1000000ULL; /* 1ms latency in virtual time */
        if (env->env_vruntime <= sched_min_vruntime + latency_vtime) {
            if (env->env_vdeadline < best_deadline) {
                best = env;
                best_deadline = env->env_vdeadline;
            }
        }
    }
    
    /* If no eligible process found, pick the one with earliest deadline */
    if (!best) {
        for (struct Env *env = sched_runnable_head; env != NULL; env = env->env_sched_link) {
            if (env->env_status == ENV_RUNNABLE && env->env_vdeadline < best_deadline) {
                best = env;
                best_deadline = env->env_vdeadline;
            }
        }
    }
    
    return best;
}

/* Wake up sleeping processes whose sleep time has expired */
void
sched_eevdf_wake_sleeping(void) {
    uint64_t now = read_tsc();
    
    for (int i = 0; i < NENV; i++) {
        struct Env *env = &envs[i];
        if (env->env_status == ENV_NOT_RUNNABLE && 
            env->env_sleep_until > 0 && 
            now >= env->env_sleep_until) {
            env->env_sleep_until = 0;
            env->env_status = ENV_RUNNABLE;
            sched_eevdf_enqueue(env);
        }
    }
}

/* Choose a user environment to run and run it */
_Noreturn void
sched_yield(void) {
    /* Wake up sleeping processes */
    sched_eevdf_wake_sleeping();
    
    /* Update vruntime for the current process if it was running */
    if (curenv && curenv->env_status == ENV_RUNNING) {
        uint64_t slice_start = curenv->env_slice_start;
        if (slice_start > 0) {
            uint64_t runtime = read_tsc() - slice_start;
            sched_eevdf_update_vruntime(curenv, runtime);
        }
        curenv->env_status = ENV_RUNNABLE;
        sched_eevdf_enqueue(curenv);
    }
    
    /* Pick next process to run */
    struct Env *next = sched_eevdf_pick_next();
    
    if (next) {
        /* Remove from queue */
        sched_eevdf_dequeue(next);
        next->env_slice_start = read_tsc();
        env_run(next);
    }
    
    /* If no runnable environments, check if current one should continue */
    if (curenv && curenv->env_status == ENV_RUNNING) {
        env_run(curenv);
    }
    
    cprintf("Halt\n");
    
    /* No runnable environments, so just halt the cpu */
    sched_halt();
}

/* Halt this CPU when there is nothing to do. Wait until the
 * timer interrupt wakes it up. This function never returns */
_Noreturn void
sched_halt(void) {

    /* For debugging and testing purposes, if there are no runnable
     * environments in the system, then drop into the kernel monitor */
    int i;
    for (i = 0; i < NENV; i++)
        if (envs[i].env_status == ENV_RUNNABLE ||
            envs[i].env_status == ENV_RUNNING) break;
    if (i == NENV) {
        cprintf("No runnable environments in the system!\n");
        for (;;) monitor(NULL);
    }

    /* Mark that no environment is running on CPU */
    curenv = NULL;

    /* Reset stack pointer, enable interrupts and then halt */
    asm volatile(
            "movq $0, %%rbp\n"
            "movq %0, %%rsp\n"
            "pushq $0\n"
            "pushq $0\n"
            "sti\n"
            "hlt\n" ::"a"(cpu_ts.ts_rsp0));

    /* Unreachable */
    for (;;)
        ;
}
