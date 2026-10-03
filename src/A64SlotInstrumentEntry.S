#include "A64SlotInstrumentLayout.h"

.text
.p2align 2

.global A64SlotInstrumentEntry
.type A64SlotInstrumentEntry, %function
.hidden A64SlotInstrumentEntry
A64SlotInstrumentEntry:
    .inst 0xD50324DF

    sub sp, sp, #A64_FRAME_SIZE

    stp x0, x1, [sp, #A64_CTX_X0]
    stp x2, x3, [sp, #A64_CTX_X2]
    stp x4, x5, [sp, #A64_CTX_X4]
    stp x6, x7, [sp, #A64_CTX_X6]
    stp x8, x9, [sp, #A64_CTX_X8]
    stp x10, x11, [sp, #A64_CTX_X10]
    stp x12, x13, [sp, #A64_CTX_X12]
    stp x14, x15, [sp, #A64_CTX_X14]

    ldr x9, [sp, #A64_FRAME_SIZE]
    str x9, [sp, #A64_CTX_X16]
    ldr x9, [sp, #(A64_FRAME_SIZE + 8)]
    str x9, [sp, #A64_CTX_X17]

    str x18, [sp, #A64_CTX_X18]
    stp x19, x20, [sp, #A64_CTX_X19]
    stp x21, x22, [sp, #A64_CTX_X21]
    stp x23, x24, [sp, #A64_CTX_X23]
    stp x25, x26, [sp, #A64_CTX_X25]
    stp x27, x28, [sp, #A64_CTX_X27]
    str x29, [sp, #A64_CTX_X29]
    str x30, [sp, #A64_CTX_LR]

    add x9, sp, #A64_TOTAL_STACK
    str x9, [sp, #A64_CTX_SP]

    mrs x9, nzcv
    str x9, [sp, #A64_CTX_NZCV]
    mrs x9, fpcr
    str x9, [sp, #A64_CTX_FPCR]
    mrs x9, fpsr
    str x9, [sp, #A64_CTX_FPSR]

    stp q0, q1, [sp, #A64_CTX_V0]
    stp q2, q3, [sp, #A64_CTX_V2]
    stp q4, q5, [sp, #A64_CTX_V4]
    stp q6, q7, [sp, #A64_CTX_V6]
    stp q8, q9, [sp, #A64_CTX_V8]
    stp q10, q11, [sp, #A64_CTX_V10]
    stp q12, q13, [sp, #A64_CTX_V12]
    stp q14, q15, [sp, #A64_CTX_V14]
    stp q16, q17, [sp, #A64_CTX_V16]
    stp q18, q19, [sp, #A64_CTX_V18]
    stp q20, q21, [sp, #A64_CTX_V20]
    stp q22, q23, [sp, #A64_CTX_V22]
    stp q24, q25, [sp, #A64_CTX_V24]
    stp q26, q27, [sp, #A64_CTX_V26]
    stp q28, q29, [sp, #A64_CTX_V28]
    stp q30, q31, [sp, #A64_CTX_V30]

    mov x0, x16
    mov x1, sp
    bl A64SlotInstrumentDispatch
    str w0, [sp, #A64_DECISION]

    ldr x9, [sp, #A64_DECISION]
    cbz w9, .Lcall_original

    ldp q0, q1, [sp, #A64_CTX_V0]
    ldp q2, q3, [sp, #A64_CTX_V2]
    ldp q4, q5, [sp, #A64_CTX_V4]
    ldp q6, q7, [sp, #A64_CTX_V6]
    ldp q8, q9, [sp, #A64_CTX_V8]
    ldp q10, q11, [sp, #A64_CTX_V10]
    ldp q12, q13, [sp, #A64_CTX_V12]
    ldp q14, q15, [sp, #A64_CTX_V14]
    ldp q16, q17, [sp, #A64_CTX_V16]
    ldp q18, q19, [sp, #A64_CTX_V18]
    ldp q20, q21, [sp, #A64_CTX_V20]
    ldp q22, q23, [sp, #A64_CTX_V22]
    ldp q24, q25, [sp, #A64_CTX_V24]
    ldp q26, q27, [sp, #A64_CTX_V26]
    ldp q28, q29, [sp, #A64_CTX_V28]
    ldp q30, q31, [sp, #A64_CTX_V30]

    ldr x9, [sp, #A64_CTX_FPCR]
    msr fpcr, x9
    ldr x9, [sp, #A64_CTX_FPSR]
    msr fpsr, x9
    ldr x9, [sp, #A64_CTX_NZCV]
    msr nzcv, x9

    ldp x0, x1, [sp, #A64_CTX_X0]
    ldp x2, x3, [sp, #A64_CTX_X2]
    ldp x4, x5, [sp, #A64_CTX_X4]
    ldp x6, x7, [sp, #A64_CTX_X6]
    ldp x8, x9, [sp, #A64_CTX_X8]
    ldp x10, x11, [sp, #A64_CTX_X10]
    ldp x12, x13, [sp, #A64_CTX_X12]
    ldp x14, x15, [sp, #A64_CTX_X14]
    ldr x16, [sp, #A64_CTX_X16]
    ldr x17, [sp, #A64_CTX_X17]
    ldr x18, [sp, #A64_CTX_X18]
    ldp x19, x20, [sp, #A64_CTX_X19]
    ldp x21, x22, [sp, #A64_CTX_X21]
    ldp x23, x24, [sp, #A64_CTX_X23]
    ldp x25, x26, [sp, #A64_CTX_X25]
    ldp x27, x28, [sp, #A64_CTX_X27]
    ldr x29, [sp, #A64_CTX_X29]
    ldr x30, [sp, #A64_CTX_LR]

    add sp, sp, #A64_FRAME_SIZE
    add sp, sp, #16
    mov x0, xzr
    ret

.Lcall_original:
    ldp q0, q1, [sp, #A64_CTX_V0]
    ldp q2, q3, [sp, #A64_CTX_V2]
    ldp q4, q5, [sp, #A64_CTX_V4]
    ldp q6, q7, [sp, #A64_CTX_V6]
    ldp q8, q9, [sp, #A64_CTX_V8]
    ldp q10, q11, [sp, #A64_CTX_V10]
    ldp q12, q13, [sp, #A64_CTX_V12]
    ldp q14, q15, [sp, #A64_CTX_V14]
    ldp q16, q17, [sp, #A64_CTX_V16]
    ldp q18, q19, [sp, #A64_CTX_V18]
    ldp q20, q21, [sp, #A64_CTX_V20]
    ldp q22, q23, [sp, #A64_CTX_V22]
    ldp q24, q25, [sp, #A64_CTX_V24]
    ldp q26, q27, [sp, #A64_CTX_V26]
    ldp q28, q29, [sp, #A64_CTX_V28]
    ldp q30, q31, [sp, #A64_CTX_V30]

    ldr x9, [sp, #A64_CTX_FPCR]
    msr fpcr, x9
    ldr x9, [sp, #A64_CTX_FPSR]
    msr fpsr, x9
    ldr x9, [sp, #A64_CTX_NZCV]
    msr nzcv, x9

    ldp x0, x1, [sp, #A64_CTX_X0]
    ldp x2, x3, [sp, #A64_CTX_X2]
    ldp x4, x5, [sp, #A64_CTX_X4]
    ldp x6, x7, [sp, #A64_CTX_X6]
    ldp x8, x9, [sp, #A64_CTX_X8]
    ldp x10, x11, [sp, #A64_CTX_X10]
    ldp x12, x13, [sp, #A64_CTX_X12]
    ldp x14, x15, [sp, #A64_CTX_X14]
    ldr x16, [sp, #A64_CTX_X16]
    ldr x17, [sp, #A64_CTX_X17]
    ldr x18, [sp, #A64_CTX_X18]
    ldp x19, x20, [sp, #A64_CTX_X19]
    ldp x21, x22, [sp, #A64_CTX_X21]
    ldp x23, x24, [sp, #A64_CTX_X23]
    ldp x25, x26, [sp, #A64_CTX_X25]
    ldp x27, x28, [sp, #A64_CTX_X27]
    ldr x29, [sp, #A64_CTX_X29]
    ldr x30, [sp, #A64_CTX_LR]

    ldr x16, [sp, #A64_CTX_ORIGINAL]
    add sp, sp, #A64_FRAME_SIZE
    add sp, sp, #16
    br x16

.size A64SlotInstrumentEntry, .-A64SlotInstrumentEntry
.section .note.GNU-stack,"",%progbits
