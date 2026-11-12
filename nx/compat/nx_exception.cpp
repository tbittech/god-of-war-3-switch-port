/* Puente de excepciones de Horizon -> tabla nx_sigaction (handlers "POSIX" del runtime de ps3recomp).
 * La entrada (__libnx_exception_entry) y la reanudacion por trap vienen de kof2002um-nx
 * (rexglue-sdk/src/core/exception_handler_switch.cpp + resume_ctx.h), probadas en consola.
 * OJO: este archivo va directo en el ejecutable, no en una .a: los simbolos de libnx son weak y una
 * referencia weak no saca el objeto de una libreria estatica. */
#ifdef __SWITCH__
#include <switch.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ucontext.h>
#include "nx_compat.h"
#include "nx_resume_ctx.h"
#include "../gm/guest_memory_switch.h"

#define REX_EXC_SLOTS 8
#define REX_EXC_STACK 0x10000   /* 64 KB por slot -> shift de 16 */
#define REX_EXC_DUMP  0x400     /* el dump ocupa 0x340 -> shift de 10 */
extern "C" {
alignas(16) uint8_t  g_rex_exc_stacks[REX_EXC_SLOTS][REX_EXC_STACK];
alignas(16) uint8_t  g_rex_exc_dumped[REX_EXC_SLOTS][REX_EXC_DUMP];
alignas(16) uint32_t g_rex_exc_busy[REX_EXC_SLOTS];
alignas(16) uint8_t  g_rex_exc_neon_tmp[512];
uint32_t g_rex_exc_neon_fix = 1;
u32 __nx_exception_ignoredebug = 0;
const struct nx_sigaction* nx_sigaction_get(int sig);
void _start(void);
}
static_assert(sizeof(ThreadExceptionDump) <= REX_EXC_DUMP, "el dump no entra en el slot");
static_assert(REX_EXC_SLOTS == 8, "el assembly compara contra 8 a mano");

