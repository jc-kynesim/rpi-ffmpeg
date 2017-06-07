#include "rpi_shader.h"

#ifdef _MSC_VER
   #include <stdint.h>
   /* cast through uintptr_t to avoid warnings */
   #define POINTER_TO_UINT(X) ((unsigned int)(uintptr_t)(X))
#else
   #define POINTER_TO_UINT(X) ((unsigned int)(X))
#endif

#ifdef __cplusplus
extern "C" { /* the types are probably wrong... */
#endif
#ifdef __cplusplus
}
#endif

#ifdef _MSC_VER
__declspec(align(8))
#elif defined(__GNUC__)
__attribute__((aligned(8)))
#endif
unsigned int rpi_shader[] = {
// ::mc_setup_c_q0
// ::mc_start
/* [0x00000000] */ 0x0000000c, 0xe80009e7, // mov  dst, srel(i)
// ::mc_setup_c_qn
/* [0x00000008] */ 0x00000001, 0xe0020927, // mov tmurs, 1
/* [0x00000010] */ 0x15827d80, 0x10020027, // mov ra0, unif
/* [0x00000018] */ 0x15827d80, 0x10020627, // mov ra_base, unif
/* [0x00000020] */ 0x0d801dc0, 0xd0020827, // sub r0, unif, 1
/* [0x00000028] */ 0x0c9e7000, 0x10021667, // add rb_max_x, r0, r0
/* [0x00000030] */ 0x0d801dc0, 0xd00217a7, // sub rb_max_y, unif, 1
/* [0x00000038] */ 0xff100100, 0xe0020527, // mov ra_kff100100, 0xff100100
/* [0x00000040] */ 0x000000ff, 0xe00215a7, // mov rb_k255, 255
/* [0x00000048] */ 0x00000000, 0xe0024104, // mov ra4, 0 ; mov rb4, 0
/* [0x00000050] */ 0x00000000, 0xe0024145, // mov ra5, 0 ; mov rb5, 0
/* [0x00000058] */ 0x00000000, 0xe0024186, // mov ra6, 0 ; mov rb6, 0
/* [0x00000060] */ 0x00000000, 0xe00241c7, // mov ra7, 0 ; mov rb7, 0
/* [0x00000068] */ 0xc0000000, 0xe0020867, // mov r1, vdw_setup_1(0)
/* [0x00000070] */ 0x95800dbf, 0xd002550c, // mov rb_xpitch, unif   ; mov ra12, 0
/* [0x00000078] */ 0x95800dbf, 0xd002540d, // mov rb_pitch, unif    ; mov ra13, 0
/* [0x00000080] */ 0x00000000, 0xe00059ce, // nop                   ; mov ra14, 0
/* [0x00000088] */ 0x8c5103f6, 0x1802560f, // add rb_dma1_base, r1, rb_pitch ; mov ra15, ra_k0
/* [0x00000090] */ 0x14981f80, 0xd0020827, // and r0, 1, elem_num
/* [0x00000098] */ 0x409c7007, 0xd00049e0, // nop                   ; mul24 r0, r0, 7
/* [0x000000a0] */ 0x0c9a7180, 0x100210a7, // add rb_elem_x, r0, elem_num
/* [0x000000a8] */ 0x0c027d80, 0x14020827, // add r0, ra0.16b, ra0.16b
/* [0x000000b0] */ 0x0c9c21c0, 0x10020827, // add r0, r0, rb_elem_x
/* [0x000000b8] */ 0x930001f6, 0xd2225811, // max r0, r0, 0         ; mov ra_y, ra0.16a
/* [0x000000c0] */ 0x129d91c0, 0x10020827, // min r0, r0, rb_max_x
/* [0x000000c8] */ 0x119c31c0, 0xd0220567, // shl ra_xshift_next, r0, 3
/* [0x000000d0] */ 0x149dc1c0, 0xd0020827, // and r0, r0, -4
/* [0x000000d8] */ 0x0d510dc0, 0x18020867, // sub r1, ra_k0, rb_pitch
/* [0x000000e0] */ 0x149e7040, 0x10020867, // and r1, r0, r1
/* [0x000000e8] */ 0x569d404f, 0x10024821, // xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
/* [0x000000f0] */ 0x0c9e7040, 0x10020827, // add r0, r0, r1
/* [0x000000f8] */ 0x0c627c00, 0x10020627, // add ra_base, ra_base, r0
/* [0x00000100] */ 0x0c809f80, 0xd0021367, // add rb_wt_den_p15, 9, unif
/* [0x00000108] */ 0x159e6fc0, 0x100208a7, // mov r2, qpu_num
/* [0x00000110] */ 0x0f9c25c0, 0xd0020867, // asr r1, r2, 2
/* [0x00000118] */ 0x119c63c0, 0xd0020867, // shl r1, r1, 6
/* [0x00000120] */ 0x149c35c0, 0xd0020827, // and r0, r2, 3
/* [0x00000128] */ 0x159e7040, 0x10020827, // or  r0, r0, r1
/* [0x00000130] */ 0x00004800, 0xe0020867, // mov r1, vpm_setup(0, 4, h8p(0, 0))
/* [0x00000138] */ 0x0c9e7040, 0x10021727, // add r_vpm, r0, r1
/* [0x00000140] */ 0x80004004, 0xe0020867, // mov r1, vdw_setup_0(0, 0, dma_h8p(0,0,0))
/* [0x00000148] */ 0x119c51c0, 0xd0020827, // shl r0, r0, 5
/* [0x00000150] */ 0x0c9e7040, 0x100216e7, // add r_dma, r0, r1
/* [0x00000158] */ 0x15827d80, 0x10020027, // mov ra0, unif
/* [0x00000160] */ 0x15827d80, 0x10020667, // mov ra_base2, unif
/* [0x00000168] */ 0x0c027d80, 0x14020827, // add r0, ra0.16b, ra0.16b
/* [0x00000170] */ 0x8c0021f6, 0x12125811, // add r0, r0, rb_elem_x ; mov ra_y2, ra0.16a
/* [0x00000178] */ 0x139c01c0, 0xd0020827, // max r0, r0, 0
/* [0x00000180] */ 0x129d91c0, 0x10020827, // min r0, r0, rb_max_x
/* [0x00000188] */ 0x119c31c0, 0xd0021067, // shl rb_xshift2_next, r0, 3
/* [0x00000190] */ 0x149dc1c0, 0xd0020827, // and r0, r0, -4
/* [0x00000198] */ 0x0d510dc0, 0x18020867, // sub r1, ra_k0, rb_pitch
/* [0x000001a0] */ 0x149e7040, 0x10020867, // and r1, r0, r1
/* [0x000001a8] */ 0x569d404f, 0x10024821, // xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
/* [0x000001b0] */ 0x8c467076, 0x12024822, // add r0, r0, r1        ; mov r2, ra_y2
/* [0x000001b8] */ 0x0c667c00, 0x10020667, // add ra_base2, ra_base2, r0
/* [0x000001c0] */ 0x95442ff6, 0xd40248e0, // mov r3, PREREAD       ; mov r0, ra_y
/* [0x000001c8] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
// :c_preload
/* [0x000001d0] */ 0x0d9c17c0, 0xd00228e7, // sub.setf r3, r3, 1
/* [0x000001d8] */ 0x139c01c0, 0xd0020867, // max r1, r0, 0
/* [0x000001e0] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_max_y
/* [0x000001e8] */ 0x4c51018f, 0x1a024821, // add r0, r0, ra_k1     ; mul24 r1, r1, rb_pitch
/* [0x000001f0] */ 0x0c627c40, 0x10020e27, // add t0s, ra_base, r1
/* [0x000001f8] */ 0x139c05c0, 0xd0020867, // max r1, r2, 0
/* [0x00000200] */ 0xffffffb0, 0xf03809e7, // brr.anynz -, r:c_preload
/* [0x00000208] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_max_y
/* [0x00000210] */ 0x4c51058f, 0x1a0248a1, // add r2, r2, ra_k1     ; mul24 r1, r1, rb_pitch
/* [0x00000218] */ 0x0c667c40, 0x10020f27, // add t1s, ra_base2, r1
/* [0x00000220] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000228] */ 0x159e7000, 0x10220467, // mov ra_y, r0
/* [0x00000230] */ 0x159e7480, 0x10120467, // mov ra_y2, r2
/* [0x00000238] */ 0x009e7000, 0x100009e7, // nop
// ::mc_filter_uv
/* [0x00000240] */ 0x159dcfc0, 0x10021c67, // mov vw_setup, rb_vpm_init
/* [0x00000248] */ 0x15827d80, 0x100200a7, // mov ra2, unif
/* [0x00000250] */ 0x14981dc0, 0xd00229e7, // and.setf -, elem_num, 1
/* [0x00000258] */ 0xec0a7d89, 0x14024821, // add r0, ra2.16b, ra2.16b ; v8subs r1, r1, r1
/* [0x00000260] */ 0x0c9c21c0, 0x10020827, // add r0, r0, rb_elem_x
/* [0x00000268] */ 0x8d8103f6, 0x10024863, // sub r1, r1, rb_pitch  ; mov r3, unif
/* [0x00000270] */ 0x935401f6, 0xd4125815, // max r0, r0, 0         ; mov ra_xshift, ra_xshift_next
/* [0x00000278] */ 0x928191f6, 0x10025801, // min r0, r0, rb_max_x  ; mov ra1, unif
/* [0x00000280] */ 0x119c31c0, 0xd0220567, // shl ra_xshift_next, r0, 3
/* [0x00000288] */ 0x9481c1f6, 0xd0025800, // and r0, r0, -4        ; mov ra0, unif
/* [0x00000290] */ 0x800a7036, 0x122059d3, // nop                   ; mov ra_y_next, ra2.16a
/* [0x00000298] */ 0x54042077, 0xd4024862, // and r1, r0, r1        ; mul24 r2, ra1.16b, 2
/* [0x000002a0] */ 0x569d404f, 0x10024821, // xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
/* [0x000002a8] */ 0x8c067076, 0x12024821, // add r0, r0, r1        ; mov r1, ra1.16a
/* [0x000002b0] */ 0x0c9e7600, 0x100206a7, // add ra_base_next, r3, r0
/* [0x000002b8] */ 0x119c73c0, 0xd0020827, // shl r0, r1, 7
/* [0x000002c0] */ 0x8d818eb6, 0x10025743, // sub rb_dma1, rb_dma1_base, r2 ; mov ra3, unif
/* [0x000002c8] */ 0x8c8013f6, 0xd0025456, // add rb_i_tmu, r1, 3 - PREREAD ; mov ra_wt_off_mul_l0, unif
/* [0x000002d0] */ 0x8c8033f6, 0xd002d496, // add rb_lcount, r1, 3  ; mov.ifnz ra_wt_off_mul_l0, unif
/* [0x000002d8] */ 0x8c0e70b6, 0x18024808, // add r0, r0, r2      ; mov rb8,  ra3.8a
/* [0x000002e0] */ 0x910d01f6, 0xda024809, // shl r0, r0, i_shift16 ; mov rb9,  ra3.8b
/* [0x000002e8] */ 0x8c59b1f6, 0x140256a1, // add rb_dma0, r0, rb_dma0_base ; mov r1, ra_wt_off_l0
/* [0x000002f0] */ 0x9581edbf, 0x100255c9, // mov rb_dest, unif     ; mov ra9, rb_max_y
/* [0x000002f8] */ 0x910cd3f6, 0x1c02484a, // shl r1, r1, rb_wt_den_p15 ; mov rb10, ra3.8c
/* [0x00000300] */ 0x950c0ff6, 0xde02494b, // mov r5quad, 0             ; mov rb11, ra3.8d
/* [0x00000308] */ 0x8f8013f6, 0xd002531e, // asr rb_wt_off, r1, 1  ; mov ra_link, unif
/* [0x00000310] */ 0x11581dc0, 0xd21205a7, // shl ra_wt_mul_l0, ra_wt_mul_l0, 1
// :uvloop
/* [0x00000318] */ 0xcd511bee, 0xaa0269e5, // sub.setf -, r5, rb_i_tmu ; v8adds r5rep, r5, ra_k1 ; ldtmu0
/* [0x00000320] */ 0x0e567980, 0x120208a7, // shr r2, r4, ra_xshift
/* [0x00000328] */ 0x804e7036, 0x140089e3, // nop                   ; mov.ifz r3, ra_y_next
/* [0x00000330] */ 0x8e4485f6, 0xd402c863, // shr r1, r2, 8         ; mov.ifnz r3, ra_y
/* [0x00000338] */ 0x8c6817f6, 0xd0029818, // add r0, r3, 1         ; mov.ifz ra_base, ra_base_next
/* [0x00000340] */ 0x94981f80, 0xd02279d1, // and.setf -, 1, elem_num ; mov ra_y, r0
/* [0x00000348] */ 0x93531789, 0xd80248e0, // max r3, r3, ra_k0     ; mov      r0, r1 << 15
/* [0x00000350] */ 0x9227f792, 0xd00288e1, // min r3, r3, ra9       ; mov.ifz  r1, r2 << 1
/* [0x00000358] */ 0x559d049f, 0x10044822, // mov.ifz r0, r2        ; mul24 r2, r3, rb_pitch
/* [0x00000360] */ 0x8c616c87, 0x10024e20, // add t0s, ra_base, r2  ; v8min r0, r0, rb_k255
/* [0x00000368] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x00000370] */ 0x540163f0, 0x18024863, // and r1, r1, rb_k255   ; mul24      r3, ra0.8a,       r0
/* [0x00000378] */ 0x4003e030, 0xda0049e2, // nop                   ; mul24      r2, ra0.8b << 2,  r0 << 2  @ "mul_used", 0
/* [0x00000380] */ 0x40038031, 0xd800c9e3, // nop                   ; mul24.ifnz r3, ra0.8a << 8,  r1 << 8  @ "mul_used", 0
/* [0x00000388] */ 0x40036031, 0xda00c9e2, // nop                   ; mul24.ifnz r2, ra0.8b << 10, r1 << 10 @ "mul_used", 0
/* [0x00000390] */ 0x4d03c4f0, 0xdc0248a3, // sub r2, r2, r3        ; mul24      r3, ra0.8c << 4,  r0 << 4  @ "mul_used", 0
/* [0x00000398] */ 0x40034031, 0xdc00c9e3, // nop                   ; mul24.ifnz r3, ra0.8c << 12, r1 << 12 @ "mul_used", 0
/* [0x000003a0] */ 0x4c03a4f0, 0xde0248a3, // add r2, r2, r3        ; mul24      r3, ra0.8d << 6,  r0 << 6  @ "mul_used", 0
/* [0x000003a8] */ 0x40032031, 0xde00c9e3, // nop                   ; mul24.ifnz r3, ra0.8d << 14, r1 << 14 @ "mul_used", 0
/* [0x000003b0] */ 0x0d9e74c0, 0x10020827, // sub r0, r2, r3
/* [0x000003b8] */ 0x8d144bf6, 0xd00279c4, // sub.setf -, r5, 4     ; mov ra4, ra5
/* [0x000003c0] */ 0xffffff38, 0xf06809e7, // brr.anyn -, r:uvloop
/* [0x000003c8] */ 0x55189db7, 0x10024161, // mov ra5, ra6          ; mul24 r1, ra6, rb9
/* [0x000003d0] */ 0x151e7d80, 0x100201a7, // mov ra6, ra7
/* [0x000003d8] */ 0x55108037, 0x100241e0, // mov ra7, r0           ; mul24 r0, ra4, rb8
/* [0x000003e0] */ 0x4d18a237, 0x10024860, // sub r1, r1, r0        ; mul24 r0, ra6, rb10
/* [0x000003e8] */ 0x4c1cb237, 0x10024860, // add r1, r1, r0        ; mul24 r0, ra7, rb11
/* [0x000003f0] */ 0x0d9e7200, 0x10020867, // sub r1, r1, r0
/* [0x000003f8] */ 0x4d512bce, 0x120269e1, // sub.setf -, r5, rb_lcount ; mul24 r1, r1, ra_k256
/* [0x00000400] */ 0x0f9ce3c0, 0xd0020867, // asr r1, r1, 14
/* [0x00000408] */ 0x405a700e, 0x120049e1, // nop                   ; mul24 r1, r1, ra_wt_mul_l0
/* [0x00000410] */ 0x119c83c0, 0xd0020867, // shl r1, r1, 8
/* [0x00000418] */ 0x0c9cc3c0, 0x10020867, // add r1, r1, rb_wt_off
/* [0x00000420] */ 0xfffffed8, 0xf06809e7, // brr.anyn -, r:uvloop
/* [0x00000428] */ 0x0f9cd3c0, 0x10c20067, // asr ra1.8as, r1, rb_wt_den_p15
/* [0x00000430] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000438] */ 0x15067d80, 0x18020c27, // mov vpm, ra1.8a
/* [0x00000440] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000448] */ 0x159dafc0, 0x10021c67, // mov vw_setup, rb_dma0
/* [0x00000450] */ 0x159ddfc0, 0x10021c67, // mov vw_setup, rb_dma1
/* [0x00000458] */ 0x159d7fc0, 0x10021ca7, // mov vw_addr, rb_dest
// ::mc_filter_uv_b0
/* [0x00000460] */ 0x9581cff6, 0x10025c42, // mov vw_setup, rb_vpm_init ; mov ra2, unif
/* [0x00000468] */ 0x14981dc0, 0xd00229e7, // and.setf -, elem_num, 1
/* [0x00000470] */ 0xec0a7d89, 0x14024821, // add r0, ra2.16b, ra2.16b ; v8subs r1, r1, r1
/* [0x00000478] */ 0x8c0821f6, 0x12225813, // add r0, r0, rb_elem_x ; mov ra_y_next, ra2.16a
/* [0x00000480] */ 0x8d8103f6, 0x10024863, // sub r1, r1, rb_pitch  ; mov r3, unif
/* [0x00000488] */ 0x935401f6, 0xd4125815, // max r0, r0, 0         ; mov ra_xshift, ra_xshift_next
/* [0x00000490] */ 0x928191f6, 0x10025801, // min r0, r0, rb_max_x  ; mov ra1, unif
/* [0x00000498] */ 0x119c31c0, 0xd0220567, // shl ra_xshift_next, r0, 3
/* [0x000004a0] */ 0x9481c1f6, 0xd0025800, // and r0, r0, -4        ; mov ra0, unif
/* [0x000004a8] */ 0x54042077, 0xd4024862, // and r1, r0, r1        ; mul24 r2, ra1.16b, 2
/* [0x000004b0] */ 0x569d404f, 0x10024821, // xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
/* [0x000004b8] */ 0x8c067076, 0x12024821, // add r0, r0, r1        ; mov r1, ra1.16a
/* [0x000004c0] */ 0x0c9e7600, 0x100206a7, // add ra_base_next, r3, r0
/* [0x000004c8] */ 0x918073f6, 0xd0025802, // shl r0, r1, 7         ; mov ra2, unif
/* [0x000004d0] */ 0x0d9d8e80, 0x10021767, // sub rb_dma1, rb_dma1_base, r2
/* [0x000004d8] */ 0x0c9c13c0, 0xd0021467, // add rb_i_tmu, r1, 3 - PREREAD
/* [0x000004e0] */ 0x0c9c33c0, 0xd00214a7, // add rb_lcount, r1, 3
/* [0x000004e8] */ 0x8c8270b6, 0x10125816, // add r0, r0, r2        ; mov ra_wt_mul_l0, unif
/* [0x000004f0] */ 0x915201bf, 0x1c12d816, // shl r0, r0, ra_k16    ; mov.ifnz ra_wt_mul_l0, unif
/* [0x000004f8] */ 0x8c81b1f6, 0x10025683, // add rb_dma0, r0, rb_dma0_base ; mov ra3, unif
/* [0x00000500] */ 0x159defc0, 0x10020267, // mov ra9, rb_max_y
/* [0x00000508] */ 0xec0e7d89, 0x14024821, // add r0, ra3.16b, ra3.16b ; v8subs r1, r1, r1
/* [0x00000510] */ 0x8c0c21f6, 0x12125813, // add r0, r0, rb_elem_x ; mov ra_y2_next, ra3.16a
/* [0x00000518] */ 0x8d8103f6, 0x10024863, // sub r1, r1, rb_pitch  ; mov r3, unif
/* [0x00000520] */ 0x935011bf, 0x18024800, // max r0, r0, ra_k0     ; mov rb_xshift2, rb_xshift2_next
/* [0x00000528] */ 0x928191f6, 0x10025801, // min r0, r0, rb_max_x  ; mov ra1, unif
/* [0x00000530] */ 0x119c31c0, 0xd0021067, // shl rb_xshift2_next, r0, 3
/* [0x00000538] */ 0x149dc1c0, 0xd0020827, // and r0, r0, -4
/* [0x00000540] */ 0x94827076, 0x10025843, // and r1, r0, r1        ; mov ra3, unif
/* [0x00000548] */ 0x569d404f, 0x10024821, // xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
/* [0x00000550] */ 0x8c0e7076, 0x18024808, // add r0, r0, r1        ; mov rb8,  ra3.8a
/* [0x00000558] */ 0x0c9e7600, 0x100214e7, // add rb_base2_next, r3, r0
/* [0x00000560] */ 0x950e0ff6, 0x1a024489, // mov ra_wt_off_mul_l1, unif        ; mov rb9,  ra3.8b
/* [0x00000568] */ 0x950e0ff6, 0x1c06448a, // mov.ifnz ra_wt_off_mul_l1, unif   ; mov rb10, ra3.8c
/* [0x00000570] */ 0x15827d80, 0x100215e7, // mov rb_dest, unif
/* [0x00000578] */ 0x950c0ff6, 0xde02494b, // mov r5quad,0          ; mov rb11, ra3.8d
/* [0x00000580] */ 0x1148ddc0, 0x14020867, // shl r1, ra_wt_off_l1, rb_wt_den_p15
/* [0x00000588] */ 0x8f8093f6, 0xd002531e, // asr rb_wt_off, r1, 9  ; mov ra_link, unif
// :uvloop_b
/* [0x00000590] */ 0xcd511bee, 0xaa0269e5, // sub.setf -, r5, rb_i_tmu ; v8adds r5rep, r5, ra_k1 ; ldtmu0
/* [0x00000598] */ 0x8e5539bf, 0x12029899, // shr r2, r4, ra_xshift ; mov.ifz ra_base2, rb_base2_next
/* [0x000005a0] */ 0x8e4c85f6, 0xd0029851, // shr r1, r2, 8         ; mov.ifz ra_y_y2, ra_y_y2_next
/* [0x000005a8] */ 0x95685ff6, 0x10029118, // mov rb4, rb5          ; mov.ifz ra_base, ra_base_next
/* [0x000005b0] */ 0x8c441fb6, 0xd4224463, // add ra_y, 1, ra_y     ; mov r3, ra_y
/* [0x000005b8] */ 0x14981f80, 0xd00229e7, // and.setf -, 1, elem_num
/* [0x000005c0] */ 0x93531789, 0xd80248e0, // max r3, r3, ra_k0     ; mov      r0, r1 << 15
/* [0x000005c8] */ 0x9227f792, 0xd00288e1, // min r3, r3, ra9       ; mov.ifz  r1, r2 << 1
/* [0x000005d0] */ 0x559d049f, 0x10044823, // mov.ifz r0, r2        ; mul24 r3, r3, rb_pitch
/* [0x000005d8] */ 0x8c616cc7, 0x10024e20, // add t0s, ra_base, r3  ; v8min r0, r0, rb_k255
/* [0x000005e0] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x000005e8] */ 0x540163f0, 0x18024863, // and r1, r1, rb_k255   ; mul24      r3, ra0.8a,       r0
/* [0x000005f0] */ 0x4003e030, 0xda0049e2, // nop                   ; mul24      r2, ra0.8b << 2,  r0 << 2  @ "mul_used", 0
/* [0x000005f8] */ 0x40038031, 0xd800c9e3, // nop                   ; mul24.ifnz r3, ra0.8a << 8,  r1 << 8  @ "mul_used", 0
/* [0x00000600] */ 0x40036031, 0xda00c9e2, // nop                   ; mul24.ifnz r2, ra0.8b << 10, r1 << 10 @ "mul_used", 0
/* [0x00000608] */ 0x4d03c4f0, 0xdc0248a3, // sub r2, r2, r3        ; mul24      r3, ra0.8c << 4,  r0 << 4  @ "mul_used", 0
/* [0x00000610] */ 0x40034031, 0xdc00c9e3, // nop                   ; mul24.ifnz r3, ra0.8c << 12, r1 << 12 @ "mul_used", 0
/* [0x00000618] */ 0x4c03a4f0, 0xde0248a3, // add r2, r2, r3        ; mul24      r3, ra0.8d << 6,  r0 << 6  @ "mul_used", 0
/* [0x00000620] */ 0x40032031, 0xde00c9e3, // nop                   ; mul24.ifnz r3, ra0.8d << 14, r1 << 14 @ "mul_used", 0
/* [0x00000628] */ 0x8d9c64ff, 0xb00240c5, // sub ra3, r2, r3       ; mov rb5, rb6          ; ldtmu1
/* [0x00000630] */ 0x8e1409f6, 0x10025884, // shr r2, r4, rb_xshift2 ; mov ra4, ra5
/* [0x00000638] */ 0x8e4485f6, 0xd2024863, // shr r1, r2, 8         ; mov r3, ra_y2
/* [0x00000640] */ 0x8c5077bf, 0x1a124446, // add ra_y2, r3, ra_k1  ; mov rb6, rb7
/* [0x00000648] */ 0x14981f80, 0xd00229e7, // and.setf -, 1, elem_num
/* [0x00000650] */ 0x93531789, 0xd80248e0, // max r3, r3, ra_k0     ; mov      r0, r1 << 15
/* [0x00000658] */ 0x9227f792, 0xd00288e1, // min r3, r3, ra9       ; mov.ifz  r1, r2 << 1
/* [0x00000660] */ 0x559d049f, 0x10044823, // mov.ifz r0, r2        ; mul24 r3, r3, rb_pitch
/* [0x00000668] */ 0x8c656cc7, 0x10024f20, // add t1s, ra_base2, r3 ; v8min r0, r0, rb_k255
/* [0x00000670] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x00000678] */ 0x540563f0, 0x18024863, // and r1, r1, rb_k255   ; mul24      r3, ra1.8a,       r0
/* [0x00000680] */ 0x4007e030, 0xda0049e2, // nop                   ; mul24      r2, ra1.8b << 2,  r0 << 2  @ "mul_used", 0
/* [0x00000688] */ 0x40078031, 0xd800c9e3, // nop                   ; mul24.ifnz r3, ra1.8a << 8,  r1 << 8  @ "mul_used", 0
/* [0x00000690] */ 0x40076031, 0xda00c9e2, // nop                   ; mul24.ifnz r2, ra1.8b << 10, r1 << 10 @ "mul_used", 0
/* [0x00000698] */ 0x4d07c4f0, 0xdc0248a3, // sub r2, r2, r3        ; mul24      r3, ra1.8c << 4,  r0 << 4  @ "mul_used", 0
/* [0x000006a0] */ 0x40074031, 0xdc00c9e3, // nop                   ; mul24.ifnz r3, ra1.8c << 12, r1 << 12 @ "mul_used", 0
/* [0x000006a8] */ 0x4c07a4f0, 0xde0248a3, // add r2, r2, r3        ; mul24      r3, ra1.8d << 6,  r0 << 6  @ "mul_used", 0
/* [0x000006b0] */ 0x40072031, 0xde00c9e3, // nop                   ; mul24.ifnz r3, ra1.8d << 14, r1 << 14 @ "mul_used", 0
/* [0x000006b8] */ 0x8d184bf6, 0xd00279c5, // sub.setf -, r5, 4     ; mov ra5, ra6
/* [0x000006c0] */ 0xfffffeb0, 0xf06809e7, // brr.anyn -, r:uvloop_b
/* [0x000006c8] */ 0x8d1e74f6, 0x10025806, // sub r0, r2, r3        ; mov ra6, ra7
/* [0x000006d0] */ 0x950e7036, 0x100241c7, // mov ra7, r0           ; mov rb7, ra3
/* [0x000006d8] */ 0x4008403e, 0x180049e0, // nop                   ; mul24 r0, rb4, ra2.8a
/* [0x000006e0] */ 0x4008503e, 0x1a0049e1, // nop                   ; mul24 r1, rb5, ra2.8b
/* [0x000006e8] */ 0x4d08623e, 0x1c024860, // sub r1, r1, r0        ; mul24 r0, rb6, ra2.8c
/* [0x000006f0] */ 0x4c08723e, 0x1e024860, // add r1, r1, r0        ; mul24 r0, rb7, ra2.8d
/* [0x000006f8] */ 0x4d108237, 0x100248a0, // sub r2, r1, r0        ; mul24 r0, ra4, rb8
/* [0x00000700] */ 0x40149037, 0x100049e1, // nop                   ; mul24 r1, ra5, rb9
/* [0x00000708] */ 0x4d18a237, 0x10024860, // sub r1, r1, r0        ; mul24 r0, ra6, rb10
/* [0x00000710] */ 0x4c1cb237, 0x10024860, // add r1, r1, r0        ; mul24 r0, ra7, rb11
/* [0x00000718] */ 0x4d527216, 0x12024862, // sub r1, r1, r0        ; mul24 r2, r2, ra_k256
/* [0x00000720] */ 0x4f50e5ce, 0xd20248a1, // asr r2, r2, 14        ; mul24 r1, r1, ra_k256
/* [0x00000728] */ 0x4f58e3d6, 0xd2024862, // asr r1, r1, 14        ; mul24 r2, r2, ra_wt_mul_l0
/* [0x00000730] */ 0x4c48c5ce, 0x120248a1, // add r2, r2, rb_wt_off ; mul24 r1, r1, ra_wt_mul_l1
/* [0x00000738] */ 0x0c9e7280, 0x10020867, // add r1, r1, r2
/* [0x00000740] */ 0x4d512bce, 0x120269e1, // sub.setf -, r5, rb_lcount ; mul24 r1, r1, ra_k256
/* [0x00000748] */ 0xfffffe28, 0xf06809e7, // brr.anyn -, r:uvloop_b
/* [0x00000750] */ 0x0f9cd3c0, 0x10c200e7, // asr ra3.8as, r1, rb_wt_den_p15
/* [0x00000758] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000760] */ 0x150e7d80, 0x18020c27, // mov vpm, ra3.8a
/* [0x00000768] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000770] */ 0x159dafc0, 0x10021c67, // mov vw_setup, rb_dma0
/* [0x00000778] */ 0x159ddfc0, 0x10021c67, // mov vw_setup, rb_dma1
/* [0x00000780] */ 0x159d7fc0, 0x10021ca7, // mov vw_addr, rb_dest
// ::mc_sync_q0
/* [0x00000788] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000790] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000798] */ 0x00000010, 0xe80009e7, // mov  dst, sacq(i)
/* [0x000007a0] */ 0x00000010, 0xe80009e7, // mov  dst, sacq(i)
/* [0x000007a8] */ 0x00000010, 0xe80009e7, // mov  dst, sacq(i)
/* [0x000007b0] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x000007b8] */ 0x0000001c, 0xe80009e7, // mov  dst, sacq(i)
/* [0x000007c0] */ 0x00000001, 0xe80009e7, // mov  dst, srel(i)
/* [0x000007c8] */ 0x0000000d, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q1
/* [0x000007d0] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x000007d8] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x000007e0] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x000007e8] */ 0x00000000, 0xe80009e7, // mov  dst, srel(i)
/* [0x000007f0] */ 0x00000011, 0xe80009e7, // mov  dst, sacq(i)
/* [0x000007f8] */ 0x00000002, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q2
/* [0x00000800] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000808] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000810] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000818] */ 0x00000000, 0xe80009e7, // mov  dst, srel(i)
/* [0x00000820] */ 0x00000012, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000828] */ 0x00000003, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q3
/* [0x00000830] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000838] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000840] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000848] */ 0x00000000, 0xe80009e7, // mov  dst, srel(i)
/* [0x00000850] */ 0x00000013, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000858] */ 0x009e7000, 0x100009e7, // nop
// ::mc_sync_q4
/* [0x00000860] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000868] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000870] */ 0x00000014, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000878] */ 0x00000014, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000880] */ 0x00000014, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000888] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000890] */ 0x0000001d, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000898] */ 0x00000005, 0xe80009e7, // mov  dst, srel(i)
/* [0x000008a0] */ 0x0000000e, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q5
/* [0x000008a8] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x000008b0] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x000008b8] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x000008c0] */ 0x00000004, 0xe80009e7, // mov  dst, srel(i)
/* [0x000008c8] */ 0x00000015, 0xe80009e7, // mov  dst, sacq(i)
/* [0x000008d0] */ 0x00000006, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q6
/* [0x000008d8] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x000008e0] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x000008e8] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x000008f0] */ 0x00000004, 0xe80009e7, // mov  dst, srel(i)
/* [0x000008f8] */ 0x00000016, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000900] */ 0x00000007, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q7
/* [0x00000908] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000910] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000918] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000920] */ 0x00000004, 0xe80009e7, // mov  dst, srel(i)
/* [0x00000928] */ 0x00000017, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000930] */ 0x009e7000, 0x100009e7, // nop
// ::mc_sync_q8
/* [0x00000938] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000940] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000948] */ 0x00000018, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000950] */ 0x00000018, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000958] */ 0x00000018, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000960] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000968] */ 0x0000001e, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000970] */ 0x00000009, 0xe80009e7, // mov  dst, srel(i)
/* [0x00000978] */ 0x0000000c, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q9
/* [0x00000980] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000988] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000990] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000998] */ 0x00000008, 0xe80009e7, // mov  dst, srel(i)
/* [0x000009a0] */ 0x00000019, 0xe80009e7, // mov  dst, sacq(i)
/* [0x000009a8] */ 0x0000000a, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q10
/* [0x000009b0] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x000009b8] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x000009c0] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x000009c8] */ 0x00000008, 0xe80009e7, // mov  dst, srel(i)
/* [0x000009d0] */ 0x0000001a, 0xe80009e7, // mov  dst, sacq(i)
/* [0x000009d8] */ 0x0000000b, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q11
/* [0x000009e0] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x000009e8] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x000009f0] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x000009f8] */ 0x00000008, 0xe80009e7, // mov  dst, srel(i)
/* [0x00000a00] */ 0x0000001b, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000a08] */ 0x009e7000, 0x100009e7, // nop
// ::mc_exit
// ::mc_exit_c
/* [0x00000a10] */ 0x009e7000, 0xa00009e7, // nop                   ; nop           ; ldtmu0
/* [0x00000a18] */ 0x009e7000, 0xb00009e7, // nop                   ; nop           ; ldtmu1
/* [0x00000a20] */ 0x009e7000, 0xa00009e7, // nop                   ; nop           ; ldtmu0
/* [0x00000a28] */ 0x159f2fc0, 0xb00009e7, // mov -, vw_wait        ; nop           ; ldtmu1
/* [0x00000a30] */ 0x009e7000, 0x300009e7, // nop                   ; nop           ; thrend
/* [0x00000a38] */ 0x009e7000, 0x100009e7, // nop
/* [0x00000a40] */ 0x009e7000, 0x100009e7, // nop
// ::mc_interrupt_exit12
// ::mc_interrupt_exit12c
/* [0x00000a48] */ 0x009e7000, 0xa00009e7, // nop                   ; nop           ; ldtmu0
/* [0x00000a50] */ 0x009e7000, 0xb00009e7, // nop                   ; nop           ; ldtmu1
/* [0x00000a58] */ 0x009e7000, 0xa00009e7, // nop                   ; nop           ; ldtmu0
/* [0x00000a60] */ 0x159f2fc0, 0xb00009e7, // mov -, vw_wait        ; nop           ; ldtmu1
/* [0x00000a68] */ 0x0000001c, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000a70] */ 0x009e7000, 0x300009e7, // nop                   ; nop           ; thrend
/* [0x00000a78] */ 0x00000001, 0xe00209a7, // mov interrupt, 1
/* [0x00000a80] */ 0x009e7000, 0x100009e7, // nop
// ::mc_setup_y_q0
/* [0x00000a88] */ 0x0000000c, 0xe80009e7, // mov  dst, srel(i)
// ::mc_setup_y_qn
/* [0x00000a90] */ 0x95801ff6, 0xd0025900, // mov tmurs, 1          ; mov ra0, unif
/* [0x00000a98] */ 0x15827d80, 0x10020267, // mov ra9, unif
/* [0x00000aa0] */ 0x15827d80, 0x10020067, // mov ra1, unif
/* [0x00000aa8] */ 0x15827d80, 0x100202e7, // mov ra11, unif
/* [0x00000ab0] */ 0xff100100, 0xe0020527, // mov ra_kff100100, 0xff100100
/* [0x00000ab8] */ 0x000000ff, 0xe00215a7, // mov rb_k255, 255
/* [0x00000ac0] */ 0x15827d80, 0x100200e7, // mov ra3, unif
/* [0x00000ac8] */ 0x15827d80, 0x10021527, // mov rb_xpitch, unif
/* [0x00000ad0] */ 0x0d0c1dc0, 0xd4021667, // sub rb_max_x, ra3.16b, 1
/* [0x00000ad8] */ 0x0d0c1dc0, 0xd20217a7, // sub rb_max_y, ra3.16a, 1
/* [0x00000ae0] */ 0x15827d80, 0x10021427, // mov rb_pitch, unif
/* [0x00000ae8] */ 0xc0000000, 0xe0020867, // mov r1, vdw_setup_1(0)
/* [0x00000af0] */ 0x159d03c0, 0x10021627, // or  rb_dma1_base, r1, rb_pitch
/* [0x00000af8] */ 0x159a7d80, 0x100208e7, // mov r3, elem_num
/* [0x00000b00] */ 0x0c027cc0, 0x14020827, // add r0, ra0.16b, r3
/* [0x00000b08] */ 0x139c01c0, 0xd0020827, // max r0, r0, 0
/* [0x00000b10] */ 0x129d91c0, 0x10020827, // min r0, r0, rb_max_x
/* [0x00000b18] */ 0x119c31c0, 0xd0220567, // shl ra_xshift_next, r0, 3
/* [0x00000b20] */ 0xf49dc1d2, 0xd0024822, // and r0, r0, -4        ; v8subs r2, r2, r2
/* [0x00000b28] */ 0x0d9d05c0, 0x100208a7, // sub r2, r2, rb_pitch
/* [0x00000b30] */ 0x149e7080, 0x10020867, // and r1, r0, r2
/* [0x00000b38] */ 0x569d404f, 0x10024821, // xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
/* [0x00000b40] */ 0x0c9e7040, 0x10020827, // add r0, r0, r1
/* [0x00000b48] */ 0x0c267c00, 0x10020627, // add ra_base, ra9, r0
/* [0x00000b50] */ 0x0c067cc0, 0x14020827, // add r0, ra1.16b, r3
/* [0x00000b58] */ 0x139c01c0, 0xd0020827, // max r0, r0, 0
/* [0x00000b60] */ 0x129d91c0, 0x10020827, // min r0, r0, rb_max_x
/* [0x00000b68] */ 0x119c31c0, 0xd0021067, // shl rb_xshift2_next, r0, 3
/* [0x00000b70] */ 0x149dc1c0, 0xd0020827, // and r0, r0, -4
/* [0x00000b78] */ 0x149e7080, 0x10020867, // and r1, r0, r2
/* [0x00000b80] */ 0x569d404f, 0x10024821, // xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
/* [0x00000b88] */ 0x0c9e7040, 0x10020827, // add r0, r0, r1
/* [0x00000b90] */ 0x0c2e7c00, 0x10020667, // add ra_base2, ra11, r0
/* [0x00000b98] */ 0x15027d80, 0x12020827, // mov r0, ra0.16a
/* [0x00000ba0] */ 0x15067d80, 0x120208a7, // mov r2, ra1.16a
/* [0x00000ba8] */ 0x00000002, 0xe00208e7, // mov r3, PREREAD
// :y_preload
/* [0x00000bb0] */ 0x0d9c17c0, 0xd00228e7, // sub.setf r3, r3, 1
/* [0x00000bb8] */ 0x139c01c0, 0xd0020867, // max r1, r0, 0
/* [0x00000bc0] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_max_y
/* [0x00000bc8] */ 0x4c51018f, 0x1a024821, // add r0, r0, ra_k1     ; mul24 r1, r1, rb_pitch
/* [0x00000bd0] */ 0x0c627c40, 0x10020e27, // add t0s, ra_base, r1
/* [0x00000bd8] */ 0x139c05c0, 0xd0020867, // max r1, r2, 0
/* [0x00000be0] */ 0xffffffb0, 0xf03809e7, // brr.anynz -, r:y_preload
/* [0x00000be8] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_max_y
/* [0x00000bf0] */ 0x4c51058f, 0x1a0248a1, // add r2, r2, ra_k1     ; mul24 r1, r1, rb_pitch
/* [0x00000bf8] */ 0x0c667c40, 0x10020f27, // add t1s, ra_base2, r1
/* [0x00000c00] */ 0x159e7000, 0x10220467, // mov ra_y, r0
/* [0x00000c08] */ 0x159e7480, 0x10120467, // mov ra_y2, r2
/* [0x00000c10] */ 0x0c809dc0, 0xd0021367, // add rb_wt_den_p15, unif, 9
/* [0x00000c18] */ 0x159e6fc0, 0x100208a7, // mov r2, qpu_num
/* [0x00000c20] */ 0x0f9c25c0, 0xd0020867, // asr r1, r2, 2
/* [0x00000c28] */ 0x119c63c0, 0xd0020867, // shl r1, r1, 6
/* [0x00000c30] */ 0x149c35c0, 0xd0020827, // and r0, r2, 3
/* [0x00000c38] */ 0x159e7040, 0x10020827, // or  r0, r0, r1
/* [0x00000c40] */ 0x00004800, 0xe0020867, // mov r1, vpm_setup(0, 4, h8p(0, 0))
/* [0x00000c48] */ 0x0c9e7040, 0x10021727, // add r_vpm, r0, r1
/* [0x00000c50] */ 0x80004004, 0xe0020867, // mov r1, vdw_setup_0(0, 0, dma_h8p(0,0,0))
/* [0x00000c58] */ 0x119c51c0, 0xd0020827, // shl r0, r0, 5
/* [0x00000c60] */ 0x0c9e7040, 0x100216e7, // add r_dma, r0, r1
/* [0x00000c68] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000c70] */ 0x00000000, 0xe0024208, // mov ra8,  0           ; mov rb8,  0
/* [0x00000c78] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000c80] */ 0x00000000, 0xe0024249, // mov ra9,  0           ; mov rb9,  0
/* [0x00000c88] */ 0x00000000, 0xe002428a, // mov ra10, 0           ; mov rb10, 0
/* [0x00000c90] */ 0x00000000, 0xe00242cb, // mov ra11, 0           ; mov rb11, 0
// :per_block_setup
/* [0x00000c98] */ 0x935401f6, 0xd4125815, // max r0, r0, 0         ; mov ra_xshift, ra_xshift_next
/* [0x00000ca0] */ 0x129d91c0, 0x10020827, // min r0, r0, rb_max_x
/* [0x00000ca8] */ 0x119c31c0, 0xd0220567, // shl ra_xshift_next, r0, 3
/* [0x00000cb0] */ 0xf49dc1d2, 0xd0024822, // and r0, r0, -4        ; v8subs r2, r2, r2
/* [0x00000cb8] */ 0x8d8105f6, 0x1002589a, // sub r2, r2, rb_pitch  ; mov ra_base_next, unif
/* [0x00000cc0] */ 0x940270b6, 0x12225853, // and r1, r0, r2        ; mov ra_y_next, ra0.16a
/* [0x00000cc8] */ 0x569d404f, 0x10024821, // xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
/* [0x00000cd0] */ 0x8c827076, 0x10025801, // add r0, r0, r1        ; mov ra1, unif
/* [0x00000cd8] */ 0x0c6a7c00, 0x100206a7, // add ra_base_next, ra_base_next, r0
/* [0x00000ce0] */ 0x0c067cc0, 0x14020827, // add r0, ra1.16b, r3
/* [0x00000ce8] */ 0x930401f6, 0xd2125813, // max r0, r0, 0         ; mov ra_y2_next, ra1.16a
/* [0x00000cf0] */ 0x928191f6, 0x10024813, // min r0, r0, rb_max_x  ; mov rb_base2_next, unif
/* [0x00000cf8] */ 0x119c31c0, 0xd0021067, // shl rb_xshift2_next, r0, 3
/* [0x00000d00] */ 0x9481c1f6, 0xd0025810, // and r0, r0, -4        ; mov ra_width_height, unif
/* [0x00000d08] */ 0x149e7080, 0x10020867, // and r1, r0, r2
/* [0x00000d10] */ 0x569d404f, 0x10024821, // xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
/* [0x00000d18] */ 0x8c9dc07f, 0x10024831, // add r0, r0, r1        ; mov vw_setup, rb_vpm_init
/* [0x00000d20] */ 0x0c9d3e00, 0x100214e7, // add rb_base2_next, rb_base2_next, r0
/* [0x00000d28] */ 0x0d418f80, 0x14021767, // sub rb_dma1, rb_dma1_base, ra_width
/* [0x00000d30] */ 0x8c405df6, 0xd2025460, // add rb_i_tmu, ra_height, 7 - PREREAD ; mov r0, ra_height
/* [0x00000d38] */ 0x12527180, 0x1c020827, // min r0, r0, ra_k16
/* [0x00000d40] */ 0x0c9c71c0, 0xd00214a7, // add rb_lcount, r0, 7
/* [0x00000d48] */ 0x119c71c0, 0xd0020827, // shl r0,   r0, 7
/* [0x00000d50] */ 0x0c427180, 0x14020827, // add r0,   r0, ra_width
/* [0x00000d58] */ 0x119d01c0, 0xd0020827, // shl r0,   r0, i_shift16
/* [0x00000d60] */ 0x8c81b1f6, 0x100256a0, // add rb_dma0, r0, rb_dma0_base ; mov r0, unif
/* [0x00000d68] */ 0x918101f6, 0xd0045816, // shl.ifz r0, r0, i_shift16 ; mov ra_wt_off_mul_l0, unif
/* [0x00000d70] */ 0x119c31c0, 0xd0020227, // shl ra8, r0, 3
/* [0x00000d78] */ 0x00010100, 0xe0020867, // mov r1,0x00010100
/* [0x00000d80] */ 0x10227380, 0x1e4200a7, // ror ra2.8a, r1, ra8.8d
/* [0x00000d88] */ 0x10227380, 0x1c420027, // ror ra0.8a, r1, ra8.8c
/* [0x00000d90] */ 0x01040400, 0xe0020867, // mov r1, 0x01040400
/* [0x00000d98] */ 0x10227380, 0x1e5200a7, // ror ra2.8b, r1, ra8.8d
/* [0x00000da0] */ 0x10227380, 0x1c520027, // ror ra0.8b, r1, ra8.8c
/* [0x00000da8] */ 0x050b0a00, 0xe0020867, // mov r1,0x050b0a00
/* [0x00000db0] */ 0x10227380, 0x1e6200a7, // ror ra2.8c, r1, ra8.8d
/* [0x00000db8] */ 0x10227380, 0x1c620027, // ror ra0.8c, r1, ra8.8c
/* [0x00000dc0] */ 0x11283a40, 0xe0020867, // mov r1,0x11283a40
/* [0x00000dc8] */ 0x10227380, 0x1e7200a7, // ror ra2.8d, r1, ra8.8d
/* [0x00000dd0] */ 0x10227380, 0x1c720027, // ror ra0.8d, r1, ra8.8c
/* [0x00000dd8] */ 0x3a281100, 0xe0020867, // mov r1,0x3a281100
/* [0x00000de0] */ 0x902203bf, 0x1e025812, // ror r0, r1, ra8.8d    ; mov ra_wt_off_mul_l1, unif
/* [0x00000de8] */ 0x90216387, 0x1c424044, // ror ra1.8a, r1, ra8.8c ; v8min rb4, r0, rb_k255
/* [0x00000df0] */ 0x0a0b0500, 0xe0020867, // mov r1,0x0a0b0500
/* [0x00000df8] */ 0x10227380, 0x1e020827, // ror r0, r1, ra8.8d
/* [0x00000e00] */ 0x90216387, 0x1c524045, // ror ra1.8b, r1, ra8.8c ; v8min rb5, r0, rb_k255
/* [0x00000e08] */ 0x04040100, 0xe0020867, // mov r1,0x04040100
/* [0x00000e10] */ 0x10227380, 0x1e020827, // ror r0, r1, ra8.8d
/* [0x00000e18] */ 0x90216387, 0x1c624046, // ror ra1.8c, r1, ra8.8c ; v8min rb6, r0, rb_k255
/* [0x00000e20] */ 0x954a0dbf, 0x10064597, // mov.ifnz ra_wt_off_mul_l0, ra_wt_off_mul_l1 ; mov rb_dest, unif
/* [0x00000e28] */ 0x01010000, 0xe0020867, // mov r1,0x01010000
/* [0x00000e30] */ 0x10227380, 0x1e020827, // ror r0, r1, ra8.8d
/* [0x00000e38] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000e40] */ 0x90216387, 0x1c724047, // ror ra1.8d, r1, ra8.8c ; v8min rb7, r0, rb_k255
/* [0x00000e48] */ 0xf158dddb, 0x14024825, // shl r0, ra_wt_off_l0, rb_wt_den_p15 ; v8subs r5rep, r3, r3
/* [0x00000e50] */ 0x8f8091f6, 0xd002531e, // asr rb_wt_off, r0, 9  ; mov ra_link, unif
// ::mc_filter
/* [0x00000e58] */ 0xfffffe20, 0xf0f807a7, // brr ra_link, r:per_block_setup
/* [0x00000e60] */ 0x959a0ff6, 0x10024023, // mov ra0, unif         ; mov r3, elem_num
/* [0x00000e68] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x00000e70] */ 0x8c001cff, 0x14024800, // add r0, ra0.16b, r3   ; mov rb_xshift2, rb_xshift2_next
/* [0x00000e78] */ 0x11581dc0, 0xd21205a7, // shl ra_wt_mul_l0, ra_wt_mul_l0, 1
// :yloop
/* [0x00000e80] */ 0xcd511bee, 0xba0269e5, // sub.setf -, r5, rb_i_tmu      ; v8adds r5rep, r5, ra_k1            ; ldtmu1
/* [0x00000e88] */ 0x8e4c09f6, 0xa0029851, // shr r1, r4, rb_xshift2        ; mov.ifz ra_y_y2, ra_y_y2_next      ; ldtmu0
/* [0x00000e90] */ 0x8e5509bf, 0x12024823, // shr r0, r4, ra_xshift         ; mov r3, rb_pitch
/* [0x00000e98] */ 0x13440dc0, 0xd40208a7, // max r2, ra_y, 0
/* [0x00000ea0] */ 0x9269e5f6, 0x10029898, // min r2, r2, rb_max_y          ; mov.ifz ra_base, ra_base_next
/* [0x00000ea8] */ 0x4c441dd3, 0xd4224462, // add ra_y, ra_y, 1             ; mul24 r2, r2, r3
/* [0x00000eb0] */ 0x8c613cbf, 0x10029e19, // add t0s, ra_base, r2          ; mov.ifz ra_base2, rb_base2_next
/* [0x00000eb8] */ 0x13440dc0, 0xd20208a7, // max r2, ra_y2, 0
/* [0x00000ec0] */ 0x129de5c0, 0x100208a7, // min r2, r2, rb_max_y
/* [0x00000ec8] */ 0x4c441dd3, 0xd2124462, // add ra_y2, ra_y2, 1           ; mul24 r2, r2, r3
/* [0x00000ed0] */ 0x8c656c87, 0x10024f20, // add t1s, ra_base2, r2         ; v8min r0, r0, rb_k255
/* [0x00000ed8] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x00000ee0] */ 0x540163f0, 0x18024863, // and r1, r1, rb_k255   ; mul24      r3, ra0.8a,      r0
/* [0x00000ee8] */ 0x4003f030, 0xda0049e2, // nop                   ; mul24      r2, ra0.8b << 1, r0 << 1    @ "mul_used", 0
/* [0x00000ef0] */ 0x40038031, 0xd800c9e3, // nop                   ; mul24.ifnz r3, ra0.8a << 8, r1 << 8    @ "mul_used", 0
/* [0x00000ef8] */ 0x40037031, 0xda00c9e2, // nop                   ; mul24.ifnz r2, ra0.8b << 9, r1 << 9    @ "mul_used", 0
/* [0x00000f00] */ 0x4d03e4f0, 0xdc0248a3, // sub r2, r2, r3        ; mul24      r3, ra0.8c << 2, r0 << 2    @ "mul_used", 0
/* [0x00000f08] */ 0x40036031, 0xdc00c9e3, // nop                   ; mul24.ifnz r3, ra0.8c << 10, r1 << 10  @ "mul_used", 0
/* [0x00000f10] */ 0x4d03d4f0, 0xde0248a3, // sub r2, r2, r3        ; mul24      r3, ra0.8d << 3, r0 << 3    @ "mul_used", 0
/* [0x00000f18] */ 0x40035031, 0xde00c9e3, // nop                   ; mul24.ifnz r3, ra0.8d << 11, r1 << 11  @ "mul_used", 0
/* [0x00000f20] */ 0x4c07c4f0, 0xd80248a3, // add r2, r2, r3        ; mul24      r3, ra1.8a << 4, r0 << 4    @ "mul_used", 0
/* [0x00000f28] */ 0x40074031, 0xd800c9e3, // nop                   ; mul24.ifnz r3, ra1.8a << 12, r1 << 12  @ "mul_used", 0
/* [0x00000f30] */ 0x4c07b4f0, 0xda0248a3, // add r2, r2, r3        ; mul24      r3, ra1.8b << 5, r0 << 5    @ "mul_used", 0
/* [0x00000f38] */ 0x40073031, 0xda00c9e3, // nop                   ; mul24.ifnz r3, ra1.8b << 13, r1 << 13  @ "mul_used", 0
/* [0x00000f40] */ 0x4d07a4f0, 0xdc0248a3, // sub r2, r2, r3        ; mul24      r3, ra1.8c << 6, r0 << 6    @ "mul_used", 0
/* [0x00000f48] */ 0x40072031, 0xdc00c9e3, // nop                   ; mul24.ifnz r3, ra1.8c << 14, r1 << 14  @ "mul_used", 0
/* [0x00000f50] */ 0x4c0794f0, 0xde0248a3, // add r2, r2, r3        ; mul24      r3, ra1.8d << 7, r0 << 7    @ "mul_used", 0
/* [0x00000f58] */ 0x40071031, 0xde00c9e3, // nop                   ; mul24.ifnz r3, ra1.8d << 15, r1 << 15  @ "mul_used", 0
/* [0x00000f60] */ 0x0d9e74c0, 0x10020827, // sub r0, r2, r3
/* [0x00000f68] */ 0x8d208bf6, 0xd00269e1, // sub.setf -, r5, 8     ; mov r1,   ra8
/* [0x00000f70] */ 0x95249dbf, 0x10024208, // mov ra8,  ra9         ; mov rb8,  rb9
/* [0x00000f78] */ 0xfffffee8, 0xf06809e7, // brr.anyn -, r:yloop
/* [0x00000f80] */ 0x9528adbf, 0x10024249, // mov ra9,  ra10        ; mov rb9,  rb10
/* [0x00000f88] */ 0x952cbdbf, 0x1002428a, // mov ra10, ra11        ; mov rb10, rb11
/* [0x00000f90] */ 0x959e7009, 0x100242cb, // mov ra11, r0          ; mov rb11, r1
/* [0x00000f98] */ 0x4008803e, 0x180049e0, // nop                   ; mul24 r0, rb8,  ra2.8a
/* [0x00000fa0] */ 0x4008903e, 0x1a0049e1, // nop                   ; mul24 r1, rb9,  ra2.8b
/* [0x00000fa8] */ 0x4d08a23e, 0x1c024860, // sub r1, r1, r0        ; mul24 r0, rb10, ra2.8c
/* [0x00000fb0] */ 0x4d08b23e, 0x1e024860, // sub r1, r1, r0        ; mul24 r0, rb11, ra2.8d
/* [0x00000fb8] */ 0x4c204237, 0x10024860, // add r1, r1, r0        ; mul24 r0, ra8,  rb4
/* [0x00000fc0] */ 0x4c245237, 0x10024860, // add r1, r1, r0        ; mul24 r0, ra9,  rb5
/* [0x00000fc8] */ 0x4d286237, 0x10024860, // sub r1, r1, r0        ; mul24 r0, ra10, rb6
/* [0x00000fd0] */ 0x4c2c7237, 0x10024860, // add r1, r1, r0        ; mul24 r0, ra11, rb7
/* [0x00000fd8] */ 0x8d9f223f, 0x10020867, // sub r1, r1, r0        ; mov -, vw_wait
/* [0x00000fe0] */ 0x4d512bce, 0x120269e1, // sub.setf -, r5, rb_lcount ; mul24 r1, r1, ra_k256
/* [0x00000fe8] */ 0x0f9ce3c0, 0xd0020867, // asr r1, r1, 14
/* [0x00000ff0] */ 0x405a700e, 0x120049e1, // nop                   ; mul24 r1, r1, ra_wt_mul_l0
/* [0x00000ff8] */ 0x0c9cc3c0, 0x10020867, // add r1, r1, rb_wt_off
/* [0x00001000] */ 0x914083f6, 0xd2024860, // shl r1, r1, 8         ; mov r0, ra_height
/* [0x00001008] */ 0xfffffe58, 0xf06809e7, // brr.anyn -, r:yloop
/* [0x00001010] */ 0x0f9cd3c0, 0x10c200e7, // asr ra3.8as, r1, rb_wt_den_p15
/* [0x00001018] */ 0x00000010, 0xe0020867, // mov r1, 16
/* [0x00001020] */ 0x8d0e7076, 0x18024830, // sub r0, r0, r1        ; mov vpm, ra3.8a
/* [0x00001028] */ 0x939c01c0, 0xd01279d0, // max.setf -, r0, 0     ; mov ra_height, r0
/* [0x00001030] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x00001038] */ 0x929da07f, 0x10024831, // min r0, r0, r1        ; mov vw_setup, rb_dma0
/* [0x00001040] */ 0x8d9dd07f, 0x100248b1, // sub r2, r0, r1        ; mov vw_setup, rb_dma1
/* [0x00001048] */ 0x809d703f, 0x100049f2, // nop                   ; mov vw_addr, rb_dest
/* [0x00001050] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x00001058] */ 0x119d75c0, 0xd0020827, // shl r0, r2, i_shift23
/* [0x00001060] */ 0x0c9dae00, 0x100216a7, // add rb_dma0, rb_dma0, r0
/* [0x00001068] */ 0xfffffdf8, 0xf0f809e7, // brr -, r:yloop
/* [0x00001070] */ 0x409d000f, 0x100049e0, // nop                   ; mul24 r0, r1, rb_pitch
/* [0x00001078] */ 0x0c9d7e00, 0x100215e7, // add rb_dest, rb_dest, r0
/* [0x00001080] */ 0x159dcfc0, 0x10021c67, // mov vw_setup, rb_vpm_init
// ::mc_filter_b
/* [0x00001088] */ 0xfffffbf0, 0xf0f807a7, // brr ra_link, r:per_block_setup
/* [0x00001090] */ 0x959a0ff6, 0x10024023, // mov ra0, unif         ; mov r3, elem_num
/* [0x00001098] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x000010a0] */ 0x8c001cff, 0x14024800, // add r0, ra0.16b, r3   ; mov rb_xshift2, rb_xshift2_next
// :yloopb
/* [0x000010a8] */ 0xcd511bee, 0xba0269e5, // sub.setf -, r5, rb_i_tmu      ; v8adds r5rep, r5, ra_k1             ; ldtmu1
/* [0x000010b0] */ 0x8e4c09f6, 0xa0029851, // shr r1, r4, rb_xshift2        ; mov.ifz ra_y_y2, ra_y_y2_next      ; ldtmu0
/* [0x000010b8] */ 0x8e5509bf, 0x12024823, // shr r0, r4, ra_xshift         ; mov r3, rb_pitch
/* [0x000010c0] */ 0x13440dc0, 0xd40208a7, // max r2, ra_y, 0
/* [0x000010c8] */ 0x9269e5f6, 0x10029898, // min r2, r2, rb_max_y          ; mov.ifz ra_base, ra_base_next
/* [0x000010d0] */ 0x4c441dd3, 0xd4224462, // add ra_y, ra_y, 1             ; mul24 r2, r2, r3
/* [0x000010d8] */ 0x8c613cbf, 0x10029e19, // add t0s, ra_base, r2          ; mov.ifz ra_base2, rb_base2_next
/* [0x000010e0] */ 0x13440dc0, 0xd20208a7, // max r2, ra_y2, 0
/* [0x000010e8] */ 0x129de5c0, 0x100208a7, // min r2, r2, rb_max_y
/* [0x000010f0] */ 0x4c441dd3, 0xd2124462, // add ra_y2, ra_y2, 1           ; mul24 r2, r2, r3
/* [0x000010f8] */ 0x8c656c87, 0x10024f20, // add t1s, ra_base2, r2         ; v8min r0, r0, rb_k255
/* [0x00001100] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x00001108] */ 0x540163f0, 0x18024863, // and r1, r1, rb_k255   ; mul24      r3, ra0.8a,      r0
/* [0x00001110] */ 0x4003f030, 0xda0049e2, // nop                   ; mul24      r2, ra0.8b << 1, r0 << 1    @ "mul_used", 0
/* [0x00001118] */ 0x40038031, 0xd800c9e3, // nop                   ; mul24.ifnz r3, ra0.8a << 8, r1 << 8    @ "mul_used", 0
/* [0x00001120] */ 0x40037031, 0xda00c9e2, // nop                   ; mul24.ifnz r2, ra0.8b << 9, r1 << 9    @ "mul_used", 0
/* [0x00001128] */ 0x4d03e4f0, 0xdc0248a3, // sub r2, r2, r3        ; mul24      r3, ra0.8c << 2, r0 << 2    @ "mul_used", 0
/* [0x00001130] */ 0x40036031, 0xdc00c9e3, // nop                   ; mul24.ifnz r3, ra0.8c << 10, r1 << 10  @ "mul_used", 0
/* [0x00001138] */ 0x4d03d4f0, 0xde0248a3, // sub r2, r2, r3        ; mul24      r3, ra0.8d << 3, r0 << 3    @ "mul_used", 0
/* [0x00001140] */ 0x40035031, 0xde00c9e3, // nop                   ; mul24.ifnz r3, ra0.8d << 11, r1 << 11  @ "mul_used", 0
/* [0x00001148] */ 0x4c07c4f0, 0xd80248a3, // add r2, r2, r3        ; mul24      r3, ra1.8a << 4, r0 << 4    @ "mul_used", 0
/* [0x00001150] */ 0x40074031, 0xd800c9e3, // nop                   ; mul24.ifnz r3, ra1.8a << 12, r1 << 12  @ "mul_used", 0
/* [0x00001158] */ 0x4c07b4f0, 0xda0248a3, // add r2, r2, r3        ; mul24      r3, ra1.8b << 5, r0 << 5    @ "mul_used", 0
/* [0x00001160] */ 0x40073031, 0xda00c9e3, // nop                   ; mul24.ifnz r3, ra1.8b << 13, r1 << 13  @ "mul_used", 0
/* [0x00001168] */ 0x4d07a4f0, 0xdc0248a3, // sub r2, r2, r3        ; mul24      r3, ra1.8c << 6, r0 << 6    @ "mul_used", 0
/* [0x00001170] */ 0x40072031, 0xdc00c9e3, // nop                   ; mul24.ifnz r3, ra1.8c << 14, r1 << 14  @ "mul_used", 0
/* [0x00001178] */ 0x4c0794f0, 0xde0248a3, // add r2, r2, r3        ; mul24      r3, ra1.8d << 7, r0 << 7    @ "mul_used", 0
/* [0x00001180] */ 0x40071031, 0xde00c9e3, // nop                   ; mul24.ifnz r3, ra1.8d << 15, r1 << 15  @ "mul_used", 0
/* [0x00001188] */ 0x0d9e74c0, 0x10020827, // sub r0, r2, r3
/* [0x00001190] */ 0x8d208bf6, 0xd00269e1, // sub.setf -, r5, 8     ; mov r1,   ra8
/* [0x00001198] */ 0x95249dbf, 0x10024208, // mov ra8,  ra9         ; mov rb8,  rb9
/* [0x000011a0] */ 0xfffffee8, 0xf06809e7, // brr.anyn -, r:yloopb
/* [0x000011a8] */ 0x9528adbf, 0x10024249, // mov ra9,  ra10        ; mov rb9,  rb10
/* [0x000011b0] */ 0x952cbdbf, 0x1002428a, // mov ra10, ra11        ; mov rb10, rb11
/* [0x000011b8] */ 0x959e7009, 0x100242cb, // mov ra11, r0          ; mov rb11, r1
/* [0x000011c0] */ 0x4008803e, 0x180049e0, // nop                   ; mul24 r0, rb8,  ra2.8a
/* [0x000011c8] */ 0x4008903e, 0x1a0049e1, // nop                   ; mul24 r1, rb9,  ra2.8b
/* [0x000011d0] */ 0x4d08a23e, 0x1c024860, // sub r1, r1, r0        ; mul24 r0, rb10, ra2.8c
/* [0x000011d8] */ 0x4d08b23e, 0x1e024860, // sub r1, r1, r0        ; mul24 r0, rb11, ra2.8d
/* [0x000011e0] */ 0x4c204237, 0x10024860, // add r1, r1, r0        ; mul24 r0, ra8,  rb4
/* [0x000011e8] */ 0x4c245237, 0x10024860, // add r1, r1, r0        ; mul24 r0, ra9,  rb5
/* [0x000011f0] */ 0x4d286237, 0x10024860, // sub r1, r1, r0        ; mul24 r0, ra10, rb6
/* [0x000011f8] */ 0x4c2c7237, 0x10024860, // add r1, r1, r0        ; mul24 r0, ra11, rb7
/* [0x00001200] */ 0x8d9cc23f, 0x10024862, // sub r1, r1, r0        ; mov r2, rb_wt_off
/* [0x00001208] */ 0x4d512bce, 0x120269e1, // sub.setf -, r5, rb_lcount ; mul24 r1, r1, ra_k256
/* [0x00001210] */ 0x0f9ce3c0, 0xd0020867, // asr r1, r1, 14
/* [0x00001218] */ 0x405a700e, 0x120049e0, // nop                   ; mul24 r0, r1, ra_wt_mul_l0
/* [0x00001220] */ 0x4c4b808e, 0xd2024821, // add r0, r0, r2        ; mul24 r1, r1 << 8, ra_wt_mul_l1 << 8    @ "mul_used", 0
/* [0x00001228] */ 0x8c9f223f, 0x10020867, // add r1, r1, r0        ; mov -, vw_wait
/* [0x00001230] */ 0x914083f6, 0xd2024860, // shl r1, r1, 8         ; mov r0, ra_height
/* [0x00001238] */ 0xfffffe50, 0xf06809e7, // brr.anyn -, r:yloopb
/* [0x00001240] */ 0x0f9cd3c0, 0x10c200e7, // asr ra3.8as, r1, rb_wt_den_p15
/* [0x00001248] */ 0x00000010, 0xe0020867, // mov r1, 16
/* [0x00001250] */ 0x8d0e7076, 0x18024830, // sub r0, r0, r1        ; mov vpm, ra3.8a
/* [0x00001258] */ 0x939c01c0, 0xd01279d0, // max.setf -, r0, 0     ; mov ra_height, r0
/* [0x00001260] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x00001268] */ 0x929da07f, 0x10024831, // min r0, r0, r1        ; mov vw_setup, rb_dma0
/* [0x00001270] */ 0x8d9dd07f, 0x100248b1, // sub r2, r0, r1        ; mov vw_setup, rb_dma1
/* [0x00001278] */ 0x809d703f, 0x100049f2, // nop                   ; mov vw_addr, rb_dest
/* [0x00001280] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x00001288] */ 0x119d75c0, 0xd0020827, // shl r0, r2, i_shift23
/* [0x00001290] */ 0x0c9dae00, 0x100216a7, // add rb_dma0, rb_dma0, r0
/* [0x00001298] */ 0xfffffdf0, 0xf0f809e7, // brr -, r:yloopb
/* [0x000012a0] */ 0x409d000f, 0x100049e0, // nop                   ; mul24 r0, r1, rb_pitch
/* [0x000012a8] */ 0x0c9d7e00, 0x100215e7, // add rb_dest, rb_dest, r0
/* [0x000012b0] */ 0x159dcfc0, 0x10021c67, // mov vw_setup, rb_vpm_init
// ::mc_filter_y_p00
/* [0x000012b8] */ 0x959a0ff6, 0x10024023, // mov ra0, unif         ; mov r3, elem_num
/* [0x000012c0] */ 0x15567d80, 0x14120567, // mov ra_xshift, ra_xshift_next
/* [0x000012c8] */ 0x0c027cc0, 0x14020827, // add r0, ra0.16b, r3
/* [0x000012d0] */ 0x139c01c0, 0xd0020827, // max r0, r0, 0
/* [0x000012d8] */ 0x129d91c0, 0x10020827, // min r0, r0, rb_max_x
/* [0x000012e0] */ 0x119c31c0, 0xd0220567, // shl ra_xshift_next, r0, 3
/* [0x000012e8] */ 0xf49dc1d2, 0xd0024822, // and r0, r0, -4        ; v8subs r2, r2, r2
/* [0x000012f0] */ 0x8d8105f6, 0x1002589a, // sub r2, r2, rb_pitch  ; mov ra_base_next, unif
/* [0x000012f8] */ 0x940270b6, 0x12225853, // and r1, r0, r2        ; mov ra_y_next, ra0.16a
/* [0x00001300] */ 0x569d404f, 0x10024821, // xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
/* [0x00001308] */ 0x8c827076, 0x10025810, // add r0, r0, r1        ; mov ra_width_height, unif
/* [0x00001310] */ 0x8c69cc3f, 0x100246b1, // add ra_base_next, ra_base_next, r0 ; mov vw_setup, rb_vpm_init
/* [0x00001318] */ 0x0d418f80, 0x14021767, // sub rb_dma1, rb_dma1_base, ra_width
/* [0x00001320] */ 0x8d402df6, 0xd2025460, // sub rb_i_tmu, ra_height, PREREAD ; mov r0, ra_height
/* [0x00001328] */ 0x12527180, 0x1c020827, // min r0, r0, ra_k16
/* [0x00001330] */ 0x8c8001f6, 0xd0025496, // add rb_lcount, r0, 0  ; mov ra_wt_off_mul_l0, unif
/* [0x00001338] */ 0x918071f6, 0xd0024817, // shl r0,   r0, 7       ; mov rb_dest, unif
/* [0x00001340] */ 0x0c427180, 0x14020827, // add r0,   r0, ra_width
/* [0x00001348] */ 0x119d01c0, 0xd0020827, // shl r0,   r0, i_shift16
/* [0x00001350] */ 0x0c9db1c0, 0x100216a7, // add rb_dma0, r0, rb_dma0_base
/* [0x00001358] */ 0xf158dddb, 0x14024825, // shl r0, ra_wt_off_l0, rb_wt_den_p15 ; v8subs r5rep, r3, r3
/* [0x00001360] */ 0x8f8011f6, 0xd002531e, // asr rb_wt_off, r0, 1  ; mov ra_link, unif
// :yloop_p00
/* [0x00001368] */ 0xcd511bee, 0x1a0269e5, // sub.setf -, r5, rb_i_tmu  ; v8adds r5rep, r5, ra_k1
/* [0x00001370] */ 0x804e7036, 0xa42099d1, // nop                   ; mov.ifz ra_y, ra_y_next      ; ldtmu0
/* [0x00001378] */ 0x8e5509bf, 0x12024823, // shr r0, r4, ra_xshift ; mov r3, rb_pitch
/* [0x00001380] */ 0x13440dc0, 0xd40208a7, // max r2, ra_y, 0
/* [0x00001388] */ 0x9269e5f6, 0x10029898, // min r2, r2, rb_max_y  ; mov.ifz ra_base, ra_base_next
/* [0x00001390] */ 0x4c441dd3, 0xd4224462, // add ra_y, ra_y, 1     ; mul24 r2, r2, r3
/* [0x00001398] */ 0x8c616c87, 0x10024e20, // add t0s, ra_base, r2  ; v8min r0, r0, rb_k255
/* [0x000013a0] */ 0x4d592bc6, 0x120269e1, // sub.setf -, r5, rb_lcount ; mul24 r1, r0, ra_wt_mul_l0
/* [0x000013a8] */ 0x9140f3f6, 0xd2024860, // shl r1, r1, 15        ; mov r0, ra_height
/* [0x000013b0] */ 0x0c9cc3c0, 0x10020867, // add r1, r1, rb_wt_off
/* [0x000013b8] */ 0xffffff90, 0xf06809e7, // brr.anyn -, r:yloop_p00
/* [0x000013c0] */ 0x0f9cd3c0, 0x10c200e7, // asr ra3.8as, r1, rb_wt_den_p15
/* [0x000013c8] */ 0x95532dbf, 0x1c020867, // mov r1, ra_k16        ; mov -, vw_wait
/* [0x000013d0] */ 0x8d0e7076, 0x18024830, // sub r0, r0, r1        ; mov vpm, ra3.8a
/* [0x000013d8] */ 0x939c01c0, 0xd01279d0, // max.setf -, r0, 0     ; mov ra_height, r0
/* [0x000013e0] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x000013e8] */ 0x929da07f, 0x10024831, // min r0, r0, r1        ; mov vw_setup, rb_dma0
/* [0x000013f0] */ 0x8d9dd07f, 0x100248b1, // sub r2, r0, r1        ; mov vw_setup, rb_dma1
/* [0x000013f8] */ 0x809d703f, 0x100049f2, // nop                   ; mov vw_addr, rb_dest
/* [0x00001400] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x00001408] */ 0x119d75c0, 0xd0020827, // shl r0, r2, i_shift23
/* [0x00001410] */ 0x0c9dae00, 0x100216a7, // add rb_dma0, rb_dma0, r0
/* [0x00001418] */ 0xffffff30, 0xf0f809e7, // brr -, r:yloop_p00
/* [0x00001420] */ 0x409d000f, 0x100049e0, // nop                   ; mul24 r0, r1, rb_pitch
/* [0x00001428] */ 0x0c9d7e00, 0x100215e7, // add rb_dest, rb_dest, r0
/* [0x00001430] */ 0x159dcfc0, 0x10021c67, // mov vw_setup, rb_vpm_init
// ::mc_filter_y_b00
/* [0x00001438] */ 0xfffff840, 0xf0f807a7, // brr ra_link, r:per_block_setup
/* [0x00001440] */ 0x959a0ff6, 0x10024023, // mov ra0, unif         ; mov r3, elem_num
/* [0x00001448] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x00001450] */ 0x8c001cff, 0x14024800, // add r0, ra0.16b, r3   ; mov rb_xshift2, rb_xshift2_next
/* [0x00001458] */ 0x00000007, 0xe0020827, // mov r0, 7
/* [0x00001460] */ 0x0d9d1e00, 0x10021467, // sub rb_i_tmu, rb_i_tmu, r0
/* [0x00001468] */ 0x0d9d2e00, 0x100214a7, // sub rb_lcount, rb_lcount, r0
/* [0x00001470] */ 0x95588ff6, 0xd0024821, // mov r0, 8             ; mov r1, ra_wt_off_mul_l0
/* [0x00001478] */ 0x119cce00, 0x10021327, // shl rb_wt_off, rb_wt_off, r0
/* [0x00001480] */ 0x809f8009, 0xd000d9d6, // nop                   ; mov.ifnz ra_wt_off_mul_l0, r1 << 8
// :yloop_b00
/* [0x00001488] */ 0xcd511bee, 0xba0269e5, // sub.setf -, r5, rb_i_tmu      ; v8adds r5rep, r5, ra_k1             ; ldtmu1
/* [0x00001490] */ 0x8e4c09f6, 0xa0029851, // shr r1, r4, rb_xshift2 ; mov.ifz ra_y_y2, ra_y_y2_next      ; ldtmu0
/* [0x00001498] */ 0x8e5509bf, 0x12024823, // shr r0, r4, ra_xshift ; mov r3, rb_pitch
/* [0x000014a0] */ 0x13440dc0, 0xd40208a7, // max r2, ra_y, 0
/* [0x000014a8] */ 0x9269e5f6, 0x10029898, // min r2, r2, rb_max_y  ; mov.ifz ra_base, ra_base_next
/* [0x000014b0] */ 0x4c441dd3, 0xd4224462, // add ra_y, ra_y, 1     ; mul24 r2, r2, r3
/* [0x000014b8] */ 0x8c613cbf, 0x10029e19, // add t0s, ra_base, r2  ; mov.ifz ra_base2, rb_base2_next
/* [0x000014c0] */ 0x13440dc0, 0xd20208a7, // max r2, ra_y2, 0
/* [0x000014c8] */ 0x129de5c0, 0x100208a7, // min r2, r2, rb_max_y
/* [0x000014d0] */ 0x4c441dd3, 0xd2124462, // add ra_y2, ra_y2, 1   ; mul24 r2, r2, r3
/* [0x000014d8] */ 0x8c656c87, 0x10024f20, // add t1s, ra_base2, r2 ; v8min r0, r0, rb_k255
/* [0x000014e0] */ 0x545963c6, 0x12024860, // and r1, r1, rb_k255   ; mul24 r0, r0, ra_wt_mul_l0
/* [0x000014e8] */ 0x4d492bce, 0x120269e1, // sub.setf -, r5, rb_lcount ; mul24 r1, r1, ra_wt_mul_l1
/* [0x000014f0] */ 0x0c9e7040, 0x10020867, // add r1, r0, r1
/* [0x000014f8] */ 0x119ce3c0, 0xd0020867, // shl r1, r1, 14
/* [0x00001500] */ 0x8c40c3f6, 0x12024860, // add r1, r1, rb_wt_off ; mov r0, ra_height
/* [0x00001508] */ 0xffffff60, 0xf06809e7, // brr.anyn -, r:yloop_b00
/* [0x00001510] */ 0x0f9cd3c0, 0x10c200e7, // asr ra3.8as, r1, rb_wt_den_p15
/* [0x00001518] */ 0x95532dbf, 0x1c020867, // mov r1, ra_k16        ; mov -, vw_wait
/* [0x00001520] */ 0x8d0e7076, 0x18024830, // sub r0, r0, r1        ; mov vpm, ra3.8a
/* [0x00001528] */ 0x939c01c0, 0xd01279d0, // max.setf -, r0, 0     ; mov ra_height, r0
/* [0x00001530] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x00001538] */ 0x929da07f, 0x10024831, // min r0, r0, r1        ; mov vw_setup, rb_dma0
/* [0x00001540] */ 0x8d9dd07f, 0x100248b1, // sub r2, r0, r1        ; mov vw_setup, rb_dma1
/* [0x00001548] */ 0x809d703f, 0x100049f2, // nop                   ; mov vw_addr, rb_dest
/* [0x00001550] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x00001558] */ 0x119d75c0, 0xd0020827, // shl r0, r2, i_shift23
/* [0x00001560] */ 0x0c9dae00, 0x100216a7, // add rb_dma0, rb_dma0, r0
/* [0x00001568] */ 0xffffff00, 0xf0f809e7, // brr -, r:yloop_b00
/* [0x00001570] */ 0x409d000f, 0x100049e0, // nop                   ; mul24 r0, r1, rb_pitch
/* [0x00001578] */ 0x0c9d7e00, 0x100215e7, // add rb_dest, rb_dest, r0
/* [0x00001580] */ 0x159dcfc0, 0x10021c67, // mov vw_setup, rb_vpm_init
// ::mc_end
};
#ifdef __HIGHC__
#pragma Align_to(8, rpi_shader)
#endif
