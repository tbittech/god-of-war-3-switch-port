/* Se incluye a la fuerza (-include) en todo el runtime de ps3recomp al compilar para Switch. */
#pragma once
#ifdef __SWITCH__
#include <time.h>
#include <setjmp.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifndef _setjmp
#define _setjmp(b)     setjmp(b)
#define _longjmp(b, v) longjmp(b, v)
#endif
/* Signals: Horizon no los tiene. Estas definiciones solo dejan compilar win32_compat.c;
 * sigaction() no instala nada (nx_compat.cpp). Las faltas hay que traerlas desde __libnx_exception_handler. */
#include <signal.h>
#include <limits.h>
#ifndef SA_SIGINFO
#define SA_SIGINFO 0x2
#endif
#ifndef SA_RESTART
#define SA_RESTART 0x10000000
#endif
#ifndef SA_ONSTACK
#define SA_ONSTACK 0x1
#endif
/* newlib no trae sa_sigaction: se reemplaza struct sigaction/sigaction() por una version propia
 * que guarda los handlers en una tabla (nx_compat.cpp), para despacharlos desde el handler de libnx. */
#ifdef __cplusplus
extern "C" {
#endif
struct nx_sigaction {
    union { void (*sa_handler)(int); void (*sa_sigaction)(int, siginfo_t*, void*); };
    sigset_t sa_mask; int sa_flags;
};
int nx_sigaction(int sig, const struct nx_sigaction* act, struct nx_sigaction* old);
#ifdef __cplusplus
}
#endif
#define sigaction nx_sigaction
#define si_addr si_value.sival_ptr
#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN 16384
#endif
#ifndef PS3_SUSPEND_SIGNAL
#define PS3_SUSPEND_SIGNAL SIGUSR1
#endif
#ifndef BUS_ADRALN
#define BUS_ADRALN 1
#define BUS_ADRERR 2
#define BUS_OBJERR 3
#define ILL_ILLOPC 1
#define ILL_PRVOPC 5
#define ILL_PRVREG 6
#define FPE_INTDIV 1
#define FPE_INTOVF 2
#define FPE_FLTDIV 3
#define FPE_FLTOVF 4
#define FPE_FLTUND 5
#define FPE_FLTRES 6
#define FPE_FLTINV 7
#define FPE_FLTSUB 8
#define SEGV_MAPERR 1
#define SEGV_ACCERR 2
#endif
/* newlib no tiene tm_gmtoff: en Horizon la hora local se trata como UTC */
#define tm_gmtoff tm_isdst * 0L
#ifdef __cplusplus
extern "C" {
#endif
time_t timegm(struct tm* tm);
int pthread_mutex_timedlock(pthread_mutex_t* m, const struct timespec* abs_timeout);
#ifdef __cplusplus
}
#endif
#endif
