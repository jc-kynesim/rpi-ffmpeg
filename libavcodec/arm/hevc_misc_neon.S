#include "libavutil/arm/asm.S"
#include "neon.S"

@ rpi_zap_coeff_vals(
@   uint16_t * buf,          [r0]
@   unsigned int log_n_m2)   [r1]

function rpi_zap_coeff_vals, export=1
        vmov.i64 q8, #0
        adr     r12, zc_tab
        vmov.i64 q9, #0
        tst     r0, #63
        vmov.i64 q10, #0
        add     r0, #63
        vmov.i64 q11, #0
        and     r0, #~63
        ldr     pc, [r12, r1, lsl #2]

zc_tab:
        .word   zc_lc2
        .word   zc_lc3
        .word   zc_lc4
        .word   zc_lc5

@ 4*4*2: "32 bytes" 64 or 0 depending on dst address
zc_lc2:
        vstmeq  r0, {q8-q11}
        bx      lr

@ 16*16*2 = 512 = 64 * 8
zc_lc4:
        vstm    r0!, {q8-q11}
        vstm    r0!, {q8-q11}
        vstm    r0!, {q8-q11}
        vstm    r0!, {q8-q11}
        vstm    r0!, {q8-q11}
        vstm    r0!, {q8-q11}
@ 8*8*2 = 128
zc_lc3:
        vstm    r0!, {q8-q11}
        vstm    r0,  {q8-q11}
        bx      lr

@ 32*32*2 = 2048 = 128 * 16
zc_lc5:
        vmov.i64 q12, #0
        vmov.i64 q13, #0
        vmov.i64 q14, #0
        vmov.i64 q15, #0
        mov     r2, #4
1:
        vstm    r0!, {q8-q15}
        subs    r2, #1
        vstm    r0!, {q8-q15}
        vstm    r0!, {q8-q15}
        vstm    r0!, {q8-q15}
        bne     1b
        bx      lr

endfunc