__asm__(".text\n"
        ".balign 4\n"
        ".global __libnx_exception_entry\n"
        ".type __libnx_exception_entry, %function\n"
        "__libnx_exception_entry:\n"
        "    cbz  x1, .Lrex_exc_abort\n"
        "    adrp x2, g_rex_exc_neon_fix\n"
        "    ldr  w3, [x2, :lo12:g_rex_exc_neon_fix]\n"
        "    cbz  w3, .Lrex_exc_neon_saved\n"
        "    adrp x2, g_rex_exc_neon_tmp\n"
        "    add  x2, x2, :lo12:g_rex_exc_neon_tmp\n"
        "    stp  q0, q1, [x2, #0]\n"
        "    stp  q2, q3, [x2, #32]\n"
        "    stp  q4, q5, [x2, #64]\n"
        "    stp  q6, q7, [x2, #96]\n"
        "    stp  q8, q9, [x2, #128]\n"
        "    stp  q10, q11, [x2, #160]\n"
        "    stp  q12, q13, [x2, #192]\n"
        "    stp  q14, q15, [x2, #224]\n"
        "    stp  q16, q17, [x2, #256]\n"
        "    stp  q18, q19, [x2, #288]\n"
        "    stp  q20, q21, [x2, #320]\n"
        "    stp  q22, q23, [x2, #352]\n"
        "    stp  q24, q25, [x2, #384]\n"
        "    stp  q26, q27, [x2, #416]\n"
        "    stp  q28, q29, [x2, #448]\n"
        "    stp  q30, q31, [x2, #480]\n"
        ".Lrex_exc_neon_saved:\n"
        "    ldr  x2, [x1, #88]\n"
        "    adrp x3, RexResumeTrap\n"
        "    add  x3, x3, :lo12:RexResumeTrap\n"
        "    cmp  x2, x3\n"
        "    b.eq .Lrex_exc_resume\n"
        /* debugger? svcGetInfo(InfoType_DebuggerAttached) preserving x9-x21 */
        "    stp  x9,  x10, [sp, #-16]!\n"
        "    stp  x11, x12, [sp, #-16]!\n"
        "    stp  x13, x14, [sp, #-16]!\n"
        "    stp  x15, x16, [sp, #-16]!\n"
        "    stp  x17, x18, [sp, #-16]!\n"
        "    stp  x19, x20, [sp, #-16]!\n"
        "    str  x21, [sp, #-16]!\n"
        "    stp  x0,  x1,  [sp, #-16]!\n"
        "    sub  sp, sp, #16\n"
        "    mov  x0, sp\n"
        "    mov  x1, #8\n"
        "    mov  w2, wzr\n"
        "    mov  x3, #0\n"
        "    bl   svcGetInfo\n"
        "    mov  w6, w0\n"
        "    ldr  x7, [sp], #16\n"
        "    ldp  x0,  x1,  [sp], #16\n"
        "    ldr  x21, [sp], #16\n"
        "    ldp  x19, x20, [sp], #16\n"
        "    ldp  x17, x18, [sp], #16\n"
        "    ldp  x15, x16, [sp], #16\n"
        "    ldp  x13, x14, [sp], #16\n"
        "    ldp  x11, x12, [sp], #16\n"
        "    ldp  x9,  x10, [sp], #16\n"
        "    cbnz w6, .Lrex_exc_abort\n"
        "    adrp x6, __nx_exception_ignoredebug\n"
        "    ldr  w5, [x6, :lo12:__nx_exception_ignoredebug]\n"
        "    cbnz w5, .Lrex_exc_dump\n"
        "    cbnz x7, .Lrex_exc_abort\n"
        /*
         * Take a free slot. Only x3-x7 are touched: x0 holds the exception type and x1
         * the kernel frame, and both are needed below.
         */
        ".Lrex_exc_dump:\n"
        "    adrp x6, g_rex_exc_busy\n"
        "    add  x6, x6, :lo12:g_rex_exc_busy\n"
        "    mov  w7, wzr\n"
        ".Lrex_exc_requests:\n"
        "    add  x5, x6, x7, lsl #2\n"
        "    ldaxr w4, [x5]\n"
        "    cbnz w4, .Lrex_exc_next\n"
        "    mov  w3, #1\n"
        "    stlxr w4, w3, [x5]\n"
        "    cbz  w4, .Lrex_exc_taken\n"
        "    b    .Lrex_exc_requests\n"
        ".Lrex_exc_next:\n"
        "    clrex\n"
        "    add  w7, w7, #1\n"
        "    cmp  w7, #8\n"
        "    b.lo .Lrex_exc_requests\n"
        /*
         * All taken: go back to the first one and wait. A thread stalled here is far
         * less harmful than two writing to the same dump.
         */
        "    mov  w7, wzr\n"
        "    b    .Lrex_exc_requests\n"
        ".Lrex_exc_taken:\n"
        /*
         * This slot's dump, with the slot number in pad[0], which the resume branch
         * reads from the copy to release it.
         */
        "    adrp x2, g_rex_exc_dumped\n"
        "    add  x2, x2, :lo12:g_rex_exc_dumped\n"
        "    add  x2, x2, x7, lsl #10\n"
        "    mov  x5, x2\n"
        "    str  w0,  [x2], #4\n"
        "    str  w7,  [x2], #4\n"
        "    str  wzr, [x2], #4\n"
        "    str  wzr, [x2], #4\n"
        "    ldp  x3, x4, [x1]\n"
        "    str  x5, [x1], #16\n"
        "    stp  x3, x4, [x2], #16\n"
        "    ldp  x3, x4, [x1], #16\n"
        "    stp  x3, x4, [x2], #16\n"
        "    ldp  x3, x4, [x1], #16\n"
        "    stp  x3, x4, [x2], #16\n"
        "    ldp  x3, x4, [x1], #16\n"
        "    stp  x3, x4, [x2], #16\n"
        "    ldr  x3, [x1], #8\n"
        "    str  x3, [x2], #8\n"
        "    str  x9, [x2], #8\n"
        "    stp  x10, x11, [x2], #16\n"
        "    stp  x12, x13, [x2], #16\n"
        "    stp  x14, x15, [x2], #16\n"
        "    stp  x16, x17, [x2], #16\n"
        "    stp  x18, x19, [x2], #16\n"
        "    stp  x20, x21, [x2], #16\n"
        "    stp  x22, x23, [x2], #16\n"
        "    stp  x24, x25, [x2], #16\n"
        "    stp  x26, x27, [x2], #16\n"
        "    str  x28, [x2], #8\n"
        "    str  x29, [x2], #8\n"
        "    ldr  x3, [x1], #8\n"
        "    str  x3, [x2], #8\n"
        /* Top of this slot's stack: base + (slot + 1) * 64 KB. */
        "    adrp x4, g_rex_exc_stacks\n"
        "    add  x4, x4, :lo12:g_rex_exc_stacks\n"
        "    add  x7, x7, #1\n"
        "    add  x4, x4, x7, lsl #16\n"
        "    ldr  x3, [x1]\n"
        "    str  x4, [x1], #8\n"
        "    str  x3, [x2], #8\n"
        "    adrp x4, RexExceptionReturnEntry\n"
        "    add  x4, x4, :lo12:RexExceptionReturnEntry\n"
        "    ldr  x3, [x1]\n"
        "    str  x4, [x1], #8\n"
        "    str  x3, [x2], #8\n"
        "    str  xzr, [x2], #8\n"
        "    adrp x3, g_rex_exc_neon_fix\n"
        "    ldr  w3, [x3, :lo12:g_rex_exc_neon_fix]\n"
        "    cbz  w3, .Lrex_exc_neon_live\n"
        "    adrp x3, g_rex_exc_neon_tmp\n"
        "    add  x3, x3, :lo12:g_rex_exc_neon_tmp\n"
        "    ldp  q0, q1, [x3, #0]\n"
        "    ldp  q2, q3, [x3, #32]\n"
        "    ldp  q4, q5, [x3, #64]\n"
        "    ldp  q6, q7, [x3, #96]\n"
        "    ldp  q8, q9, [x3, #128]\n"
        "    ldp  q10, q11, [x3, #160]\n"
        "    ldp  q12, q13, [x3, #192]\n"
        "    ldp  q14, q15, [x3, #224]\n"
        "    ldp  q16, q17, [x3, #256]\n"
        "    ldp  q18, q19, [x3, #288]\n"
        "    ldp  q20, q21, [x3, #320]\n"
        "    ldp  q22, q23, [x3, #352]\n"
        "    ldp  q24, q25, [x3, #384]\n"
        "    ldp  q26, q27, [x3, #416]\n"
        "    ldp  q28, q29, [x3, #448]\n"
        "    ldp  q30, q31, [x3, #480]\n"
        ".Lrex_exc_neon_live:\n"
        "    stp  q0,  q1,  [x2], #32\n"
        "    stp  q2,  q3,  [x2], #32\n"
        "    stp  q4,  q5,  [x2], #32\n"
        "    stp  q6,  q7,  [x2], #32\n"
        "    stp  q8,  q9,  [x2], #32\n"
        "    stp  q10, q11, [x2], #32\n"
        "    stp  q12, q13, [x2], #32\n"
        "    stp  q14, q15, [x2], #32\n"
        "    stp  q16, q17, [x2], #32\n"
        "    stp  q18, q19, [x2], #32\n"
        "    stp  q20, q21, [x2], #32\n"
        "    stp  q22, q23, [x2], #32\n"
        "    stp  q24, q25, [x2], #32\n"
        "    stp  q26, q27, [x2], #32\n"
        "    stp  q28, q29, [x2], #32\n"
        "    stp  q30, q31, [x2], #32\n"
        "    ldr  w3, [x1], #4\n"
        "    str  w3, [x2], #4\n"
        "    ldr  w3, [x1], #4\n"
        "    str  w3, [x2], #4\n"
        "    ldr  w3, [x1], #4\n"
        "    str  w3, [x2], #4\n"
        "    ldr  w3, [x1], #4\n"
        "    str  w3, [x2], #4\n"
        "    ldr  x3, [x1], #8\n"
        "    str  x3, [x2], #8\n"
        "    mov  w0, wzr\n"
        "    b    .Lrex_exc_end\n"
        /* resume: frame <- copied dump (x8 of the frame) */
        ".Lrex_exc_resume:\n"
        "    ldr  x2, [x1, #64]\n"
        /*
         * Release the slot. The thread no longer runs on the exception stack here
         * (it is inside the kernel, about to return to the faulting code), so another
         * thread can take it without overwriting anything. The mask is in case the
         * copied dump were damaged.
         */
        "    ldr  w3, [x2, #4]\n"
        "    and  w3, w3, #7\n"
        "    adrp x4, g_rex_exc_busy\n"
        "    add  x4, x4, :lo12:g_rex_exc_busy\n"
        "    add  x4, x4, x3, lsl #2\n"
        "    stlr wzr, [x4]\n"
        "    ldp  x3, x4, [x2, #16]\n"
        "    stp  x3, x4, [x1, #0]\n"
        "    ldp  x3, x4, [x2, #32]\n"
        "    stp  x3, x4, [x1, #16]\n"
        "    ldp  x3, x4, [x2, #48]\n"
        "    stp  x3, x4, [x1, #32]\n"
        "    ldp  x3, x4, [x2, #64]\n"
        "    stp  x3, x4, [x1, #48]\n"
        "    ldr  x3, [x2, #80]\n"
        "    str  x3, [x1, #64]\n"
        "    ldp  x3, x4, [x2, #256]\n"
        "    stp  x3, x4, [x1, #72]\n"
        "    ldr  x3, [x2, #272]\n"
        "    str  x3, [x1, #88]\n"
        "    ldr  w3, [x2, #800]\n"
        "    str  w3, [x1, #96]\n"
        "    adrp x3, g_rex_exc_neon_fix\n"
        "    ldr  w3, [x3, :lo12:g_rex_exc_neon_fix]\n"
        "    cbz  w3, .Lrex_exc_resume_go\n"
        "    add  x3, x2, #288\n"
        "    ldp  q0, q1, [x3, #0]\n"
        "    ldp  q2, q3, [x3, #32]\n"
        "    ldp  q4, q5, [x3, #64]\n"
        "    ldp  q6, q7, [x3, #96]\n"
        "    ldp  q8, q9, [x3, #128]\n"
        "    ldp  q10, q11, [x3, #160]\n"
        "    ldp  q12, q13, [x3, #192]\n"
        "    ldp  q14, q15, [x3, #224]\n"
        "    ldp  q16, q17, [x3, #256]\n"
        "    ldp  q18, q19, [x3, #288]\n"
        "    ldp  q20, q21, [x3, #320]\n"
        "    ldp  q22, q23, [x3, #352]\n"
        "    ldp  q24, q25, [x3, #384]\n"
        "    ldp  q26, q27, [x3, #416]\n"
        "    ldp  q28, q29, [x3, #448]\n"
        "    ldp  q30, q31, [x3, #480]\n"
        ".Lrex_exc_resume_go:\n"
        "    mov  w0, wzr\n"
        "    b    .Lrex_exc_end\n"
        ".Lrex_exc_abort:\n"
        "    mov  w0, #0xf801\n"
        ".Lrex_exc_end:\n"
        "    bl   svcReturnFromException\n"
        "    b    .\n"
        ".size __libnx_exception_entry, .-__libnx_exception_entry\n"
        "\n"
        ".balign 4\n"
        ".type RexExceptionReturnEntry, %function\n"
        "RexExceptionReturnEntry:\n"
        "    bl   __libnx_exception_handler\n"
        "    mov  w0, wzr\n"
        "    mov  x1, #0\n"
        "    mov  x2, #0\n"
        "    bl   svcBreak\n"
        "    b    .\n"
        ".size RexExceptionReturnEntry, .-RexExceptionReturnEntry\n");

