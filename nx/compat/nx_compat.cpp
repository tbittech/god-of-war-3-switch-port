/* Capa POSIX minima para correr el runtime de ps3recomp en Horizon.
 * mmap/mprotect: la ventana guest de 4 GB es una reserva de VA (RexGmInit); "mprotect RW" = commit + mapear. */
#ifdef __SWITCH__
#include <switch.h>
#include <malloc.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <dlfcn.h>
#include <ifaddrs.h>
#include "nx_compat.h"
#include "../gm/guest_memory_switch.h"

static uint8_t* g_win = nullptr;
static size_t   g_win_size = 0;
static const size_t kStep = 0x10000;   /* paso para forzar el mapeo de lo comprometido */

static bool in_window(void* a) { return g_win && (uint8_t*)a >= g_win && (uint8_t*)a < g_win + g_win_size; }

extern "C" void* mmap(void* addr, size_t len, int prot, int flags, int fd, off_t) {
    if (fd != -1) { errno = ENOSYS; return MAP_FAILED; }
    /* La reserva grande (vm_init con PROT_NONE, o main.cpp con RW + MAP_NORESERVE): solo VA.
     * Las paginas se comprometen al tocarlas, desde el handler de excepciones (nx_exception.cpp). */
    (void)prot; (void)flags;
    if (!addr && len >= 0x40000000ull) {
        if (g_win) { errno = ENOMEM; return MAP_FAILED; }
        g_win = RexGmInit(len);
        fprintf(stderr, "[nx] reserva guest 0x%llx bytes -> %p (gm_err=0x%x, modo=%d)\n",
                (unsigned long long)len, (void*)g_win, RexGmLastResult(), RexGmProtectionMode());
        if (!g_win) { errno = ENOMEM; return MAP_FAILED; }
        g_win_size = len;
        RexGmAddView(g_win, 0, len);
        return g_win;
    }
    if (addr) { errno = ENOMEM; return MAP_FAILED; }                /* sin mapeo a direccion pedida */
    size_t sz = (len + 0xFFF) & ~(size_t)0xFFF;
    void* p = memalign(0x1000, sz);
    if (!p) { errno = ENOMEM; return MAP_FAILED; }
    memset(p, 0, sz);
    return p;
}

extern "C" int munmap(void* addr, size_t len) {
    if (in_window(addr)) return 0;      /* recortes de cabeza/cola de la reserva: nada que hacer */
    free(addr); (void)len; return 0;
}

extern "C" int mprotect(void* addr, size_t len, int prot) {
    if (!in_window(addr)) return 0;     /* heap normal: siempre RW */
    size_t off = (uint8_t*)addr - g_win;
    if (prot & (PROT_READ | PROT_WRITE)) {
        if (!RexGmCommit(off, len, REX_GM_WRITE)) { errno = ENOMEM; return -1; }
        for (size_t o = off & ~(kStep - 1); o < off + len; o += kStep) RexGmFaultIn((uint64_t)(g_win + o));
        if (!(prot & PROT_WRITE)) RexGmProtect((uint8_t*)addr, len, REX_GM_READ, nullptr);
        else RexGmProtect((uint8_t*)addr, len, REX_GM_WRITE, nullptr);
        return 0;
    }
    RexGmProtect((uint8_t*)addr, len, REX_GM_NONE, nullptr);
    return 0;
}

extern "C" int madvise(void* addr, size_t len, int) {
    if (in_window(addr)) RexGmDecommit((uint8_t*)addr - g_win, len);
    return 0;
}

extern "C" long syscall(long n, ...) {
    if (n == SYS_gettid) { u64 tid = 0; svcGetThreadId(&tid, CUR_THREAD_HANDLE); return (long)tid; }
    errno = ENOSYS; return -1;
}

extern "C" time_t timegm(struct tm* tm) {      /* newlib en Horizon: sin zona horaria, mktime ya es UTC */
    return mktime(tm);
}

extern "C" int pthread_mutex_timedlock(pthread_mutex_t* m, const struct timespec* abs_timeout) {
    for (;;) {
        int r = pthread_mutex_trylock(m);
        if (r != EBUSY) return r;
        struct timespec now; clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec > abs_timeout->tv_sec ||
            (now.tv_sec == abs_timeout->tv_sec && now.tv_nsec >= abs_timeout->tv_nsec)) return ETIMEDOUT;
        svcSleepThread(200000);
    }
}

/* libc que newlib/libnx declara pero no trae. */
extern "C" __attribute__((weak)) long sysconf(int name) {   /* weak: con NVK manda el de nvk_compat.o */
    switch (name) {
        case _SC_PAGESIZE:          return 0x1000;
        case _SC_NPROCESSORS_CONF:
        case _SC_NPROCESSORS_ONLN:  return 4;
        default: errno = EINVAL;    return -1;
    }
}
extern "C" int timespec_get(struct timespec* ts, int base) {
    if (base != 1 /* TIME_UTC */ || clock_gettime(CLOCK_REALTIME, ts) != 0) return 0;
    return base;
}
extern "C" __attribute__((weak)) int posix_memalign(void** out, size_t align, size_t size) {
    void* p = memalign(align, size);
    if (!p) return ENOMEM;
    *out = p; return 0;
}
extern "C" int pipe(int fd[2]) { (void)fd; errno = ENOSYS; return -1; }   /* ppu_loader: la sonda de lectura queda apagada */

extern "C" void* dlopen(const char*, int) { return nullptr; }
extern "C" void* dlsym(void*, const char*) { return nullptr; }
extern "C" int   dlclose(void*) { return 0; }
extern "C" const char* dlerror(void) { return "dlopen no existe en Horizon"; }
extern "C" int   dladdr(const void*, Dl_info*) { return 0; }
extern "C" int   getifaddrs(struct ifaddrs** out) { if (out) *out = nullptr; return 0; }
extern "C" void  freeifaddrs(struct ifaddrs*) {}
#endif

#ifdef __SWITCH__
#include <ucontext.h>
/* makecontext: los argumentos son ints (convencion POSIX); la funcion no debe retornar (uc_link no se sigue). */
extern "C" void makecontext(ucontext_t* uc, void (*fn)(void), int argc, ...) {
    va_list ap; va_start(ap, argc);
    for (int i = 0; i < argc && i < 8; i++) uc->uc_mcontext.regs[i] = (unsigned)va_arg(ap, int);
    va_end(ap);
    uintptr_t top = ((uintptr_t)uc->uc_stack.ss_sp + uc->uc_stack.ss_size) & ~(uintptr_t)15;
    uc->uc_mcontext.sp = top;
    uc->uc_mcontext.pc = (unsigned long long)(uintptr_t)fn;
    uc->uc_mcontext.regs[29] = 0;
    uc->uc_mcontext.regs[30] = 0;
}
#endif

#ifdef __SWITCH__
/* Tabla de handlers de "signals". Nadie los dispara todavia: falta el puente desde __libnx_exception_handler. */
static struct nx_sigaction g_sigtab[64];
extern "C" int nx_sigaction(int sig, const struct nx_sigaction* act, struct nx_sigaction* old) {
    if (sig < 0 || sig >= 64) { errno = EINVAL; return -1; }
    if (old) *old = g_sigtab[sig];
    if (act) g_sigtab[sig] = *act;
    return 0;
}
extern "C" const struct nx_sigaction* nx_sigaction_get(int sig) { return (sig >= 0 && sig < 64) ? &g_sigtab[sig] : nullptr; }
#endif
