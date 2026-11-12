/* regs[i] en 48+8*i; sp 296; pc 304; d8..d15 en 320 */
    .text
    .global getcontext
    .type getcontext,%function
getcontext:
    str xzr, [x0, #48]
    stp x19, x20, [x0, #200]
    stp x21, x22, [x0, #216]
    stp x23, x24, [x0, #232]
    stp x25, x26, [x0, #248]
    stp x27, x28, [x0, #264]
    stp x29, x30, [x0, #280]
    mov x2, sp
    str x2, [x0, #296]
    str x30, [x0, #304]
    stp d8, d9, [x0, #320]
    stp d10, d11, [x0, #336]
    stp d12, d13, [x0, #352]
    stp d14, d15, [x0, #368]
    mov w0, #0
    ret
    .global setcontext
    .type setcontext,%function
setcontext:
    ldp x19, x20, [x0, #200]
    ldp x21, x22, [x0, #216]
    ldp x23, x24, [x0, #232]
    ldp x25, x26, [x0, #248]
    ldp x27, x28, [x0, #264]
    ldp x29, x30, [x0, #280]
    ldr x2, [x0, #296]
    mov sp, x2
    ldp d8, d9, [x0, #320]
    ldp d10, d11, [x0, #336]
    ldp d12, d13, [x0, #352]
    ldp d14, d15, [x0, #368]
    ldr x16, [x0, #304]
    ldp x2, x3, [x0, #64]
    ldp x4, x5, [x0, #80]
    ldp x6, x7, [x0, #96]
    ldr x1, [x0, #56]
    ldr x0, [x0, #48]
    br x16
    .global swapcontext
    .type swapcontext,%function
swapcontext:
    str xzr, [x0, #48]
    stp x19, x20, [x0, #200]
    stp x21, x22, [x0, #216]
    stp x23, x24, [x0, #232]
    stp x25, x26, [x0, #248]
    stp x27, x28, [x0, #264]
    stp x29, x30, [x0, #280]
    mov x2, sp
    str x2, [x0, #296]
    str x30, [x0, #304]
    stp d8, d9, [x0, #320]
    stp d10, d11, [x0, #336]
    stp d12, d13, [x0, #352]
    stp d14, d15, [x0, #368]
    mov x0, x1
    b setcontext
