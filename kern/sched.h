/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_SCHED_H
#define JOS_KERN_SCHED_H
#ifndef JOS_KERNEL
#error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/types.h>
#include <kern/env.h>

_Noreturn void sched_yield(void);

/* EEVDF scheduler functions */
void sched_eevdf_init(void);
void sched_eevdf_enqueue(struct Env *env);
void sched_eevdf_dequeue(struct Env *env);
struct Env *sched_eevdf_pick_next(void);
void sched_eevdf_update_vruntime(struct Env *env, uint64_t runtime);
uint64_t sched_eevdf_calc_vdeadline(struct Env *env, uint64_t vruntime);
void sched_eevdf_wake_sleeping(void);

#endif /* !JOS_KERN_SCHED_H */