#define GOW3_CRASH_LOG "sdmc:/switch/gow3-nx/gow3_crash.log"
static uint32_t s_in_fatal = 0;
static uint64_t s_faultins = 0, s_dispatched = 0, s_demand = 0, s_raced = 0;

static void dump_to(FILE* f, const char* why, int sig, const ThreadExceptionDump* c) {
    uint64_t base = (uint64_t)(uintptr_t)&_start;
    fprintf(f, "[nx-exc] %s sig=%d desc=0x%x esr=0x%x far=0x%llx pc=0x%llx (img+0x%llx) lr=0x%llx (img+0x%llx) sp=0x%llx\n",
            why, sig, c->error_desc, c->esr, (unsigned long long)c->far.x,
            (unsigned long long)c->pc.x, (unsigned long long)(c->pc.x - base),
            (unsigned long long)c->lr.x, (unsigned long long)(c->lr.x - base), (unsigned long long)c->sp.x);
    uint8_t* gm = RexGmBase();
    if (gm && c->far.x >= (uint64_t)(uintptr_t)gm && c->far.x < (uint64_t)(uintptr_t)gm + RexGmSize())
        fprintf(f, "[nx-exc] far es guest 0x%08llx\n", (unsigned long long)(c->far.x - (uint64_t)(uintptr_t)gm));
    for (int i = 0; i < 29; i++)
        fprintf(f, "x%-2d=%016llx%s", i, (unsigned long long)c->cpu_gprs[i].x, (i % 4) == 3 ? "\n" : " ");
    fprintf(f, "fp =%016llx\n", (unsigned long long)c->fp.x);
    /* backtrace por cadena de fp, solo mientras quede dentro de 1 MB sobre el sp (no leer basura) */
    uint64_t fp = c->fp.x, lo = c->sp.x, hi = c->sp.x + 0x100000;
    for (int i = 0; i < 24 && fp >= lo && fp + 16 <= hi && !(fp & 7); i++) {
        uint64_t nfp = ((uint64_t*)fp)[0], ret = ((uint64_t*)fp)[1];
        fprintf(f, "[nx-exc]  #%d img+0x%llx\n", i, (unsigned long long)(ret - base));
        if (nfp <= fp) break;
        fp = nfp;
    }
    fprintf(f, "[nx-exc] faultins=%llu demanda=%llu despachadas=%llu carreras=%llu comprometido=%llu MB gm_err=0x%x\n",
            (unsigned long long)s_faultins, (unsigned long long)s_demand, (unsigned long long)s_dispatched, (unsigned long long)s_raced,
            (unsigned long long)(RexGmCommittedBytes() >> 20), RexGmLastResult());
    fflush(f);
}

