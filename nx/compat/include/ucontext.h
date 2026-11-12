/* ucontext minimo para aarch64/Horizon (fibras de cellFiber). Implementado en nx_ucontext.S. Sin mascara de signals. */
#pragma once
#include <signal.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned long long greg_t;
typedef struct { unsigned long long fault_address; unsigned long long regs[31]; unsigned long long sp, pc, pstate;
                 unsigned long long fpregs[8]; } mcontext_t;
typedef struct ucontext_t { unsigned long uc_flags; struct ucontext_t* uc_link; stack_t uc_stack; mcontext_t uc_mcontext; } ucontext_t;
_Static_assert(offsetof(ucontext_t, uc_mcontext) == 40, "nx_ucontext.S asume uc_mcontext en 40");
_Static_assert(offsetof(ucontext_t, uc_mcontext.sp) == 296, "nx_ucontext.S asume sp en 296");
int  getcontext(ucontext_t* uc);
int  setcontext(const ucontext_t* uc);
int  swapcontext(ucontext_t* out, const ucontext_t* in);
void makecontext(ucontext_t* uc, void (*fn)(void), int argc, ...);
#ifdef __cplusplus
}
#endif
