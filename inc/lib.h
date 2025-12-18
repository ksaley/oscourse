/* Main public header file for our user-land support library,
 * whose code lives in the lib directory.
 * This library is roughly our OS's version of a standard C library,
 * and is intended to be linked into all user-mode applications
 * (NOT the kernel or boot loader). */

#ifndef JOS_INC_LIB_H
#define JOS_INC_LIB_H 1

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>
#include <inc/string.h>
#include <inc/error.h>
#include <inc/assert.h>
#include <inc/env.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#ifdef SANITIZE_USER_SHADOW_BASE
/* asan unpoison routine used for whitelisting regions. */
void platform_asan_unpoison(void *, size_t);
void platform_asan_poison(void *, size_t);
/* non-sanitized memcpy and memset allow us to access "invalid" areas for extra poisoning. */
void *__nosan_memset(void *, int, size_t);
void *__nosan_memcpy(void *, const void *src, size_t);
#endif

#define USED(x) (void)(x)

/* main user program */
void umain(int argc, char **argv);

/* libmain.c or entry.S */
extern const char *binaryname;
extern const volatile struct Env *thisenv;
extern const volatile struct Env envs[NENV];

#ifdef JOS_PROG
extern void (*volatile sys_exit)(void);
extern void (*volatile sys_yield)(void);
extern void (*volatile sys_sched_setparam)(uint64_t weight);
extern void (*volatile sys_sleep)(uint64_t usec);
extern void (*volatile sys_mutex_init)(void *m);
extern void (*volatile sys_mutex_lock)(void *m);
extern void (*volatile sys_mutex_unlock)(void *m);
extern int (*volatile sys_mutex_trylock)(void *m);
extern void (*volatile sys_condvar_init)(void *cv, void *m);
extern void (*volatile sys_condvar_wait)(void *cv);
extern void (*volatile sys_condvar_signal)(void *cv);
extern void (*volatile sys_condvar_broadcast)(void *cv);
#endif

#ifndef debug
#define debug 0
#endif

#endif /* !JOS_INC_LIB_H */
