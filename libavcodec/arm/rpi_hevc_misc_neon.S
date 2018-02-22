#include "libavutil/arm/asm.S"
#include "neon.S"

@ rpi_zap_coeff_vals_neon(
@   uint16_t * buf,          [r0]
@   unsigned int log_n_m2)   [r1]

function rpi_zap_coeff_vals_neon, export=1
        mov      ip, #1
        vmov.i64 q0, #0
        teq      r1, #0
        vmov.i64 q1, #0
        beq      2f

        lsl      ip, r1    @ 2, 4 or 8
        add      r2, r0, #32
        lsl      ip, r1    @ 4, 16 or 64 = number of 32-byte blocks to zero
        mov      r3, #64
1:      vst1.8   {q0,q1}, [r0:256], r3
        subs     ip, #2
        vst1.8   {q0,q1}, [r2:256], r3
        bne      1b
        bx       lr

2:      vst1.8   {q0,q1}, [r0:256]
        bx       lr