static void fatal(const char* why, int sig, const ThreadExceptionDump* c) {
    if (__atomic_exchange_n(&s_in_fatal, 1, __ATOMIC_SEQ_CST)) return;   /* falta dentro del volcado: svcBreak directo */
    dump_to(stderr, why, sig, c);
    fflush(stdout);
    FILE* f = fopen(GOW3_CRASH_LOG, "w");
    if (f) { dump_to(f, why, sig, c); fclose(f); }
}

static int sig_of(const ThreadExceptionDump* c) {
    unsigned ec = (c->esr >> 26) & 0x3F;
    switch (ec) {
        case 0x24: case 0x25:                     /* data abort */
            return (c->esr & 0x3F) == 0x21 ? SIGBUS : SIGSEGV;
        case 0x20: case 0x21: return SIGSEGV;     /* instruction abort */
        case 0x22: case 0x26: return SIGBUS;      /* pc / sp desalineados */
        case 0x00: case 0x0E: return SIGILL;      /* udf, estado ilegal */
        case 0x28: case 0x2C: return SIGFPE;
        case 0x3C: return SIGTRAP;                /* brk */
        default:   return SIGSEGV;
    }
}

/* libnx llama a esto en el stack de excepcion del slot. Volver = svcBreak (RexExceptionReturnEntry). */
extern "C" void __libnx_exception_handler(ThreadExceptionDump* ctx) {
    unsigned ec = (ctx->esr >> 26) & 0x3F;
    /* 1. Pagina guest comprometida pero sin mapear: se mapea y se reintenta. */
    if ((ec == 0x24 || ec == 0x25) && RexGmFaultIn(ctx->far.x)) {
        s_faultins++;
        RexResumeFromException(ctx);
    }
    /* 1b. Paginado por demanda, como el VEH de Windows en main.cpp: la ventana es todo el espacio de
     * 32 bits, y cualquier direccion guest sin comprometer se compromete (bloque de 2 MB) y se mapea. */
    if (ec == 0x24 || ec == 0x25) {
        size_t off = 0;
        uint8_t* gm = RexGmBase();
        if (gm && ctx->far.x >= (uint64_t)(uintptr_t)gm && RexGmWindowToOffset(ctx->far.x, &off) &&
            RexGmCommit(off, 1, REX_GM_WRITE) && RexGmFaultIn(ctx->far.x)) {
            s_demand++;
            RexResumeFromException(ctx);
        }
        /* 1c. Carrera: otro hilo comprometio y mapeo el mismo bloque entre el fallo y este
         * handler, asi que FaultIn contesta "ya esta". Si la pagina ya tiene el permiso que
         * pedia el acceso (WnR = bit 6 del ESR), basta con reintentar. */
        if (gm && ctx->far.x >= (uint64_t)(uintptr_t)gm) {
            MemoryInfo mi; u32 pi;
            if (R_SUCCEEDED(svcQueryMemory(&mi, &pi, ctx->far.x))) {
                const bool wr = (ctx->esr >> 6) & 1;
                if ((mi.perm & Perm_R) && (!wr || (mi.perm & Perm_W))) {
                    s_raced++;
                    RexResumeFromException(ctx);
                }
            }
        }
    }
    /* 2. Handler registrado por el runtime (sigaction -> nx_sigaction). */
    int sig = sig_of(ctx);
    const struct nx_sigaction* act = nx_sigaction_get(sig);
    if (act && act->sa_handler && !__atomic_load_n(&s_in_fatal, __ATOMIC_SEQ_CST)) {
        struct nx_sigaction a = *act;
        if (a.sa_flags & SA_SIGINFO) {
            siginfo_t si; memset(&si, 0, sizeof si);
            si.si_signo = sig;
            si.si_code  = (sig == SIGBUS) ? BUS_ADRALN : SEGV_MAPERR;
            si.si_addr  = (void*)(uintptr_t)ctx->far.x;
            ucontext_t uc; memset(&uc, 0, sizeof uc);
            uc.uc_mcontext.fault_address = ctx->far.x;
            for (int i = 0; i < 29; i++) uc.uc_mcontext.regs[i] = ctx->cpu_gprs[i].x;
            uc.uc_mcontext.regs[29] = ctx->fp.x;
            uc.uc_mcontext.regs[30] = ctx->lr.x;
            uc.uc_mcontext.sp = ctx->sp.x;
            uc.uc_mcontext.pc = ctx->pc.x;
            uc.uc_mcontext.pstate = ctx->pstate;
            a.sa_sigaction(sig, &si, &uc);
            for (int i = 0; i < 29; i++) ctx->cpu_gprs[i].x = uc.uc_mcontext.regs[i];
            ctx->fp.x = uc.uc_mcontext.regs[29];
            ctx->lr.x = uc.uc_mcontext.regs[30];
            ctx->sp.x = uc.uc_mcontext.sp;
            ctx->pc.x = uc.uc_mcontext.pc;
            ctx->pstate = (u32)uc.uc_mcontext.pstate;
        } else {
            a.sa_handler(sig);
        }
        /* El runtime "no la manejo" lo expresa reponiendo SIG_DFL y volviendo, para que la instruccion
         * falle de nuevo. Aca eso seria un bucle: si quedo en default, es fatal. */
        const struct nx_sigaction* now = nx_sigaction_get(sig);
        if (now && now->sa_handler) {
            s_dispatched++;
            RexResumeFromException(ctx);
        }
        fatal("sin manejar (handler repuso SIG_DFL)", sig, ctx);
        return;
    }
    fatal("sin handler", sig, ctx);
}
#endif
