#include "rpi_hevc_shader.h"

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
unsigned int ff_hevc_rpi_shader[] = {
// ::mc_setup_c_q0
// ::mc_start
/* [0x00000000] */ 0x0000000c, 0xe80009e7, // mov  dst, srel(i)
// ::mc_setup_c_qn
/* [0x00000008] */ 0x95801ff6, 0xd0025900, // mov tmurs, 1                  ; mov ra0, unif
/* [0x00000010] */ 0xaaaaff00, 0xe6020827, // mov r0, [0,2,0,2,0,2,0,2,1,3,1,3,1,3,1,3]
/* [0x00000018] */ 0x9181e1f6, 0xd00250d8, // shl rb_ef, r0, i_shift30      ; mov ra_base, unif
/* [0x00000020] */ 0x0d801dc0, 0xd0020827, // sub r0, unif, 1
/* [0x00000028] */ 0x119c11c0, 0xd00216a7, // shl rb_max_x, r0, v_x_shift
/* [0x00000030] */ 0x0d801dc0, 0xd00217a7, // sub rb_max_y, unif, 1
/* [0x00000038] */ 0xff800100, 0xe0020527, // mov ra_kff800100, 0xff800100
/* [0x00000040] */ 0x000000ff, 0xe0021627, // mov rb_pmask, v_pmask
/* [0x00000048] */ 0x001000ff, 0xe00205e7, // mov ra_blk_height_pmax, ((1 << v_bit_depth) - 1) | (v_blk_height << 16)
/* [0x00000050] */ 0x00004000, 0xe00217e7, // mov rb_fir_off_h, (FIR_OFFSET << (v_bit_depth - 8))
/* [0x00000058] */ 0x4000000e, 0xe0020667, // mov ra_fir_off_val_wt_den_p7, (FIR_OFFSET << 16) | (DENOM + 15 - v_bit_depth)
/* [0x00000060] */ 0x95803ff6, 0x10024754, // mov ra_ef, rb_ef              ; mov rb_xpitch, unif
/* [0x00000068] */ 0x15827d80, 0x10021427, // mov rb_pitch, unif
/* [0x00000070] */ 0xc0000000, 0xe0020867, // mov r1, vdw_setup_1(0)
/* [0x00000078] */ 0x0c9d03c0, 0x10021667, // add rb_dma1_base, r1, rb_pitch
/* [0x00000080] */ 0x14981f80, 0xd0020827, // and r0, 1, elem_num
/* [0x00000088] */ 0x409c5007, 0xd00049e0, // nop                           ; mul24 r0, r0, 5
/* [0x00000090] */ 0x0c9a7180, 0x100210a7, // add rb_elem_x, r0, elem_num
/* [0x00000098] */ 0x11001dc0, 0xd4020827, // shl r0, ra0.16b, v_x_shift
/* [0x000000a0] */ 0x0c9c21c0, 0x10020827, // add r0, r0, rb_elem_x
/* [0x000000a8] */ 0x930001f6, 0xd2225811, // max r0, r0, 0                 ; mov ra_y, ra0.16a
/* [0x000000b0] */ 0x129da1c0, 0x10020827, // min r0, r0, rb_max_x
/* [0x000000b8] */ 0x119c31c0, 0xd0220567, // shl ra_xshift_next, r0, 3
/* [0x000000c0] */ 0x149dc1c0, 0xd0020827, // and r0, r0, -4
/* [0x000000c8] */ 0x0d510dc0, 0x18020867, // sub r1, ra_k0, rb_pitch
/* [0x000000d0] */ 0x149e7040, 0x10020867, // and r1, r0, r1
/* [0x000000d8] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x000000e0] */ 0x8c827076, 0x10025800, // add r0, r0, r1                ; mov ra0, unif
/* [0x000000e8] */ 0x0c627c00, 0x10020627, // add ra_base, ra_base, r0
/* [0x000000f0] */ 0x159e6fc0, 0x100208a7, // mov r2, qpu_num
/* [0x000000f8] */ 0x0f9c25c0, 0xd0020867, // asr r1, r2, 2
/* [0x00000100] */ 0x119c63c0, 0xd0020867, // shl r1, r1, 6
/* [0x00000108] */ 0x149c35c0, 0xd0020827, // and r0, r2, 3
/* [0x00000110] */ 0x159e7040, 0x10020827, // or  r0, r0, r1
/* [0x00000118] */ 0x00004800, 0xe0020867, // mov r1, vpm_setup(0, 4, h8p(0, 0))
/* [0x00000120] */ 0x0c9e7040, 0x10021727, // add r_vpm, r0, r1
/* [0x00000128] */ 0x80004004, 0xe0020867, // mov r1, vdw_setup_0(0, 0, dma_h8p(0,0,0))
/* [0x00000130] */ 0x119c51c0, 0xd0020827, // shl r0, r0, 5
/* [0x00000138] */ 0x0c9e7040, 0x100216e7, // add r_dma, r0, r1
/* [0x00000140] */ 0x11001dc0, 0xd4020827, // shl r0, ra0.16b, v_x_shift
/* [0x00000148] */ 0x8c0021f6, 0x12125811, // add r0, r0, rb_elem_x         ; mov ra_y2, ra0.16a
/* [0x00000150] */ 0x938001f6, 0xd002480f, // max r0, r0, 0                 ; mov rb_base2, unif
/* [0x00000158] */ 0x129da1c0, 0x10020827, // min r0, r0, rb_max_x
/* [0x00000160] */ 0x119c31c0, 0xd0021067, // shl rb_xshift2_next, r0, 3
/* [0x00000168] */ 0x149dc1c0, 0xd0020827, // and r0, r0, -4
/* [0x00000170] */ 0x0d510dc0, 0x18020867, // sub r1, ra_k0, rb_pitch
/* [0x00000178] */ 0x949c307f, 0xd0024863, // and r1, r0, r1                ; mov r3, PREREAD
/* [0x00000180] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00000188] */ 0x8c467076, 0x12024822, // add r0, r0, r1                ; mov r2, ra_y2
/* [0x00000190] */ 0x8c44fe36, 0x140253e0, // add rb_base2, rb_base2, r0    ; mov r0, ra_y
// :1
/* [0x00000198] */ 0x0d9c17c0, 0xd00228e7, // sub.setf r3, r3, 1
/* [0x000001a0] */ 0x139c01c0, 0xd0020867, // max r1, r0, 0
/* [0x000001a8] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_max_y
/* [0x000001b0] */ 0x4c51018f, 0x1a024821, // add r0, r0, ra_k1             ; mul24 r1, r1, rb_pitch
/* [0x000001b8] */ 0x8c627c40, 0x10225e11, // add t0s, ra_base, r1          ; mov ra_y, r0
/* [0x000001c0] */ 0x139c05c0, 0xd0020867, // max r1, r2, 0
/* [0x000001c8] */ 0xffffffb0, 0xf03809e7, // brr.anynz -, r:1b
/* [0x000001d0] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_max_y
/* [0x000001d8] */ 0x4c51058f, 0x1a0248a1, // add r2, r2, ra_k1             ; mul24 r1, r1, rb_pitch
/* [0x000001e0] */ 0x8c9cfe52, 0x10125f11, // add t1s, rb_base2, r1         ; mov ra_y2, r2
/* [0x000001e8] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x000001f0] */ 0x00000000, 0xe0024104, // mov ra4, 0                    ; mov rb4, 0
/* [0x000001f8] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000200] */ 0x00000000, 0xe0024145, // mov ra5, 0                    ; mov rb5, 0
/* [0x00000208] */ 0x00000000, 0xe0024186, // mov ra6, 0                    ; mov rb6, 0
/* [0x00000210] */ 0x00000000, 0xe00241c7, // mov ra7, 0                    ; mov rb7, 0
// ::mc_filter_c_p
/* [0x00000218] */ 0x9581cff6, 0x10025c42, // mov vw_setup, rb_vpm_init     ; mov ra2, unif
/* [0x00000220] */ 0x8c803ff6, 0x100269e3, // add.setf -, rb_ef, rb_ef      ; mov r3, unif
/* [0x00000228] */ 0xf1081dc0, 0xd4024825, // shl r0, ra2.16b, v_x_shift    ; v8subs r5rep, r0, r0
/* [0x00000230] */ 0x8c8021f6, 0x10025810, // add r0, r0, rb_elem_x         ; mov ra_width_height, unif
/* [0x00000238] */ 0x8d810bf6, 0x10025840, // sub r1, r5, rb_pitch          ; mov ra0, unif
/* [0x00000240] */ 0x93567176, 0x14024800, // max r0, r0, r5                ; mov vrx_xshift, vrx_xshift_next
/* [0x00000248] */ 0x9209a1f6, 0x12225813, // min r0, r0, rb_max_x          ; mov vra_y_next, ra2.16a
/* [0x00000250] */ 0x119c31c0, 0xd0220567, // shl vrx_xshift_next, r0, 3
/* [0x00000258] */ 0x149dc1c0, 0xd0020827, // and r0, r0, -4
/* [0x00000260] */ 0x54402077, 0xd4024862, // and r1, r0, r1                ; mul24 r2, ra_width, v_x_mul
/* [0x00000268] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00000270] */ 0x8c827076, 0x10025803, // add r0, r0, r1                ; mov ra3, unif
/* [0x00000278] */ 0x8c427636, 0x120246a1, // add vrx_base_next, r3, r0     ; mov r1, ra_height
/* [0x00000280] */ 0x8d819eb6, 0x10025756, // sub rb_dma1, rb_dma1_base, r2 ; mov ra_wt_off_mul_l0, unif
/* [0x00000288] */ 0x8c5dc3ce, 0xdc025461, // add rb_i_tmu, r1, (3-4) - PREREAD ; v8min r1, r1, ra_blk_height
/* [0x00000290] */ 0x8c81f3f6, 0xd0039496, // add rb_lcount, r1, (3-4)      ; mov.ifc ra_wt_off_mul_l0, unif
/* [0x00000298] */ 0x918073f6, 0xd002581c, // shl r0, r1, v_dma_h_shift     ; mov ra_dest, unif
/* [0x000002a0] */ 0x8c6670b6, 0x14024822, // add r0, r0, r2                ; mov r2, ra_fir_off_val
/* [0x000002a8] */ 0x910d01f6, 0xdc02480a, // shl r0, r0, v_dma_wh_shift    ; mov rb10, ra3.8c
/* [0x000002b0] */ 0x8c59b1f6, 0x140246e1, // add ra_dma0, r0, rb_dma0_base ; mov r1, ra_wt_off_l0
/* [0x000002b8] */ 0x5158c3d6, 0xd2024860, // shl r1, r1, i_wt_den_p5       ; mul24 r0, r2, ra_wt_mul_l0
/* [0x000002c0] */ 0x8d667236, 0x14025320, // sub rb_wt_off, r1, r0         ; mov r0, ra_kmul_add
/* [0x000002c8] */ 0x8c59cc3f, 0xd21245a5, // add ra_wt_mul_l0, ra_wt_mul_l0, r0 ; mov r5rep, -4
/* [0x000002d0] */ 0x950e0dbf, 0x1e0252de, // mov rb11, ra3.8d              ; mov ra_link, unif
// :1
/* [0x000002d8] */ 0x8d151bf6, 0xa00269c4, // sub.setf -, r5, rb_i_tmu      ; mov rb4, ra5                  ; ldtmu0
/* [0x000002e0] */ 0x8e4c09f6, 0x140288a3, // shr r2, r4, vrx_xshift        ; mov.ifz  r3, vra_y_next
/* [0x000002e8] */ 0x8e4485f6, 0xd402c863, // shr r1, r2, v_v_shift         ; mov.ifnz r3, vra_y
/* [0x000002f0] */ 0x8c683ff6, 0x1002b9d8, // add.setf -, rb_ef, rb_ef      ; mov.ifz  vra_base, vrx_base_next
/* [0x000002f8] */ 0x8c531789, 0xda224460, // add vra_y, r3, ra_k1          ; mov      r0, r1 << 15
/* [0x00000300] */ 0x9353f792, 0xd803c8e1, // max r3, r3, ra_k0             ; mov.ifnc r1, r2 << 1
/* [0x00000308] */ 0x929de7d2, 0x1003c8e0, // min r3, r3, rb_max_y          ; mov.ifnc r0, r2
/* [0x00000310] */ 0x545d039f, 0x12024863, // and r1, r1, ra_pmax           ; mul24 r3, r3, rb_pitch
/* [0x00000318] */ 0x8c618cc7, 0x10024e20, // add vr_txs, vra_base, r3      ; v8min r0, r0, rb_pmask
/* [0x00000320] */ 0x4c001bf0, 0xd8025963, // add r5rep, r5, 1              ; mul24      r3, ra0.8a,       r0
/* [0x00000328] */ 0x4d01fef1, 0x1e0248a3, // sub r2, rb_fir_off_h, r3      ; mul24      r3, ra0.8d,       r1
/* [0x00000330] */ 0x4d03e4f0, 0xda0248a3, // sub r2, r2, r3                ; mul24      r3, ra0.8b << 2,  r0 << 2  @ "mul_used", 0
/* [0x00000338] */ 0x40034031, 0xda0109e3, // nop                           ; mul24.ifn  r3, ra0.8b << 12, r1 << 12 @ "mul_used", 0
/* [0x00000340] */ 0x4c03c4f0, 0xdc0248a3, // add r2, r2, r3                ; mul24      r3, ra0.8c << 4,  r0 << 4  @ "mul_used", 0
/* [0x00000348] */ 0x4c032b71, 0xdc0329e3, // add.setf -, r5, r5            ; mul24.ifn  r3, ra0.8c << 14, r1 << 14 @ "mul_used", 0
/* [0x00000350] */ 0xffffff68, 0xf06809e7, // brr.anyn -, r:1b
/* [0x00000358] */ 0x4c1ca4f7, 0x100248a0, // add r2, r2, r3                ; mul24 r0, ra7, rb10
/* [0x00000360] */ 0x550c6ffe, 0x1a024161, // mov ra5, rb6                  ; mul24 r1, rb6, ra3.8b
/* [0x00000368] */ 0x8f1c05f6, 0xd00241c6, // asr ra7, r2, v_bit_depth - 8  ; mov rb6, ra7
/* [0x00000370] */ 0x4c0c423e, 0x18024860, // add r1, r1, r0                ; mul24 r0, rb4, ra3.8a
/* [0x00000378] */ 0x4d1cb237, 0x10024860, // sub r1, r1, r0                ; mul24 r0, ra7, rb11
/* [0x00000380] */ 0x0d9e7200, 0x10020867, // sub r1, r1, r0
/* [0x00000388] */ 0x8f5c63f6, 0xdc024863, // asr r1, r1, 6                 ; mov r3, ra_blk_height
/* [0x00000390] */ 0x4d592bce, 0x120269e0, // sub.setf -, r5, rb_lcount     ; mul24 r0, r1, ra_wt_mul_l0
/* [0x00000398] */ 0x4c64c1ce, 0x14024821, // add r0, r0, rb_wt_off         ; mul24 r1, r1, ra_kmul_add
/* [0x000003a0] */ 0xed427073, 0x12024860, // sub r1, r0, r1                ; v8subs r0, ra_height, r3
/* [0x000003a8] */ 0xffffff10, 0xf06809e7, // brr.anyn -, r:1b
/* [0x000003b0] */ 0x0f9cd3c0, 0xd0020867, // asr r1, r1, i_wt_den_p6
/* [0x000003b8] */ 0x925f23bf, 0x12020867, // min r1, r1, ra_pmax           ; mov -, vw_wait
/* [0x000003c0] */ 0x5351039f, 0x18024c22, // max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch
/* [0x000003c8] */ 0x956e7036, 0x10126431, // mov.setf ra_height, r0        ; mov vw_setup, ra_dma0
/* [0x000003d0] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x000003d8] */ 0x929dd0ff, 0x10024831, // min r0, r0, r3                ; mov vw_setup, rb_dma1
/* [0x000003e0] */ 0x8d7270f6, 0x10024872, // sub r1, r0, r3                ; mov vw_addr, ra_dest
/* [0x000003e8] */ 0x119d73c0, 0xd0020867, // shl r1, r1, i_shift23
/* [0x000003f0] */ 0xfffffec8, 0xf0f809e7, // brr -, r:1b
/* [0x000003f8] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x00000400] */ 0x0c6e7c40, 0x100206e7, // add ra_dma0, ra_dma0, r1
/* [0x00000408] */ 0x8c71ccbf, 0x10024731, // add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init
// ::mc_filter_c_p_l1
/* [0x00000410] */ 0x9581cff6, 0x10025c42, // mov vw_setup, rb_vpm_init     ; mov ra2, unif
/* [0x00000418] */ 0x8c803ff6, 0x100269e3, // add.setf -, rb_ef, rb_ef      ; mov r3, unif
/* [0x00000420] */ 0xf1081dc0, 0xd4024825, // shl r0, ra2.16b, v_x_shift    ; v8subs r5rep, r0, r0
/* [0x00000428] */ 0x8c8021f6, 0x10025810, // add r0, r0, rb_elem_x         ; mov ra_width_height, unif
/* [0x00000430] */ 0x8d810bf6, 0x10025840, // sub r1, r5, rb_pitch          ; mov ra0, unif
/* [0x00000438] */ 0x939c117f, 0x10125815, // max r0, r0, r5                ; mov vrx_xshift, vrx_xshift_next
/* [0x00000440] */ 0x9209a1f6, 0x12125813, // min r0, r0, rb_max_x          ; mov vra_y_next, ra2.16a
/* [0x00000448] */ 0x119c31c0, 0xd0021067, // shl vrx_xshift_next, r0, 3
/* [0x00000450] */ 0x149dc1c0, 0xd0020827, // and r0, r0, -4
/* [0x00000458] */ 0x54402077, 0xd4024862, // and r1, r0, r1                ; mul24 r2, ra_width, v_x_mul
/* [0x00000460] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00000468] */ 0x8c827076, 0x10025803, // add r0, r0, r1                ; mov ra3, unif
/* [0x00000470] */ 0x8c427636, 0x120254e1, // add vrx_base_next, r3, r0     ; mov r1, ra_height
/* [0x00000478] */ 0x8d819eb6, 0x10025756, // sub rb_dma1, rb_dma1_base, r2 ; mov ra_wt_off_mul_l0, unif
/* [0x00000480] */ 0x8c5dc3ce, 0xdc025461, // add rb_i_tmu, r1, (3-4) - PREREAD ; v8min r1, r1, ra_blk_height
/* [0x00000488] */ 0x8c81f3f6, 0xd0039496, // add rb_lcount, r1, (3-4)      ; mov.ifc ra_wt_off_mul_l0, unif
/* [0x00000490] */ 0x918073f6, 0xd002581c, // shl r0, r1, v_dma_h_shift     ; mov ra_dest, unif
/* [0x00000498] */ 0x8c6670b6, 0x14024822, // add r0, r0, r2                ; mov r2, ra_fir_off_val
/* [0x000004a0] */ 0x910d01f6, 0xdc02480a, // shl r0, r0, v_dma_wh_shift    ; mov rb10, ra3.8c
/* [0x000004a8] */ 0x8c59b1f6, 0x140246e1, // add ra_dma0, r0, rb_dma0_base ; mov r1, ra_wt_off_l0
/* [0x000004b0] */ 0x5158c3d6, 0xd2024860, // shl r1, r1, i_wt_den_p5       ; mul24 r0, r2, ra_wt_mul_l0
/* [0x000004b8] */ 0x8d667236, 0x14025320, // sub rb_wt_off, r1, r0         ; mov r0, ra_kmul_add
/* [0x000004c0] */ 0x8c59cc3f, 0xd21245a5, // add ra_wt_mul_l0, ra_wt_mul_l0, r0 ; mov r5rep, -4
/* [0x000004c8] */ 0x950e0dbf, 0x1e0252de, // mov rb11, ra3.8d              ; mov ra_link, unif
// :1
/* [0x000004d0] */ 0x8d151bf6, 0xb00269c4, // sub.setf -, r5, rb_i_tmu      ; mov rb4, ra5                  ; ldtmu1
/* [0x000004d8] */ 0x8e5539bf, 0x1202888f, // shr r2, r4, vrx_xshift        ; mov.ifz  vra_base, vrx_base_next
/* [0x000004e0] */ 0x8e4485f6, 0xd202c863, // shr r1, r2, v_v_shift         ; mov.ifnz r3, vra_y
/* [0x000004e8] */ 0x8c4c3ff6, 0x1202a9e3, // add.setf -, rb_ef, rb_ef      ; mov.ifz  r3, vra_y_next
/* [0x000004f0] */ 0x8c531789, 0xda124460, // add vra_y, r3, ra_k1          ; mov      r0, r1 << 15
/* [0x000004f8] */ 0x9353f792, 0xd803c8e1, // max r3, r3, ra_k0             ; mov.ifnc r1, r2 << 1
/* [0x00000500] */ 0x929de7d2, 0x1003c8e0, // min r3, r3, rb_max_y          ; mov.ifnc r0, r2
/* [0x00000508] */ 0x545d039f, 0x12024863, // and r1, r1, ra_pmax           ; mul24 r3, r3, rb_pitch
/* [0x00000510] */ 0x8c5cfec6, 0x12024f20, // add vr_txs, vra_base, r3      ; v8min r0, r0, ra_pmax
/* [0x00000518] */ 0x4c001bf0, 0xd8025963, // add r5rep, r5, 1              ; mul24      r3, ra0.8a,       r0
/* [0x00000520] */ 0x4d01fef1, 0x1e0248a3, // sub r2, rb_fir_off_h, r3      ; mul24      r3, ra0.8d,       r1
/* [0x00000528] */ 0x4d03e4f0, 0xda0248a3, // sub r2, r2, r3                ; mul24      r3, ra0.8b << 2,  r0 << 2  @ "mul_used", 0
/* [0x00000530] */ 0x40034031, 0xda0109e3, // nop                           ; mul24.ifn  r3, ra0.8b << 12, r1 << 12 @ "mul_used", 0
/* [0x00000538] */ 0x4c03c4f0, 0xdc0248a3, // add r2, r2, r3                ; mul24      r3, ra0.8c << 4,  r0 << 4  @ "mul_used", 0
/* [0x00000540] */ 0x4c032b71, 0xdc0329e3, // add.setf -, r5, r5            ; mul24.ifn  r3, ra0.8c << 14, r1 << 14 @ "mul_used", 0
/* [0x00000548] */ 0xffffff68, 0xf06809e7, // brr.anyn -, r:1b
/* [0x00000550] */ 0x4c1ca4f7, 0x100248a0, // add r2, r2, r3                ; mul24 r0, ra7, rb10
/* [0x00000558] */ 0x550c6ffe, 0x1a024161, // mov ra5, rb6                  ; mul24 r1, rb6, ra3.8b
/* [0x00000560] */ 0x8f1c05f6, 0xd00241c6, // asr ra7, r2, v_bit_depth - 8  ; mov rb6, ra7
/* [0x00000568] */ 0x4c0c423e, 0x18024860, // add r1, r1, r0                ; mul24 r0, rb4, ra3.8a
/* [0x00000570] */ 0x4d1cb237, 0x10024860, // sub r1, r1, r0                ; mul24 r0, ra7, rb11
/* [0x00000578] */ 0x0d9e7200, 0x10020867, // sub r1, r1, r0
/* [0x00000580] */ 0x8f5c63f6, 0xdc024863, // asr r1, r1, 6                 ; mov r3, ra_blk_height
/* [0x00000588] */ 0x4d592bce, 0x120269e0, // sub.setf -, r5, rb_lcount     ; mul24 r0, r1, ra_wt_mul_l0
/* [0x00000590] */ 0x4c64c1ce, 0x14024821, // add r0, r0, rb_wt_off         ; mul24 r1, r1, ra_kmul_add
/* [0x00000598] */ 0xed427073, 0x12024860, // sub r1, r0, r1                ; v8subs r0, ra_height, r3
/* [0x000005a0] */ 0xffffff10, 0xf06809e7, // brr.anyn -, r:1b
/* [0x000005a8] */ 0x0f9cd3c0, 0xd0020867, // asr r1, r1, i_wt_den_p6
/* [0x000005b0] */ 0x925f23bf, 0x12020867, // min r1, r1, ra_pmax           ; mov -, vw_wait
/* [0x000005b8] */ 0x5351039f, 0x18024c22, // max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch
/* [0x000005c0] */ 0x956e7036, 0x10126431, // mov.setf ra_height, r0        ; mov vw_setup, ra_dma0
/* [0x000005c8] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x000005d0] */ 0x929dd0ff, 0x10024831, // min r0, r0, r3                ; mov vw_setup, rb_dma1
/* [0x000005d8] */ 0x8d7270f6, 0x10024872, // sub r1, r0, r3                ; mov vw_addr, ra_dest
/* [0x000005e0] */ 0x119d73c0, 0xd0020867, // shl r1, r1, i_shift23
/* [0x000005e8] */ 0xfffffec8, 0xf0f809e7, // brr -, r:1b
/* [0x000005f0] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x000005f8] */ 0x0c6e7c40, 0x100206e7, // add ra_dma0, ra_dma0, r1
/* [0x00000600] */ 0x8c71ccbf, 0x10024731, // add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init
// ::mc_filter_c_b
/* [0x00000608] */ 0x9581cff6, 0x10025c42, // mov vw_setup, rb_vpm_init     ; mov ra2, unif
/* [0x00000610] */ 0x8c803ff6, 0x100269e3, // add.setf -, rb_ef, rb_ef      ; mov r3, unif
/* [0x00000618] */ 0xf1081dc9, 0xd4024825, // shl r0, ra2.16b, v_x_shift    ; v8subs r5rep, r1, r1
/* [0x00000620] */ 0x8c0821f6, 0x12225813, // add r0, r0, rb_elem_x         ; mov ra_y_next, ra2.16a
/* [0x00000628] */ 0x8d810bf6, 0x10025850, // sub r1, r5, rb_pitch          ; mov ra_width_height, unif
/* [0x00000630] */ 0x93567176, 0x14125815, // max r0, r0, r5                ; mov ra_xshift, ra_xshift_next
/* [0x00000638] */ 0x9281a1f6, 0x10025800, // min r0, r0, rb_max_x          ; mov ra0, unif
/* [0x00000640] */ 0x119c31c0, 0xd0220567, // shl ra_xshift_next, r0, 3
/* [0x00000648] */ 0x9481c1f6, 0xd0025802, // and r0, r0, -4                ; mov ra2, unif
/* [0x00000650] */ 0x54402077, 0xd4024862, // and r1, r0, r1                ; mul24 r2, ra_width, v_x_mul
/* [0x00000658] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00000660] */ 0x8c427076, 0x12024821, // add r0, r0, r1                ; mov r1, ra_height
/* [0x00000668] */ 0x8c9c163f, 0x10024680, // add ra_base_next, r3, r0      ; mov rb_xshift2, rb_xshift2_next
/* [0x00000670] */ 0x8d819eb6, 0x10025756, // sub rb_dma1, rb_dma1_base, r2 ; mov ra_wt_off_mul_l0, unif
/* [0x00000678] */ 0x8c5dc3ce, 0xdc025461, // add rb_i_tmu, r1, (3-4) - PREREAD ; v8min r1, r1, ra_blk_height
/* [0x00000680] */ 0x8c59f3f6, 0xd4139496, // add rb_lcount, r1, (3-4)      ; mov.ifc ra_wt_mul_l0, ra_wt_off_l0
/* [0x00000688] */ 0x918073f6, 0xd0025803, // shl r0, r1, v_dma_h_shift     ; mov ra3, unif
/* [0x00000690] */ 0x8c8270b6, 0x10024823, // add r0, r0, r2                ; mov r3, unif
/* [0x00000698] */ 0x910d01f6, 0xd2125813, // shl r0, r0, v_dma_wh_shift    ; mov ra_y2_next, ra3.16a
/* [0x000006a0] */ 0x8c0db1f6, 0x140246e0, // add ra_dma0, r0, rb_dma0_base ; mov r0, ra3.16b
/* [0x000006a8] */ 0x918011f6, 0xd0025801, // shl r0, r0, v_x_shift         ; mov ra1, unif
/* [0x000006b0] */ 0x8c8021f6, 0x10025803, // add r0, r0, rb_elem_x         ; mov ra3, unif
/* [0x000006b8] */ 0x8d810bf6, 0x10025852, // sub r1, r5, rb_pitch          ; mov ra_wt_off_mul_l1, unif
/* [0x000006c0] */ 0x939de17f, 0x10025809, // max r0, r0, r5                ; mov ra9, rb_max_y
/* [0x000006c8] */ 0x9265a1f6, 0x14024822, // min r0, r0, rb_max_x          ; mov r2, ra_kmul_add
/* [0x000006d0] */ 0x119c31c0, 0xd0021067, // shl rb_xshift2_next, r0, 3
/* [0x000006d8] */ 0x9481c1f6, 0xd0039812, // and r0, r0, -4                ; mov.ifc ra_wt_off_mul_l1, unif
/* [0x000006e0] */ 0x949dc07f, 0xd0024865, // and r1, r0, r1                ; mov r5rep, -4
/* [0x000006e8] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x000006f0] */ 0x8c827076, 0x1002581c, // add r0, r0, r1                ; mov ra_dest, unif
/* [0x000006f8] */ 0x8c667636, 0x140254e0, // add rb_base2_next, r3, r0     ; mov r0, ra_fir_off_val
/* [0x00000700] */ 0x4c5a7c86, 0x121245a1, // add ra_wt_mul_l0, ra_wt_mul_l0, r2 ; mul24 r1, r0, ra_wt_mul_l0
/* [0x00000708] */ 0x4c4a7c86, 0x121244a0, // add ra_wt_mul_l1, ra_wt_mul_l1, r2 ; mul24 r0, r0, ra_wt_mul_l1
/* [0x00000710] */ 0x8c4a7076, 0x14024821, // add r0, r0, r1                ; mov r1, ra_wt_off_l1
/* [0x00000718] */ 0x910cd3f6, 0xde02484b, // shl r1, r1, i_wt_den_p6       ; mov rb11, ra3.8d
/* [0x00000720] */ 0x8d827236, 0x1002531e, // sub rb_wt_off, r1, r0         ; mov ra_link, unif
/* [0x00000728] */ 0x95080ff6, 0x1e024287, // mov ra10, rb_xshift2          ; mov rb7,  ra2.8d
// :1
/* [0x00000730] */ 0x0d9d1bc0, 0xa00229e7, // sub.setf -, r5, rb_i_tmu      ; nop                           ; ldtmu0
/* [0x00000738] */ 0x8e5539bf, 0x1202888f, // shr r2, r4, ra_xshift         ; mov.ifz rb_base2, rb_base2_next
/* [0x00000740] */ 0x8e4c85f6, 0xd0029851, // shr r1, r2, v_v_shift         ; mov.ifz ra_y_y2, ra_y_y2_next
/* [0x00000748] */ 0x8c683ff6, 0x1002b9d8, // add.setf -, rb_ef, rb_ef      ; mov.ifz ra_base, ra_base_next
/* [0x00000750] */ 0x8c441fb6, 0xd4224463, // add ra_y, 1, ra_y             ; mov r3, ra_y
/* [0x00000758] */ 0x93531789, 0xd80248e0, // max r3, r3, ra_k0             ; mov      r0, r1 << 15
/* [0x00000760] */ 0x9227f792, 0xd003c8e1, // min r3, r3, ra9               ; mov.ifnc r1, r2 << 1
/* [0x00000768] */ 0x559d049f, 0x100e4823, // mov.ifnc r0, r2               ; mul24 r3, r3, rb_pitch
/* [0x00000770] */ 0x8c618cc7, 0x10024e20, // add t0s, ra_base, r3          ; v8min r0, r0, rb_pmask
/* [0x00000778] */ 0x540183f0, 0x18024862, // and r1, r1, rb_pmask          ; mul24      r2, ra0.8a,       r0
/* [0x00000780] */ 0x4d01feb1, 0x1e0248a3, // sub r2, rb_fir_off_h, r2      ; mul24      r3, ra0.8d,       r1
/* [0x00000788] */ 0x4d03e4f0, 0xda0248a3, // sub r2, r2, r3                ; mul24      r3, ra0.8b << 2,  r0 << 2  @ "mul_used", 0
/* [0x00000790] */ 0x40034031, 0xda0109e3, // nop                           ; mul24.ifn  r3, ra0.8b << 12, r1 << 12 @ "mul_used", 0
/* [0x00000798] */ 0x4c03c4f0, 0xdc0248a3, // add r2, r2, r3                ; mul24      r3, ra0.8c << 4,  r0 << 4  @ "mul_used", 0
/* [0x000007a0] */ 0x40032031, 0xdc0109e3, // nop                           ; mul24.ifn  r3, ra0.8c << 14, r1 << 14 @ "mul_used", 0
/* [0x000007a8] */ 0x4c0854fe, 0xb8025804, // add r0, r2, r3                ; mul24 ra4, rb5, ra2.8a        ; ldtmu1
/* [0x000007b0] */ 0x8e2869bf, 0x10024885, // shr r2, r4, ra10              ; mov rb5, rb6
/* [0x000007b8] */ 0x8e4485f6, 0xd2024863, // shr r1, r2, v_v_shift         ; mov r3, ra_y2
/* [0x000007c0] */ 0x8e1c01f6, 0xd00241c6, // shr ra7, r0, v_bit_depth - 8  ; mov rb6, ra7
/* [0x000007c8] */ 0x8c531789, 0xda124460, // add ra_y2, r3, ra_k1          ; mov      r0, r1 << 15
/* [0x000007d0] */ 0x9353f792, 0xd803c8e1, // max r3, r3, ra_k0             ; mov.ifnc r1, r2 << 1
/* [0x000007d8] */ 0x925de7ce, 0x120248e1, // min r3, r3, rb_max_y          ; v8min r1, r1, ra_pmax
/* [0x000007e0] */ 0x559d049f, 0x100e4823, // mov.ifnc r0, r2               ; mul24 r3, r3, rb_pitch
/* [0x000007e8] */ 0x8c5cfec6, 0x12024f20, // add t1s, rb_base2, r3         ; v8min r0, r0, ra_pmax
/* [0x000007f0] */ 0x4c041bf0, 0xd8025962, // add r5rep, r5, 1              ; mul24      r2, ra1.8a,       r0
/* [0x000007f8] */ 0x4d05feb1, 0x1e0248a3, // sub r2, rb_fir_off_h, r2      ; mul24      r3, ra1.8d,       r1
/* [0x00000800] */ 0x4d07e4f0, 0xda0248a3, // sub r2, r2, r3                ; mul24      r3, ra1.8b << 2,  r0 << 2  @ "mul_used", 0
/* [0x00000808] */ 0x40074031, 0xda0109e3, // nop                           ; mul24.ifn  r3, ra1.8b << 12, r1 << 12 @ "mul_used", 0
/* [0x00000810] */ 0x4c07c6b0, 0xdc0248a3, // add r2, r3, r2                ; mul24      r3, ra1.8c << 4,  r0 << 4  @ "mul_used", 0
/* [0x00000818] */ 0x4c072b71, 0xdc0329e3, // add.setf -, r5, r5            ; mul24.ifn  r3, ra1.8c << 14, r1 << 14 @ "mul_used", 0
/* [0x00000820] */ 0xfffffef0, 0xf06809e7, // brr.anyn -, r:1b
/* [0x00000828] */ 0x4c0c94fe, 0x180248a0, // add r2, r2, r3                ; mul24 r0, rb9,  ra3.8a
/* [0x00000830] */ 0x550caffe, 0x1a025261, // mov rb9, rb10                 ; mul24 r1, rb10, ra3.8b
/* [0x00000838] */ 0x8e2c05f6, 0xd00242ca, // shr ra11, r2, v_bit_depth - 8 ; mov rb10, ra11
/* [0x00000840] */ 0x4d08523e, 0x1a0248a1, // sub r2, r1, r0                ; mul24 r1, rb5,  ra2.8b
/* [0x00000848] */ 0x8d112bf6, 0x100269e0, // sub.setf -, r5, rb_lcount     ; mov r0, ra4
/* [0x00000850] */ 0x4d08623e, 0x1c024860, // sub r1, r1, r0                ; mul24 r0, rb6,  ra2.8c
/* [0x00000858] */ 0x4c1c7237, 0x10024860, // add r1, r1, r0                ; mul24 r0, ra7,  rb7
/* [0x00000860] */ 0x4d0ca23e, 0x1c024860, // sub r1, r1, r0                ; mul24 r0, rb10, ra3.8c
/* [0x00000868] */ 0x4c2cb437, 0x100248a0, // add r2, r2, r0                ; mul24 r0, ra11, rb11
/* [0x00000870] */ 0x0d9e7400, 0x100208a7, // sub r2, r2, r0
/* [0x00000878] */ 0x0e9c63c0, 0xd0020867, // shr r1, r1, 6
/* [0x00000880] */ 0x4e5865ce, 0xd20248a0, // shr r2, r2, 6                 ; mul24 r0, r1, ra_wt_mul_l0
/* [0x00000888] */ 0x4c4a7456, 0x120248a1, // add r2, r2, r1                ; mul24 r1, r2, ra_wt_mul_l1
/* [0x00000890] */ 0x4c667216, 0x14024862, // add r1, r1, r0                ; mul24 r2, r2, ra_kmul_add
/* [0x00000898] */ 0x8d5e72b6, 0x1c024863, // sub r1, r1, r2                ; mov r3, ra_blk_height
/* [0x000008a0] */ 0xec40c3f3, 0x12024860, // add r1, r1, rb_wt_off         ; v8subs r0, ra_height, r3
/* [0x000008a8] */ 0xfffffe68, 0xf06809e7, // brr.anyn -, r:1b
/* [0x000008b0] */ 0x0f667380, 0x18020867, // asr r1, r1, ra_wt_den_p7
/* [0x000008b8] */ 0x925f23bf, 0x12020867, // min r1, r1, ra_pmax           ; mov -, vw_wait
/* [0x000008c0] */ 0x5351039f, 0x18024c22, // max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch
/* [0x000008c8] */ 0x956e7036, 0x10126431, // mov.setf ra_height, r0        ; mov vw_setup, ra_dma0
/* [0x000008d0] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x000008d8] */ 0x929dd0ff, 0x10024831, // min r0, r0, r3                ; mov vw_setup, rb_dma1
/* [0x000008e0] */ 0x8d7270f6, 0x10024872, // sub r1, r0, r3                ; mov vw_addr, ra_dest
/* [0x000008e8] */ 0x119d73c0, 0xd0020867, // shl r1, r1, i_shift23
/* [0x000008f0] */ 0xfffffe20, 0xf0f809e7, // brr -, r:1b
/* [0x000008f8] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x00000900] */ 0x0c6e7c40, 0x100206e7, // add ra_dma0, ra_dma0, r1
/* [0x00000908] */ 0x8c71ccbf, 0x10024731, // add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init
// ::mc_sync_q0
/* [0x00000910] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000918] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000920] */ 0x00000010, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000928] */ 0x00000010, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000930] */ 0x00000010, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000938] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000940] */ 0x0000001c, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000948] */ 0x00000001, 0xe80009e7, // mov  dst, srel(i)
/* [0x00000950] */ 0x0000000d, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q1
/* [0x00000958] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000960] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000968] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000970] */ 0x00000000, 0xe80009e7, // mov  dst, srel(i)
/* [0x00000978] */ 0x00000011, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000980] */ 0x00000002, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q2
/* [0x00000988] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000990] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000998] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x000009a0] */ 0x00000000, 0xe80009e7, // mov  dst, srel(i)
/* [0x000009a8] */ 0x00000012, 0xe80009e7, // mov  dst, sacq(i)
/* [0x000009b0] */ 0x00000003, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q3
/* [0x000009b8] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x000009c0] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x000009c8] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x000009d0] */ 0x00000000, 0xe80009e7, // mov  dst, srel(i)
/* [0x000009d8] */ 0x00000013, 0xe80009e7, // mov  dst, sacq(i)
/* [0x000009e0] */ 0x009e7000, 0x100009e7, // nop
// ::mc_sync_q4
/* [0x000009e8] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x000009f0] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x000009f8] */ 0x00000014, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000a00] */ 0x00000014, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000a08] */ 0x00000014, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000a10] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000a18] */ 0x0000001d, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000a20] */ 0x00000005, 0xe80009e7, // mov  dst, srel(i)
/* [0x00000a28] */ 0x0000000e, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q5
/* [0x00000a30] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000a38] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000a40] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000a48] */ 0x00000004, 0xe80009e7, // mov  dst, srel(i)
/* [0x00000a50] */ 0x00000015, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000a58] */ 0x00000006, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q6
/* [0x00000a60] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000a68] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000a70] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000a78] */ 0x00000004, 0xe80009e7, // mov  dst, srel(i)
/* [0x00000a80] */ 0x00000016, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000a88] */ 0x00000007, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q7
/* [0x00000a90] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000a98] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000aa0] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000aa8] */ 0x00000004, 0xe80009e7, // mov  dst, srel(i)
/* [0x00000ab0] */ 0x00000017, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000ab8] */ 0x009e7000, 0x100009e7, // nop
// ::mc_sync_q8
/* [0x00000ac0] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000ac8] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000ad0] */ 0x00000018, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000ad8] */ 0x00000018, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000ae0] */ 0x00000018, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000ae8] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000af0] */ 0x0000001e, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000af8] */ 0x00000009, 0xe80009e7, // mov  dst, srel(i)
/* [0x00000b00] */ 0x0000000c, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q9
/* [0x00000b08] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000b10] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000b18] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000b20] */ 0x00000008, 0xe80009e7, // mov  dst, srel(i)
/* [0x00000b28] */ 0x00000019, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000b30] */ 0x0000000a, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q10
/* [0x00000b38] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000b40] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000b48] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000b50] */ 0x00000008, 0xe80009e7, // mov  dst, srel(i)
/* [0x00000b58] */ 0x0000001a, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000b60] */ 0x0000000b, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync_q11
/* [0x00000b68] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000b70] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000b78] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000b80] */ 0x00000008, 0xe80009e7, // mov  dst, srel(i)
/* [0x00000b88] */ 0x0000001b, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000b90] */ 0x009e7000, 0x100009e7, // nop
// ::mc_exit_c_qn
// ::mc_exit_y_qn
/* [0x00000b98] */ 0x00000002, 0xe00228e7, // mov.setf r3, PREREAD - 1
// :1
/* [0x00000ba0] */ 0xffffffe0, 0xf03809e7, // brr.anynz -, r:1b
/* [0x00000ba8] */ 0x009e7000, 0xa00009e7, // nop                   ; nop           ; ldtmu0
/* [0x00000bb0] */ 0x009e7000, 0xb00009e7, // nop                   ; nop           ; ldtmu1
/* [0x00000bb8] */ 0x0d9c17c0, 0xd00228e7, // sub.setf r3, r3, 1
/* [0x00000bc0] */ 0x159f2fc0, 0x100009e7, // mov  -, vw_wait
/* [0x00000bc8] */ 0x009e7000, 0x300009e7, // nop                   ; nop           ; thrend
/* [0x00000bd0] */ 0x009e7000, 0x100009e7, // nop
/* [0x00000bd8] */ 0x009e7000, 0x100009e7, // nop
// ::mc_exit_c_q0
// ::mc_exit_y_q0
/* [0x00000be0] */ 0x00000002, 0xe00228e7, // mov.setf r3, PREREAD - 1
// :1
/* [0x00000be8] */ 0xffffffe0, 0xf03809e7, // brr.anynz -, r:1b
/* [0x00000bf0] */ 0x009e7000, 0xa00009e7, // nop                   ; nop           ; ldtmu0
/* [0x00000bf8] */ 0x009e7000, 0xb00009e7, // nop                   ; nop           ; ldtmu1
/* [0x00000c00] */ 0x0d9c17c0, 0xd00228e7, // sub.setf r3, r3, 1
/* [0x00000c08] */ 0x159f2fc0, 0x100009e7, // mov  -, vw_wait
/* [0x00000c10] */ 0x0000001c, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00000c18] */ 0x009e7000, 0x300009e7, // nop                   ; nop           ; thrend
/* [0x00000c20] */ 0x00000001, 0xe00209a7, // mov interrupt, 1
/* [0x00000c28] */ 0x009e7000, 0x100009e7, // nop
// ::mc_setup_y_q0
/* [0x00000c30] */ 0x0000000c, 0xe80009e7, // mov  dst, srel(i)
// ::mc_setup_y_qn
/* [0x00000c38] */ 0x95801ff6, 0xd0025900, // mov tmurs, 1                  ; mov ra0, unif
/* [0x00000c40] */ 0x15827d80, 0x10020267, // mov ra9, unif
/* [0x00000c48] */ 0x15827d80, 0x10020067, // mov ra1, unif
/* [0x00000c50] */ 0xaaaaff00, 0xe6020827, // mov r0, [0,2,0,2,0,2,0,2,1,3,1,3,1,3,1,3]
/* [0x00000c58] */ 0x9181e1f6, 0xd00250cb, // shl rb_ef, r0, i_shift30      ; mov ra11, unif
/* [0x00000c60] */ 0xff800100, 0xe0020527, // mov ra_kff800100, 0xff800100
/* [0x00000c68] */ 0x000000ff, 0xe0021627, // mov rb_pmask, v_pmask
/* [0x00000c70] */ 0x001000ff, 0xe00205e7, // mov ra_blk_height_pmax, ((1 << v_bit_depth) - 1) | (v_blk_height << 16)
/* [0x00000c78] */ 0x00004000, 0xe00217e7, // mov rb_fir_off_h, (FIR_OFFSET << (v_bit_depth - 8))
/* [0x00000c80] */ 0x4000000e, 0xe0020667, // mov ra_fir_off_val_wt_den_p7, (FIR_OFFSET << 16) | (DENOM + 15 - v_bit_depth)
/* [0x00000c88] */ 0x050b0a00, 0xe0021567, // mov rb_y_coeffs_2, 0x050b0a00
/* [0x00000c90] */ 0x11283a40, 0xe00215a7, // mov rb_y_coeffs_3, 0x11283a40
/* [0x00000c98] */ 0x0a0b0500, 0xe00215e7, // mov rb_y_coeffs_5, 0x0a0b0500
/* [0x00000ca0] */ 0x15827d80, 0x100200e7, // mov ra3, unif
/* [0x00000ca8] */ 0x95803ff6, 0x10024754, // mov ra_ef, rb_ef              ; mov rb_xpitch, unif
/* [0x00000cb0] */ 0x0d0c1dc0, 0xd40216a7, // sub rb_max_x, ra3.16b, 1
/* [0x00000cb8] */ 0x0d0c1dc0, 0xd20217a7, // sub rb_max_y, ra3.16a, 1
/* [0x00000cc0] */ 0x959a0dbf, 0x100248d0, // mov r3, elem_num              ; mov rb_pitch, unif
/* [0x00000cc8] */ 0xc0000000, 0xe0020867, // mov r1, vdw_setup_1(0)
/* [0x00000cd0] */ 0x159d03c0, 0x10021667, // or  rb_dma1_base, r1, rb_pitch
/* [0x00000cd8] */ 0x0c027cc0, 0x14020827, // add r0, ra0.16b, r3
/* [0x00000ce0] */ 0x139c01c0, 0xd0020827, // max r0, r0, 0
/* [0x00000ce8] */ 0x129da1c0, 0x10020827, // min r0, r0, rb_max_x
/* [0x00000cf0] */ 0x119c31c0, 0xd0220567, // shl ra_xshift_next, r0, 3
/* [0x00000cf8] */ 0xf49dc1d2, 0xd0024822, // and r0, r0, -4                ; v8subs r2, r2, r2
/* [0x00000d00] */ 0x0d9d05c0, 0x100208a7, // sub r2, r2, rb_pitch
/* [0x00000d08] */ 0x149e7080, 0x10020867, // and r1, r0, r2
/* [0x00000d10] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00000d18] */ 0x0c9e7040, 0x10020827, // add r0, r0, r1
/* [0x00000d20] */ 0x0c267c00, 0x10020627, // add ra_base, ra9, r0
/* [0x00000d28] */ 0x0c067cc0, 0x14020827, // add r0, ra1.16b, r3
/* [0x00000d30] */ 0x139c01c0, 0xd0020827, // max r0, r0, 0
/* [0x00000d38] */ 0x129da1c0, 0x10020827, // min r0, r0, rb_max_x
/* [0x00000d40] */ 0x119c31c0, 0xd0021067, // shl rb_xshift2_next, r0, 3
/* [0x00000d48] */ 0x149dc1c0, 0xd0020827, // and r0, r0, -4
/* [0x00000d50] */ 0x149e7080, 0x10020867, // and r1, r0, r2
/* [0x00000d58] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00000d60] */ 0x0c9e7040, 0x10020827, // add r0, r0, r1
/* [0x00000d68] */ 0x0c2e7c00, 0x100213e7, // add rb_base2, ra11, r0
/* [0x00000d70] */ 0x80027036, 0x120049e0, // nop                           ; mov r0, ra0.16a
/* [0x00000d78] */ 0x95043ff6, 0xd20248e2, // mov r3, PREREAD               ; mov r2, ra1.16a
// :1
/* [0x00000d80] */ 0x0d9c17c0, 0xd00228e7, // sub.setf r3, r3, 1
/* [0x00000d88] */ 0x139c01c0, 0xd0020867, // max r1, r0, 0
/* [0x00000d90] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_max_y
/* [0x00000d98] */ 0x4c51018f, 0x1a024821, // add r0, r0, ra_k1             ; mul24 r1, r1, rb_pitch
/* [0x00000da0] */ 0x8c627c40, 0x10225e11, // add t0s, ra_base, r1          ; mov ra_y, r0
/* [0x00000da8] */ 0x139c05c0, 0xd0020867, // max r1, r2, 0
/* [0x00000db0] */ 0xffffffb0, 0xf03809e7, // brr.anynz -, r:1b
/* [0x00000db8] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_max_y
/* [0x00000dc0] */ 0x4c51058f, 0x1a0248a1, // add r2, r2, ra_k1             ; mul24 r1, r1, rb_pitch
/* [0x00000dc8] */ 0x8c9cfe52, 0x10125f11, // add t1s, rb_base2, r1         ; mov ra_y2, r2
/* [0x00000dd0] */ 0x159e6fc0, 0x100208a7, // mov r2, qpu_num
/* [0x00000dd8] */ 0x0f9c25c0, 0xd0020867, // asr r1, r2, 2
/* [0x00000de0] */ 0x119c63c0, 0xd0020867, // shl r1, r1, 6
/* [0x00000de8] */ 0x149c35c0, 0xd0020827, // and r0, r2, 3
/* [0x00000df0] */ 0x159e7040, 0x10020827, // or  r0, r0, r1
/* [0x00000df8] */ 0x00004800, 0xe0020867, // mov r1, vpm_setup(0, 4, h8p(0, 0))
/* [0x00000e00] */ 0x0c9e7040, 0x10021727, // add r_vpm, r0, r1
/* [0x00000e08] */ 0x80004004, 0xe0020867, // mov r1, vdw_setup_0(0, 0, dma_h8p(0,0,0))
/* [0x00000e10] */ 0x119c51c0, 0xd0020827, // shl r0, r0, 5
/* [0x00000e18] */ 0x0c9e7040, 0x100216e7, // add r_dma, r0, r1
/* [0x00000e20] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00000e28] */ 0x00000000, 0xe0024208, // mov ra8,  0                   ; mov rb8,  0
/* [0x00000e30] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000e38] */ 0x00000000, 0xe0024249, // mov ra9,  0                   ; mov rb9,  0
/* [0x00000e40] */ 0x00000000, 0xe002428a, // mov ra10, 0                   ; mov rb10, 0
/* [0x00000e48] */ 0x00000000, 0xe00242cb, // mov ra11, 0                   ; mov rb11, 0
// :per_block_setup_8
/* [0x00000e50] */ 0x93567176, 0x14125815, // max r0, r0, r5                ; mov ra_xshift, ra_xshift_next
/* [0x00000e58] */ 0x129da1c0, 0x10020827, // min r0, r0, rb_max_x
/* [0x00000e60] */ 0x119c31c0, 0xd0220567, // shl ra_xshift_next, r0, 3
/* [0x00000e68] */ 0x149dc1c0, 0xd0020827, // and r0, r0, -4
/* [0x00000e70] */ 0x8d810bf6, 0x1002589a, // sub r2, r5, rb_pitch          ; mov ra_base_next, unif
/* [0x00000e78] */ 0x940270b6, 0x12225853, // and r1, r0, r2                ; mov ra_y_next, ra0.16a
/* [0x00000e80] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00000e88] */ 0x8c827076, 0x10025801, // add r0, r0, r1                ; mov ra1, unif
/* [0x00000e90] */ 0x0c6a7c00, 0x100206a7, // add ra_base_next, ra_base_next, r0
/* [0x00000e98] */ 0x0c067cc0, 0x14020827, // add r0, ra1.16b, r3
/* [0x00000ea0] */ 0x93067176, 0x12125813, // max r0, r0, r5                ; mov ra_y2_next, ra1.16a
/* [0x00000ea8] */ 0x9281a1f6, 0x10024813, // min r0, r0, rb_max_x          ; mov rb_base2_next, unif
/* [0x00000eb0] */ 0x119c31c0, 0xd0021067, // shl rb_xshift2_next, r0, 3
/* [0x00000eb8] */ 0x9481c1f6, 0xd0025810, // and r0, r0, -4                ; mov ra_width_height, unif
/* [0x00000ec0] */ 0x949dc0bf, 0x10024871, // and r1, r0, r2                ; mov vw_setup, rb_vpm_init
/* [0x00000ec8] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00000ed0] */ 0x4c401077, 0xd4024821, // add r0, r0, r1                ; mul24 r1, ra_width, v_x_mul
/* [0x00000ed8] */ 0x0c9d3e00, 0x100214e7, // add rb_base2_next, rb_base2_next, r0
/* [0x00000ee0] */ 0x8d419e76, 0x12025760, // sub rb_dma1, rb_dma1_base, r1 ; mov r0, ra_height
/* [0x00000ee8] */ 0x8c5dc1c6, 0xdc025460, // add rb_i_tmu, r0, (7-8) - PREREAD ; v8min r0, r0, ra_blk_height
/* [0x00000ef0] */ 0x0c9df1c0, 0xd00214a7, // add rb_lcount, r0, (7-8)
/* [0x00000ef8] */ 0x916471f6, 0xd4024823, // shl r0, r0, v_dma_h_shift     ; mov r3, ra_kmul_add
/* [0x00000f00] */ 0x0c9e7040, 0x10020827, // add r0, r0, r1
/* [0x00000f08] */ 0x916501f6, 0xd4024822, // shl r0, r0, v_dma_wh_shift    ; mov r2, ra_fir_off_val
/* [0x00000f10] */ 0x8c81b1f6, 0x100246e0, // add ra_dma0, r0, rb_dma0_base ; mov r0, unif
/* [0x00000f18] */ 0x918101f6, 0xd00a5816, // shl.ifnn r0, r0, i_shift16    ; mov ra_wt_off_mul_l0, unif
/* [0x00000f20] */ 0x915031f6, 0xde024205, // shl ra8, r0, 3                ; mov rb5, ra_k255
/* [0x00000f28] */ 0x01040400, 0xe0020867, // mov r1, 0x01040400
/* [0x00000f30] */ 0x10227380, 0x1e5200a7, // ror ra2.8b, r1, ra8.8d
/* [0x00000f38] */ 0x10227380, 0x1c520027, // ror ra0.8b, r1, ra8.8c
/* [0x00000f40] */ 0x10215f80, 0x1e6200a7, // ror ra2.8c, rb_y_coeffs_2, ra8.8d
/* [0x00000f48] */ 0x10215f80, 0x1c620027, // ror ra0.8c, rb_y_coeffs_2, ra8.8c
/* [0x00000f50] */ 0x00010100, 0xe0020867, // mov r1,0x00010100
/* [0x00000f58] */ 0x902203bf, 0x1e025812, // ror r0, r1, ra8.8d            ; mov ra_wt_off_mul_l1, unif
/* [0x00000f60] */ 0x90205387, 0x1c424004, // ror ra0.8a, r1, ra8.8c        ; v8min rb4, r0, rb5
/* [0x00000f68] */ 0x914883f6, 0xd0031856, // shl r1, r1, 8                 ; mov.ifn ra_wt_off_mul_l0, ra_wt_off_mul_l1
/* [0x00000f70] */ 0x902203bf, 0x1e02581c, // ror r0, r1, ra8.8d            ; mov ra_dest, unif
/* [0x00000f78] */ 0x90205387, 0x1c72404b, // ror ra1.8d, r1, ra8.8c        ; v8min rb11, r0, rb5
/* [0x00000f80] */ 0x10216f80, 0x1e7200a7, // ror ra2.8d, rb_y_coeffs_3, ra8.8d
/* [0x00000f88] */ 0x10216f80, 0x1c720027, // ror ra0.8d, rb_y_coeffs_3, ra8.8c
/* [0x00000f90] */ 0x10217f80, 0x1e5200e7, // ror ra3.8b, rb_y_coeffs_5, ra8.8d
/* [0x00000f98] */ 0x10217f80, 0x1c520067, // ror ra1.8b, rb_y_coeffs_5, ra8.8c
/* [0x00000fa0] */ 0x04040100, 0xe0020867, // mov r1,0x04040100
/* [0x00000fa8] */ 0x10227380, 0x1e6200e7, // ror ra3.8c, r1, ra8.8d
/* [0x00000fb0] */ 0x902183bf, 0xdc624065, // ror ra1.8c, r1, ra8.8c        ; mov r5rep, -8
/* [0x00000fb8] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00000fc0] */ 0x3a281100, 0xe0020867, // mov r1,0x3a281100
/* [0x00000fc8] */ 0x902203bf, 0x1e02581e, // ror r0, r1, ra8.8d            ; mov ra_link, unif
/* [0x00000fd0] */ 0x90205387, 0x1c424048, // ror ra1.8a, r1, ra8.8c        ; v8min rb8, r0, rb5
// ::mc_filter_y_pxx
/* [0x00000fd8] */ 0xfffffe58, 0xf0f807a7, // brr ra_link, r:per_block_setup_8
/* [0x00000fe0] */ 0x959a0ff6, 0x10024023, // mov ra0, unif                 ; mov r3, elem_num
/* [0x00000fe8] */ 0xec9c3fd2, 0x100269e5, // add.setf -, rb_ef, rb_ef      ; v8subs r5rep, r2, r2
/* [0x00000ff0] */ 0x8c001cff, 0x14024800, // add r0, ra0.16b, r3           ; mov rb_xshift2, rb_xshift2_next
/* [0x00000ff8] */ 0x1158cdc0, 0xd4020867, // shl r1, ra_wt_off_l0, i_wt_den_p5
/* [0x00001000] */ 0x4c5a7cd6, 0x121245a0, // add ra_wt_mul_l0, ra_wt_mul_l0, r3 ; mul24 r0, r2, ra_wt_mul_l0
/* [0x00001008] */ 0x8d9c423f, 0x1042531d, // sub rb_wt_off, r1, r0         ; mov ra_ef.8a, rb4
// :1
/* [0x00001010] */ 0x4c745dbe, 0x100279c4, // add.setf -, ra_ef, ra_ef      ; mul24 ra4, rb5, ra_ef
/* [0x00001018] */ 0x93440dff, 0xd40248a1, // max r2, ra_y, 0               ; mov r1, 0
/* [0x00001020] */ 0x9251e5f6, 0x1a0248a3, // min r2, r2, rb_max_y          ; mov r3, ra_k1
/* [0x00001028] */ 0x4c450cd7, 0xa4224462, // add ra_y, ra_y, r3            ; mul24 r2, r2, rb_pitch        ; ldtmu0
/* [0x00001030] */ 0x8c606cbf, 0x10024e05, // add t0s, ra_base, r2          ; mov rb5,  rb6
/* [0x00001038] */ 0x8e5479bf, 0x12024806, // shr r0, r4, ra_xshift         ; mov rb6,  rb7
/* [0x00001040] */ 0x93458c47, 0xb20248a0, // max r2, ra_y2, r1             ; v8min r0, r0, rb_pmask        ; ldtmu1
/* [0x00001048] */ 0x8e2009f6, 0x10024847, // shr r1, r4, rb_xshift2        ; mov rb7, ra8
/* [0x00001050] */ 0x925de5ce, 0x120248a1, // min r2, r2, rb_max_y          ; v8min r1, r1, ra_pmax
/* [0x00001058] */ 0x4c450cd7, 0x12124462, // add ra_y2, ra_y2, r3          ; mul24 r2, r2, rb_pitch
/* [0x00001060] */ 0x8c24feb6, 0x10025f08, // add t1s, rb_base2, r2         ; mov ra8,  ra9
/* [0x00001068] */ 0x4c038af1, 0xd8025962, // add r5rep, r5, r3     ; mul24      r2, ra0.8a << 8,  r1 << 8  @ "mul_used", 0
/* [0x00001070] */ 0x5501fff0, 0x180348e2, // mov r3, rb_fir_off_h  ; mul24.ifnn r2, ra0.8a,       r0
/* [0x00001078] */ 0x4d03f6b0, 0xda0248a3, // sub r2, r3, r2        ; mul24      r3, ra0.8b << 1,  r0 << 1  @ "mul_used", 0
/* [0x00001080] */ 0x40037031, 0xda0109e3, // nop                   ; mul24.ifn  r3, ra0.8b << 9,  r1 << 9  @ "mul_used", 0
/* [0x00001088] */ 0x4c03e4f0, 0xdc0248a3, // add r2, r2, r3        ; mul24      r3, ra0.8c << 2,  r0 << 2  @ "mul_used", 0
/* [0x00001090] */ 0x40036031, 0xdc0109e3, // nop                   ; mul24.ifn  r3, ra0.8c << 10, r1 << 10 @ "mul_used", 0
/* [0x00001098] */ 0x4d03d4f0, 0xde0248a3, // sub r2, r2, r3        ; mul24      r3, ra0.8d << 3,  r0 << 3  @ "mul_used", 0
/* [0x000010a0] */ 0x40035031, 0xde0109e3, // nop                   ; mul24.ifn  r3, ra0.8d << 11, r1 << 11 @ "mul_used", 0
/* [0x000010a8] */ 0x4c07c4f0, 0xd80248a3, // add r2, r2, r3        ; mul24      r3, ra1.8a << 4,  r0 << 4  @ "mul_used", 0
/* [0x000010b0] */ 0x40074031, 0xd80109e3, // nop                   ; mul24.ifn  r3, ra1.8a << 12, r1 << 12 @ "mul_used", 0
/* [0x000010b8] */ 0x4c07b4f0, 0xda0248a3, // add r2, r2, r3        ; mul24      r3, ra1.8b << 5,  r0 << 5  @ "mul_used", 0
/* [0x000010c0] */ 0x40073031, 0xda0109e3, // nop                   ; mul24.ifn  r3, ra1.8b << 13, r1 << 13 @ "mul_used", 0
/* [0x000010c8] */ 0x4d07a4f0, 0xdc0248a3, // sub r2, r2, r3        ; mul24      r3, ra1.8c << 6,  r0 << 6  @ "mul_used", 0
/* [0x000010d0] */ 0x40072031, 0xdc0109e3, // nop                   ; mul24.ifn  r3, ra1.8c << 14, r1 << 14 @ "mul_used", 0
/* [0x000010d8] */ 0x4c0794f0, 0xde0248a3, // add r2, r2, r3        ; mul24      r3, ra1.8d << 7,  r0 << 7  @ "mul_used", 0
/* [0x000010e0] */ 0x4c071b71, 0xde0329e3, // add.setf -, r5, r5    ; mul24.ifn  r3, ra1.8d << 15, r1 << 15 @ "mul_used", 0
/* [0x000010e8] */ 0xffffff08, 0xf06809e7, // brr.anyn -, r:1b
/* [0x000010f0] */ 0x4d0854fe, 0x1a0248a1, // sub r2, r2, r3                ; mul24 r1, rb5,  ra2.8b
/* [0x000010f8] */ 0x550caffe, 0x1a024260, // mov ra9,  rb10                ; mul24 r0, rb10, ra3.8b
/* [0x00001100] */ 0x8f2c05f6, 0xd00242ca, // asr ra11, r2, v_bit_depth - 8 ; mov rb10, ra11
/* [0x00001108] */ 0x4d08623e, 0x1c024860, // sub r1, r1, r0                ; mul24 r0, rb6,  ra2.8c
/* [0x00001110] */ 0x4d08723e, 0x1e024860, // sub r1, r1, r0                ; mul24 r0, rb7,  ra2.8d
/* [0x00001118] */ 0x4c208237, 0x10024860, // add r1, r1, r0                ; mul24 r0, ra8,  rb8
/* [0x00001120] */ 0x4c0ca23e, 0x1c024860, // add r1, r1, r0                ; mul24 r0, rb10, ra3.8c
/* [0x00001128] */ 0x4c2cb237, 0x10024860, // add r1, r1, r0                ; mul24 r0, ra11, rb11
/* [0x00001130] */ 0x8d5d1bf6, 0x1c0269e3, // sub.setf -, r5, rb_i_tmu      ; mov r3, ra_blk_height
/* [0x00001138] */ 0x8d1133bf, 0x1002884f, // sub r1, r1, ra4               ; mov.ifz rb_base2, rb_base2_next
/* [0x00001140] */ 0x8d6a7236, 0x10029858, // sub r1, r1, r0                ; mov.ifz ra_base, ra_base_next
/* [0x00001148] */ 0x8f4c63f6, 0xd0029851, // asr r1, r1, 6                 ; mov.ifz ra_y_y2, ra_y_y2_next
/* [0x00001150] */ 0x4d592bce, 0x120269e0, // sub.setf -, r5, rb_lcount     ; mul24 r0, r1, ra_wt_mul_l0
/* [0x00001158] */ 0x4c64c1ce, 0x14024821, // add r0, r0, rb_wt_off         ; mul24 r1, r1, ra_kmul_add
/* [0x00001160] */ 0xed427073, 0x12024860, // sub r1, r0, r1                ; v8subs r0, ra_height, r3
/* [0x00001168] */ 0xfffffe88, 0xf06809e7, // brr.anyn -, r:1b
/* [0x00001170] */ 0x0f9cd3c0, 0xd0020867, // asr r1, r1, i_wt_den_p6
/* [0x00001178] */ 0x925f23bf, 0x12020867, // min r1, r1, ra_pmax           ; mov -, vw_wait
/* [0x00001180] */ 0x5351039f, 0x18024c22, // max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch
/* [0x00001188] */ 0x956e7036, 0x10126431, // mov.setf ra_height, r0        ; mov vw_setup, ra_dma0
/* [0x00001190] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x00001198] */ 0x929dd0ff, 0x10024831, // min r0, r0, r3                ; mov vw_setup, rb_dma1
/* [0x000011a0] */ 0x8d7270f6, 0x10024872, // sub r1, r0, r3                ; mov vw_addr, ra_dest
/* [0x000011a8] */ 0x119d73c0, 0xd0020867, // shl r1, r1, i_shift23
/* [0x000011b0] */ 0xfffffe40, 0xf0f809e7, // brr -, r:1b
/* [0x000011b8] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x000011c0] */ 0x0c6e7c40, 0x100206e7, // add ra_dma0, ra_dma0, r1
/* [0x000011c8] */ 0x8c71ccbf, 0x10024731, // add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init
// ::mc_filter_y_bxx
/* [0x000011d0] */ 0xfffffc60, 0xf0f807a7, // brr ra_link, r:per_block_setup_8
/* [0x000011d8] */ 0x959a0ff6, 0x10024023, // mov ra0, unif                 ; mov r3, elem_num
/* [0x000011e0] */ 0xec9c3fd2, 0x100269e5, // add.setf -, rb_ef, rb_ef      ; v8subs r5rep, r2, r2
/* [0x000011e8] */ 0x8c001cff, 0x14024800, // add r0, ra0.16b, r3           ; mov rb_xshift2, rb_xshift2_next
/* [0x000011f0] */ 0x1158ddc0, 0xd4020867, // shl r1, ra_wt_off_l0, i_wt_den_p6
/* [0x000011f8] */ 0x4c5a7cd6, 0x121245a0, // add ra_wt_mul_l0, ra_wt_mul_l0, r3 ; mul24 r0, r2, ra_wt_mul_l0
/* [0x00001200] */ 0x4d4a7216, 0x12024860, // sub r1, r1, r0                ; mul24 r0, r2, ra_wt_mul_l1
/* [0x00001208] */ 0x8d9c423f, 0x1042531d, // sub rb_wt_off, r1, r0         ; mov ra_ef.8a, rb4
// :1
/* [0x00001210] */ 0x4c745dbe, 0x100279c4, // add.setf -, ra_ef, ra_ef      ; mul24 ra4, rb5, ra_ef
/* [0x00001218] */ 0x93440dff, 0xd40248a1, // max r2, ra_y, 0               ; mov r1, 0
/* [0x00001220] */ 0x9251e5f6, 0x1a0248a3, // min r2, r2, rb_max_y          ; mov r3, ra_k1
/* [0x00001228] */ 0x4c450cd7, 0xa4224462, // add ra_y, ra_y, r3            ; mul24 r2, r2, rb_pitch        ; ldtmu0
/* [0x00001230] */ 0x8c606cbf, 0x10024e05, // add t0s, ra_base, r2          ; mov rb5,  rb6
/* [0x00001238] */ 0x8e5479bf, 0x12024806, // shr r0, r4, ra_xshift         ; mov rb6,  rb7
/* [0x00001240] */ 0x93458c47, 0xb20248a0, // max r2, ra_y2, r1             ; v8min r0, r0, rb_pmask        ; ldtmu1
/* [0x00001248] */ 0x8e2009f6, 0x10024847, // shr r1, r4, rb_xshift2        ; mov rb7, ra8
/* [0x00001250] */ 0x925de5ce, 0x120248a1, // min r2, r2, rb_max_y          ; v8min r1, r1, ra_pmax
/* [0x00001258] */ 0x4c450cd7, 0x12124462, // add ra_y2, ra_y2, r3          ; mul24 r2, r2, rb_pitch
/* [0x00001260] */ 0x8c24feb6, 0x10025f08, // add t1s, rb_base2, r2         ; mov ra8,  ra9
/* [0x00001268] */ 0x4c038af1, 0xd8025962, // add r5rep, r5, r3     ; mul24      r2, ra0.8a << 8,  r1 << 8  @ "mul_used", 0
/* [0x00001270] */ 0x5501fff0, 0x180348e2, // mov r3, rb_fir_off_h  ; mul24.ifnn r2, ra0.8a,       r0
/* [0x00001278] */ 0x4d03f6b0, 0xda0248a3, // sub r2, r3, r2        ; mul24      r3, ra0.8b << 1,  r0 << 1  @ "mul_used", 0
/* [0x00001280] */ 0x40037031, 0xda0109e3, // nop                   ; mul24.ifn  r3, ra0.8b << 9,  r1 << 9  @ "mul_used", 0
/* [0x00001288] */ 0x4c03e4f0, 0xdc0248a3, // add r2, r2, r3        ; mul24      r3, ra0.8c << 2,  r0 << 2  @ "mul_used", 0
/* [0x00001290] */ 0x40036031, 0xdc0109e3, // nop                   ; mul24.ifn  r3, ra0.8c << 10, r1 << 10 @ "mul_used", 0
/* [0x00001298] */ 0x4d03d4f0, 0xde0248a3, // sub r2, r2, r3        ; mul24      r3, ra0.8d << 3,  r0 << 3  @ "mul_used", 0
/* [0x000012a0] */ 0x40035031, 0xde0109e3, // nop                   ; mul24.ifn  r3, ra0.8d << 11, r1 << 11 @ "mul_used", 0
/* [0x000012a8] */ 0x4c07c4f0, 0xd80248a3, // add r2, r2, r3        ; mul24      r3, ra1.8a << 4,  r0 << 4  @ "mul_used", 0
/* [0x000012b0] */ 0x40074031, 0xd80109e3, // nop                   ; mul24.ifn  r3, ra1.8a << 12, r1 << 12 @ "mul_used", 0
/* [0x000012b8] */ 0x4c07b4f0, 0xda0248a3, // add r2, r2, r3        ; mul24      r3, ra1.8b << 5,  r0 << 5  @ "mul_used", 0
/* [0x000012c0] */ 0x40073031, 0xda0109e3, // nop                   ; mul24.ifn  r3, ra1.8b << 13, r1 << 13 @ "mul_used", 0
/* [0x000012c8] */ 0x4d07a4f0, 0xdc0248a3, // sub r2, r2, r3        ; mul24      r3, ra1.8c << 6,  r0 << 6  @ "mul_used", 0
/* [0x000012d0] */ 0x40072031, 0xdc0109e3, // nop                   ; mul24.ifn  r3, ra1.8c << 14, r1 << 14 @ "mul_used", 0
/* [0x000012d8] */ 0x4c0794f0, 0xde0248a3, // add r2, r2, r3        ; mul24      r3, ra1.8d << 7,  r0 << 7  @ "mul_used", 0
/* [0x000012e0] */ 0x4c071b71, 0xde0329e3, // add.setf -, r5, r5    ; mul24.ifn  r3, ra1.8d << 15, r1 << 15 @ "mul_used", 0
/* [0x000012e8] */ 0xffffff08, 0xf06809e7, // brr.anyn -, r:1b
/* [0x000012f0] */ 0x4d0854fe, 0x1a0248a1, // sub r2, r2, r3                ; mul24 r1, rb5,  ra2.8b
/* [0x000012f8] */ 0x550caffe, 0x1a024260, // mov ra9,  rb10                ; mul24 r0, rb10, ra3.8b
/* [0x00001300] */ 0x8f2c05f6, 0xd00242ca, // asr ra11, r2, v_bit_depth - 8 ; mov rb10, ra11
/* [0x00001308] */ 0x4d08623e, 0x1c024860, // sub r1, r1, r0                ; mul24 r0, rb6,  ra2.8c
/* [0x00001310] */ 0x4d08723e, 0x1e024860, // sub r1, r1, r0                ; mul24 r0, rb7,  ra2.8d
/* [0x00001318] */ 0x4c208237, 0x10024860, // add r1, r1, r0                ; mul24 r0, ra8,  rb8
/* [0x00001320] */ 0x4c0ca23e, 0x1c024860, // add r1, r1, r0                ; mul24 r0, rb10, ra3.8c
/* [0x00001328] */ 0x4c2cb237, 0x10024860, // add r1, r1, r0                ; mul24 r0, ra11, rb11
/* [0x00001330] */ 0x0d127380, 0x10020867, // sub r1, r1, ra4
/* [0x00001338] */ 0x8d9cc23f, 0x10024862, // sub r1, r1, r0                ; mov r2, rb_wt_off
/* [0x00001340] */ 0x0f9c63c0, 0xd0020867, // asr r1, r1, 6
/* [0x00001348] */ 0x4d591bce, 0x120269e0, // sub.setf -, r5, rb_i_tmu      ; mul24 r0, r1, ra_wt_mul_l0
/* [0x00001350] */ 0x55653fce, 0x140453e1, // mov.ifz rb_base2, rb_base2_next ; mul24 r1, r1, ra_kmul_add
/* [0x00001358] */ 0x8d4e7076, 0x10029851, // sub r1, r0, r1                ; mov.ifz ra_y_y2, ra_y_y2_next
/* [0x00001360] */ 0x8d692bf6, 0x1002b9d8, // sub.setf -, r5, rb_lcount     ; mov.ifz ra_base, ra_base_next
/* [0x00001368] */ 0x8c9f8289, 0xd0024860, // add r1, r1, r2                ; mov r0, r1 << 8
/* [0x00001370] */ 0x8c5e7236, 0x1c024863, // add r1, r1, r0                ; mov r3, ra_blk_height
/* [0x00001378] */ 0xfffffe78, 0xf06809e7, // brr.anyn -, r:1b
/* [0x00001380] */ 0x4f65039f, 0x18024862, // asr r1, r1, ra_wt_den_p7      ; mul24 r2, r3, rb_pitch
/* [0x00001388] */ 0x925f23bf, 0x12020867, // min r1, r1, ra_pmax           ; mov -, vw_wait
/* [0x00001390] */ 0xf34003f3, 0xd2024c20, // max vpm, r1, 0                ; v8subs r0, ra_height, r3
/* [0x00001398] */ 0x956e7036, 0x10126431, // mov.setf ra_height, r0        ; mov vw_setup, ra_dma0
/* [0x000013a0] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x000013a8] */ 0x929dd0ff, 0x10024831, // min r0, r0, r3                ; mov vw_setup, rb_dma1
/* [0x000013b0] */ 0x8d7270f6, 0x10024872, // sub r1, r0, r3                ; mov vw_addr, ra_dest
/* [0x000013b8] */ 0x119d73c0, 0xd0020867, // shl r1, r1, i_shift23
/* [0x000013c0] */ 0xfffffe30, 0xf0f809e7, // brr -, r:1b
/* [0x000013c8] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x000013d0] */ 0x0c6e7c40, 0x100206e7, // add ra_dma0, ra_dma0, r1
/* [0x000013d8] */ 0x8c71ccbf, 0x10024731, // add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init
// ::mc_filter_y_p00
/* [0x000013e0] */ 0x959a0ff6, 0x10024020, // mov ra0, unif                 ; mov r0, elem_num
/* [0x000013e8] */ 0xf5567dad, 0x14124565, // mov ra_xshift, ra_xshift_next ; v8subs r5rep, r5, r5
/* [0x000013f0] */ 0x8c020c3f, 0x1402581a, // add r0, ra0.16b, r0           ; mov ra_base_next, unif
/* [0x000013f8] */ 0x93027176, 0x12225813, // max r0, r0, r5                ; mov ra_y_next, ra0.16a
/* [0x00001400] */ 0x9281a1f6, 0x10025810, // min r0, r0, rb_max_x          ; mov ra_width_height, unif
/* [0x00001408] */ 0x119c31c0, 0xd0220567, // shl ra_xshift_next, r0, 3
/* [0x00001410] */ 0x149dc1c0, 0xd0020827, // and r0, r0, -4
/* [0x00001418] */ 0x8d810bf6, 0x10025896, // sub r2, r5, rb_pitch          ; mov ra_wt_off_mul_l0, unif
/* [0x00001420] */ 0x149e7080, 0x10020867, // and r1, r0, r2
/* [0x00001428] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00001430] */ 0x8c827076, 0x1002581c, // add r0, r0, r1                ; mov ra_dest, unif
/* [0x00001438] */ 0x8c69cc3f, 0x100246b1, // add ra_base_next, ra_base_next, r0 ; mov vw_setup, rb_vpm_init
/* [0x00001440] */ 0x11400dc0, 0xd4020867, // shl r1, ra_width, v_x_shift
/* [0x00001448] */ 0x8d419e76, 0x12025760, // sub rb_dma1, rb_dma1_base, r1 ; mov r0, ra_height
/* [0x00001450] */ 0x8d5c31c6, 0xdc025460, // sub rb_i_tmu, r0, PREREAD     ; v8min r0, r0, ra_blk_height
/* [0x00001458] */ 0x919c71c0, 0xd0024812, // shl r0, r0, v_dma_h_shift     ; mov rb_lcount, r0
/* [0x00001460] */ 0x0c9e7040, 0x10020827, // add r0, r0, r1
/* [0x00001468] */ 0x1158edc0, 0xd4021327, // shl rb_wt_off, ra_wt_off_l0, DENOM + 7
/* [0x00001470] */ 0x918101f6, 0xd002581e, // shl r0, r0, v_dma_wh_shift    ; mov ra_link, unif
/* [0x00001478] */ 0x0c9db1c0, 0x100206e7, // add ra_dma0, r0, rb_dma0_base
// :1
/* [0x00001480] */ 0xcd511bee, 0x1a0269e5, // sub.setf -, r5, rb_i_tmu      ; v8adds r5rep, r5, ra_k1
/* [0x00001488] */ 0x804e7036, 0xa42099d1, // nop                           ; mov.ifz ra_y, ra_y_next       ; ldtmu0
/* [0x00001490] */ 0x8e5509bf, 0x12024823, // shr r0, r4, ra_xshift         ; mov r3, rb_pitch
/* [0x00001498] */ 0x13440dc0, 0xd40208a7, // max r2, ra_y, 0
/* [0x000014a0] */ 0x9269e5f6, 0x10029898, // min r2, r2, rb_max_y          ; mov.ifz ra_base, ra_base_next
/* [0x000014a8] */ 0x4c441dd3, 0xd4224462, // add ra_y, ra_y, 1             ; mul24 r2, r2, r3
/* [0x000014b0] */ 0x8c618c87, 0x10024e20, // add t0s, ra_base, r2          ; v8min r0, r0, rb_pmask
/* [0x000014b8] */ 0x4d592bc6, 0x120269e1, // sub.setf -, r5, rb_lcount     ; mul24 r1, r0, ra_wt_mul_l0
/* [0x000014c0] */ 0x915c83f6, 0xdc024863, // shl r1, r1, 8                 ; mov r3, ra_blk_height
/* [0x000014c8] */ 0xec40c3f3, 0x12024860, // add r1, r1, rb_wt_off         ; v8subs r0, ra_height, r3
/* [0x000014d0] */ 0xffffff90, 0xf06809e7, // brr.anyn -, r:1b
/* [0x000014d8] */ 0x0f9cf3c0, 0xd0020867, // asr r1, r1, DENOM + 8
/* [0x000014e0] */ 0x925f23bf, 0x12020867, // min r1, r1, ra_pmax           ; mov -, vw_wait
/* [0x000014e8] */ 0x5351039f, 0x18024c22, // max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch
/* [0x000014f0] */ 0x956e7036, 0x10126431, // mov.setf ra_height, r0 ; mov vw_setup, ra_dma0
/* [0x000014f8] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x00001500] */ 0x929dd0ff, 0x10024831, // min r0, r0, r3        ; mov vw_setup, rb_dma1
/* [0x00001508] */ 0x8d7270f6, 0x10024872, // sub r1, r0, r3        ; mov vw_addr, ra_dest
/* [0x00001510] */ 0x119d73c0, 0xd0020867, // shl r1, r1, i_shift23
/* [0x00001518] */ 0xffffff48, 0xf0f809e7, // brr -, r:1b
/* [0x00001520] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x00001528] */ 0x0c6e7c40, 0x100206e7, // add ra_dma0, ra_dma0, r1
/* [0x00001530] */ 0x8c71ccbf, 0x10024731, // add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init
// ::mc_filter_y_b00
/* [0x00001538] */ 0xfffff8f8, 0xf0f807a7, // brr ra_link, r:per_block_setup_8
/* [0x00001540] */ 0x959a0ff6, 0x10024023, // mov ra0, unif                 ; mov r3, elem_num
/* [0x00001548] */ 0xec9c3fd2, 0x100269e5, // add.setf -, rb_ef, rb_ef      ; v8subs r5rep, r2, r2
/* [0x00001550] */ 0x8c001cff, 0x14024800, // add r0, ra0.16b, r3           ; mov rb_xshift2, rb_xshift2_next
/* [0x00001558] */ 0x00000001, 0xe00208a7, // mov r2, 1
/* [0x00001560] */ 0x8c591eb6, 0x10025461, // add rb_i_tmu, rb_i_tmu, r2    ; mov r1, ra_wt_off_mul_l0
/* [0x00001568] */ 0xf158fded, 0xd4025325, // shl rb_wt_off, ra_wt_off_l0, DENOM + 8 ; v8subs r5quad, r5, r5
/* [0x00001570] */ 0x809f8009, 0xd000d9d6, // nop                           ; mov.ifnz ra_wt_off_mul_l0, r1 << 8
// :1
/* [0x00001578] */ 0x0d9d1bc0, 0xb00229e7, // sub.setf -, r5, rb_i_tmu      ; nop                           ; ldtmu1
/* [0x00001580] */ 0x8e4c09f6, 0xa0029851, // shr r1, r4, rb_xshift2        ; mov.ifz ra_y_y2, ra_y_y2_next ; ldtmu0
/* [0x00001588] */ 0x8e5509bf, 0x12024823, // shr r0, r4, ra_xshift         ; mov r3, rb_pitch
/* [0x00001590] */ 0x13440dc0, 0xd40208a7, // max r2, ra_y, 0
/* [0x00001598] */ 0x9269e5f6, 0x10029898, // min r2, r2, rb_max_y          ; mov.ifz ra_base, ra_base_next
/* [0x000015a0] */ 0x4c441dd3, 0xd4224462, // add ra_y, ra_y, 1             ; mul24 r2, r2, r3
/* [0x000015a8] */ 0x8c613cbf, 0x10028e0f, // add t0s, ra_base, r2          ; mov.ifz rb_base2, rb_base2_next
/* [0x000015b0] */ 0x13440dc0, 0xd20208a7, // max r2, ra_y2, 0
/* [0x000015b8] */ 0x129de5c0, 0x100208a7, // min r2, r2, rb_max_y
/* [0x000015c0] */ 0x4c441dd3, 0xd2124462, // add ra_y2, ra_y2, 1           ; mul24 r2, r2, r3
/* [0x000015c8] */ 0x8c5cfe86, 0x12024f20, // add t1s, rb_base2, r2         ; v8min r0, r0, ra_pmax
/* [0x000015d0] */ 0x545983c6, 0x12024860, // and r1, r1, rb_pmask          ; mul24 r0, r0, ra_wt_mul_l0
/* [0x000015d8] */ 0x4d492bce, 0x120269e1, // sub.setf -, r5, rb_lcount     ; mul24 r1, r1, ra_wt_mul_l1
/* [0x000015e0] */ 0xcc52706e, 0x1a024865, // add r1, r0, r1                ; v8adds r5rep, r5, ra_k1
/* [0x000015e8] */ 0x915c83f6, 0xdc024863, // shl r1, r1, 8                 ; mov r3, ra_blk_height
/* [0x000015f0] */ 0xec40c3f3, 0x12024860, // add r1, r1, rb_wt_off         ; v8subs r0, ra_height, r3
/* [0x000015f8] */ 0xffffff60, 0xf06809e7, // brr.anyn -, r:1b
/* [0x00001600] */ 0x0f9d03c0, 0xd0020867, // asr r1, r1, (DENOM + 9) - 32
/* [0x00001608] */ 0x925f23bf, 0x12020867, // min r1, r1, ra_pmax           ; mov -, vw_wait
/* [0x00001610] */ 0x5351039f, 0x18024c22, // max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch
/* [0x00001618] */ 0x956e7036, 0x10126431, // mov.setf ra_height, r0        ; mov vw_setup, ra_dma0
/* [0x00001620] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x00001628] */ 0x929dd0ff, 0x10024831, // min r0, r0, r3                ; mov vw_setup, rb_dma1
/* [0x00001630] */ 0x8d7270f6, 0x10024872, // sub r1, r0, r3                ; mov vw_addr, ra_dest
/* [0x00001638] */ 0x119d73c0, 0xd0020867, // shl r1, r1, i_shift23
/* [0x00001640] */ 0xffffff18, 0xf0f809e7, // brr -, r:1b
/* [0x00001648] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x00001650] */ 0x0c6e7c40, 0x100206e7, // add ra_dma0, ra_dma0, r1
/* [0x00001658] */ 0x8c71ccbf, 0x10024731, // add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init
// ::mc_setup_c10_q0
/* [0x00001660] */ 0x0000000c, 0xe80009e7, // mov  dst, srel(i)
// ::mc_setup_c10_qn
/* [0x00001668] */ 0x95801ff6, 0xd0025900, // mov tmurs, 1                  ; mov ra0, unif
/* [0x00001670] */ 0xaaaaff00, 0xe6020827, // mov r0, [0,2,0,2,0,2,0,2,1,3,1,3,1,3,1,3]
/* [0x00001678] */ 0x9181e1f6, 0xd00250d8, // shl rb_ef, r0, i_shift30      ; mov ra_base, unif
/* [0x00001680] */ 0x0d801dc0, 0xd0020827, // sub r0, unif, 1
/* [0x00001688] */ 0x119c21c0, 0xd00216a7, // shl rb_max_x, r0, v_x_shift
/* [0x00001690] */ 0x0d801dc0, 0xd00217a7, // sub rb_max_y, unif, 1
/* [0x00001698] */ 0xff800100, 0xe0020527, // mov ra_kff800100, 0xff800100
/* [0x000016a0] */ 0x0000ffff, 0xe0021627, // mov rb_pmask, v_pmask
/* [0x000016a8] */ 0x000803ff, 0xe00205e7, // mov ra_blk_height_pmax, ((1 << v_bit_depth) - 1) | (v_blk_height << 16)
/* [0x000016b0] */ 0x00010000, 0xe00217e7, // mov rb_fir_off_h, (FIR_OFFSET << (v_bit_depth - 8))
/* [0x000016b8] */ 0x4000000c, 0xe0020667, // mov ra_fir_off_val_wt_den_p7, (FIR_OFFSET << 16) | (DENOM + 15 - v_bit_depth)
/* [0x000016c0] */ 0x95803ff6, 0x10024754, // mov ra_ef, rb_ef              ; mov rb_xpitch, unif
/* [0x000016c8] */ 0x15827d80, 0x10021427, // mov rb_pitch, unif
/* [0x000016d0] */ 0xc0000000, 0xe0020867, // mov r1, vdw_setup_1(0)
/* [0x000016d8] */ 0x0c9d03c0, 0x10021667, // add rb_dma1_base, r1, rb_pitch
/* [0x000016e0] */ 0x14981f80, 0xd0020827, // and r0, 1, elem_num
/* [0x000016e8] */ 0x409c5007, 0xd00049e0, // nop                           ; mul24 r0, r0, 5
/* [0x000016f0] */ 0x0c9a7180, 0x10020827, // add r0, r0, elem_num
/* [0x000016f8] */ 0x0c9e7000, 0x100210a7, // add rb_elem_x, r0, r0
/* [0x00001700] */ 0x11002dc0, 0xd4020827, // shl r0, ra0.16b, v_x_shift
/* [0x00001708] */ 0x0c9c21c0, 0x10020827, // add r0, r0, rb_elem_x
/* [0x00001710] */ 0x930001f6, 0xd2225811, // max r0, r0, 0                 ; mov ra_y, ra0.16a
/* [0x00001718] */ 0x129da1c0, 0x10020827, // min r0, r0, rb_max_x
/* [0x00001720] */ 0x00000000, 0xe0224541, // mov ra_xshift_next, 0         ; mov rb_xshift2_next, 0
/* [0x00001728] */ 0x0d510dc0, 0x18020867, // sub r1, ra_k0, rb_pitch
/* [0x00001730] */ 0x149e7040, 0x10020867, // and r1, r0, r1
/* [0x00001738] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00001740] */ 0x8c827076, 0x10025800, // add r0, r0, r1                ; mov ra0, unif
/* [0x00001748] */ 0x0c627c00, 0x10020627, // add ra_base, ra_base, r0
/* [0x00001750] */ 0x159e6fc0, 0x100208a7, // mov r2, qpu_num
/* [0x00001758] */ 0x0f9c15c0, 0xd0020867, // asr r1, r2, 1
/* [0x00001760] */ 0x119c43c0, 0xd0020867, // shl r1, r1, 4
/* [0x00001768] */ 0x149c15c0, 0xd0020827, // and r0, r2, 1
/* [0x00001770] */ 0x159e7040, 0x10020827, // or  r0, r0, r1
/* [0x00001778] */ 0x00002900, 0xe0020867, // mov r1, vpm_setup(0, 2, h16p(0, 0))
/* [0x00001780] */ 0x0c9e7040, 0x10021727, // add r_vpm, r0, r1
/* [0x00001788] */ 0x80004002, 0xe0020867, // mov r1, vdw_setup_0(0, 0, dma_h16p(0,0,0))
/* [0x00001790] */ 0x119c61c0, 0xd0020827, // shl r0, r0, 6
/* [0x00001798] */ 0x0c9e7040, 0x100216e7, // add r_dma, r0, r1
/* [0x000017a0] */ 0x11002dc0, 0xd4020827, // shl r0, ra0.16b, v_x_shift
/* [0x000017a8] */ 0x8c0021f6, 0x12125811, // add r0, r0, rb_elem_x         ; mov ra_y2, ra0.16a
/* [0x000017b0] */ 0x938001f6, 0xd002480f, // max r0, r0, 0                 ; mov rb_base2, unif
/* [0x000017b8] */ 0x129da1c0, 0x10020827, // min r0, r0, rb_max_x
/* [0x000017c0] */ 0x0d510dc0, 0x18020867, // sub r1, ra_k0, rb_pitch
/* [0x000017c8] */ 0x949c307f, 0xd0024863, // and r1, r0, r1                ; mov r3, PREREAD
/* [0x000017d0] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x000017d8] */ 0x8c467076, 0x12024822, // add r0, r0, r1                ; mov r2, ra_y2
/* [0x000017e0] */ 0x8c44fe36, 0x140253e0, // add rb_base2, rb_base2, r0    ; mov r0, ra_y
// :1
/* [0x000017e8] */ 0x0d9c17c0, 0xd00228e7, // sub.setf r3, r3, 1
/* [0x000017f0] */ 0x139c01c0, 0xd0020867, // max r1, r0, 0
/* [0x000017f8] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_max_y
/* [0x00001800] */ 0x4c51018f, 0x1a024821, // add r0, r0, ra_k1             ; mul24 r1, r1, rb_pitch
/* [0x00001808] */ 0x8c627c40, 0x10225e11, // add t0s, ra_base, r1          ; mov ra_y, r0
/* [0x00001810] */ 0x139c05c0, 0xd0020867, // max r1, r2, 0
/* [0x00001818] */ 0xffffffb0, 0xf03809e7, // brr.anynz -, r:1b
/* [0x00001820] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_max_y
/* [0x00001828] */ 0x4c51058f, 0x1a0248a1, // add r2, r2, ra_k1             ; mul24 r1, r1, rb_pitch
/* [0x00001830] */ 0x8c9cfe52, 0x10125f11, // add t1s, rb_base2, r1         ; mov ra_y2, r2
/* [0x00001838] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00001840] */ 0x00000000, 0xe0024104, // mov ra4, 0                    ; mov rb4, 0
/* [0x00001848] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00001850] */ 0x00000000, 0xe0024145, // mov ra5, 0                    ; mov rb5, 0
/* [0x00001858] */ 0x00000000, 0xe0024186, // mov ra6, 0                    ; mov rb6, 0
/* [0x00001860] */ 0x00000000, 0xe00241c7, // mov ra7, 0                    ; mov rb7, 0
// ::mc_filter_c10_p
/* [0x00001868] */ 0x9581cff6, 0x10025c42, // mov vw_setup, rb_vpm_init     ; mov ra2, unif
/* [0x00001870] */ 0x8c803ff6, 0x100269e3, // add.setf -, rb_ef, rb_ef      ; mov r3, unif
/* [0x00001878] */ 0xf1082dc0, 0xd4024825, // shl r0, ra2.16b, v_x_shift    ; v8subs r5rep, r0, r0
/* [0x00001880] */ 0x8c8021f6, 0x10025810, // add r0, r0, rb_elem_x         ; mov ra_width_height, unif
/* [0x00001888] */ 0x8d810bf6, 0x10025840, // sub r1, r5, rb_pitch          ; mov ra0, unif
/* [0x00001890] */ 0x93567176, 0x14024800, // max r0, r0, r5                ; mov vrx_xshift, vrx_xshift_next
/* [0x00001898] */ 0x9209a1f6, 0x12225813, // min r0, r0, rb_max_x          ; mov vra_y_next, ra2.16a
/* [0x000018a0] */ 0x54404077, 0xd4024862, // and r1, r0, r1                ; mul24 r2, ra_width, v_x_mul
/* [0x000018a8] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x000018b0] */ 0x8c827076, 0x10025803, // add r0, r0, r1                ; mov ra3, unif
/* [0x000018b8] */ 0x8c427636, 0x120246a1, // add vrx_base_next, r3, r0     ; mov r1, ra_height
/* [0x000018c0] */ 0x8d819eb6, 0x10025756, // sub rb_dma1, rb_dma1_base, r2 ; mov ra_wt_off_mul_l0, unif
/* [0x000018c8] */ 0x8c5dc3ce, 0xdc025461, // add rb_i_tmu, r1, (3-4) - PREREAD ; v8min r1, r1, ra_blk_height
/* [0x000018d0] */ 0x8c81f3f6, 0xd0039496, // add rb_lcount, r1, (3-4)      ; mov.ifc ra_wt_off_mul_l0, unif
/* [0x000018d8] */ 0x918083f6, 0xd002581c, // shl r0, r1, v_dma_h_shift     ; mov ra_dest, unif
/* [0x000018e0] */ 0x8c6670b6, 0x14024822, // add r0, r0, r2                ; mov r2, ra_fir_off_val
/* [0x000018e8] */ 0x910cf1f6, 0xdc02480a, // shl r0, r0, v_dma_wh_shift    ; mov rb10, ra3.8c
/* [0x000018f0] */ 0x8c59b1f6, 0x140246e1, // add ra_dma0, r0, rb_dma0_base ; mov r1, ra_wt_off_l0
/* [0x000018f8] */ 0x5158a3d6, 0xd2024860, // shl r1, r1, i_wt_den_p5       ; mul24 r0, r2, ra_wt_mul_l0
/* [0x00001900] */ 0x8d667236, 0x14025320, // sub rb_wt_off, r1, r0         ; mov r0, ra_kmul_add
/* [0x00001908] */ 0x8c59cc3f, 0xd21245a5, // add ra_wt_mul_l0, ra_wt_mul_l0, r0 ; mov r5rep, -4
/* [0x00001910] */ 0x950e0dbf, 0x1e0252de, // mov rb11, ra3.8d              ; mov ra_link, unif
// :1
/* [0x00001918] */ 0x8d151bf6, 0xa00269c4, // sub.setf -, r5, rb_i_tmu      ; mov rb4, ra5                  ; ldtmu0
/* [0x00001920] */ 0x8e4c09f6, 0x140288a3, // shr r2, r4, vrx_xshift        ; mov.ifz  r3, vra_y_next
/* [0x00001928] */ 0x8e4505f6, 0xd402c863, // shr r1, r2, v_v_shift         ; mov.ifnz r3, vra_y
/* [0x00001930] */ 0x8c683ff6, 0x1002b9d8, // add.setf -, rb_ef, rb_ef      ; mov.ifz  vra_base, vrx_base_next
/* [0x00001938] */ 0x8c531789, 0xda224460, // add vra_y, r3, ra_k1          ; mov      r0, r1 << 15
/* [0x00001940] */ 0x9353f792, 0xd803c8e1, // max r3, r3, ra_k0             ; mov.ifnc r1, r2 << 1
/* [0x00001948] */ 0x929de7d2, 0x1003c8e0, // min r3, r3, rb_max_y          ; mov.ifnc r0, r2
/* [0x00001950] */ 0x545d039f, 0x12024863, // and r1, r1, ra_pmax           ; mul24 r3, r3, rb_pitch
/* [0x00001958] */ 0x8c618cc7, 0x10024e20, // add vr_txs, vra_base, r3      ; v8min r0, r0, rb_pmask
/* [0x00001960] */ 0x4c001bf0, 0xd8025963, // add r5rep, r5, 1              ; mul24      r3, ra0.8a,       r0
/* [0x00001968] */ 0x4d01fef1, 0x1e0248a3, // sub r2, rb_fir_off_h, r3      ; mul24      r3, ra0.8d,       r1
/* [0x00001970] */ 0x4d03e4f0, 0xda0248a3, // sub r2, r2, r3                ; mul24      r3, ra0.8b << 2,  r0 << 2  @ "mul_used", 0
/* [0x00001978] */ 0x40034031, 0xda0109e3, // nop                           ; mul24.ifn  r3, ra0.8b << 12, r1 << 12 @ "mul_used", 0
/* [0x00001980] */ 0x4c03c4f0, 0xdc0248a3, // add r2, r2, r3                ; mul24      r3, ra0.8c << 4,  r0 << 4  @ "mul_used", 0
/* [0x00001988] */ 0x4c032b71, 0xdc0329e3, // add.setf -, r5, r5            ; mul24.ifn  r3, ra0.8c << 14, r1 << 14 @ "mul_used", 0
/* [0x00001990] */ 0xffffff68, 0xf06809e7, // brr.anyn -, r:1b
/* [0x00001998] */ 0x4c1ca4f7, 0x100248a0, // add r2, r2, r3                ; mul24 r0, ra7, rb10
/* [0x000019a0] */ 0x550c6ffe, 0x1a024161, // mov ra5, rb6                  ; mul24 r1, rb6, ra3.8b
/* [0x000019a8] */ 0x8f1c25f6, 0xd00241c6, // asr ra7, r2, v_bit_depth - 8  ; mov rb6, ra7
/* [0x000019b0] */ 0x4c0c423e, 0x18024860, // add r1, r1, r0                ; mul24 r0, rb4, ra3.8a
/* [0x000019b8] */ 0x4d1cb237, 0x10024860, // sub r1, r1, r0                ; mul24 r0, ra7, rb11
/* [0x000019c0] */ 0x0d9e7200, 0x10020867, // sub r1, r1, r0
/* [0x000019c8] */ 0x8f5c63f6, 0xdc024863, // asr r1, r1, 6                 ; mov r3, ra_blk_height
/* [0x000019d0] */ 0x4d592bce, 0x120269e0, // sub.setf -, r5, rb_lcount     ; mul24 r0, r1, ra_wt_mul_l0
/* [0x000019d8] */ 0x4c64c1ce, 0x14024821, // add r0, r0, rb_wt_off         ; mul24 r1, r1, ra_kmul_add
/* [0x000019e0] */ 0xed427073, 0x12024860, // sub r1, r0, r1                ; v8subs r0, ra_height, r3
/* [0x000019e8] */ 0xffffff10, 0xf06809e7, // brr.anyn -, r:1b
/* [0x000019f0] */ 0x0f9cb3c0, 0xd0020867, // asr r1, r1, i_wt_den_p6
/* [0x000019f8] */ 0x925f23bf, 0x12020867, // min r1, r1, ra_pmax           ; mov -, vw_wait
/* [0x00001a00] */ 0x5351039f, 0x18024c22, // max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch
/* [0x00001a08] */ 0x956e7036, 0x10126431, // mov.setf ra_height, r0        ; mov vw_setup, ra_dma0
/* [0x00001a10] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x00001a18] */ 0x929dd0ff, 0x10024831, // min r0, r0, r3                ; mov vw_setup, rb_dma1
/* [0x00001a20] */ 0x8d7270f6, 0x10024872, // sub r1, r0, r3                ; mov vw_addr, ra_dest
/* [0x00001a28] */ 0x119d73c0, 0xd0020867, // shl r1, r1, i_shift23
/* [0x00001a30] */ 0xfffffec8, 0xf0f809e7, // brr -, r:1b
/* [0x00001a38] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x00001a40] */ 0x0c6e7c40, 0x100206e7, // add ra_dma0, ra_dma0, r1
/* [0x00001a48] */ 0x8c71ccbf, 0x10024731, // add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init
// ::mc_filter_c10_p_l1
/* [0x00001a50] */ 0x9581cff6, 0x10025c42, // mov vw_setup, rb_vpm_init     ; mov ra2, unif
/* [0x00001a58] */ 0x8c803ff6, 0x100269e3, // add.setf -, rb_ef, rb_ef      ; mov r3, unif
/* [0x00001a60] */ 0xf1082dc0, 0xd4024825, // shl r0, ra2.16b, v_x_shift    ; v8subs r5rep, r0, r0
/* [0x00001a68] */ 0x8c8021f6, 0x10025810, // add r0, r0, rb_elem_x         ; mov ra_width_height, unif
/* [0x00001a70] */ 0x8d810bf6, 0x10025840, // sub r1, r5, rb_pitch          ; mov ra0, unif
/* [0x00001a78] */ 0x939c117f, 0x10125815, // max r0, r0, r5                ; mov vrx_xshift, vrx_xshift_next
/* [0x00001a80] */ 0x9209a1f6, 0x12125813, // min r0, r0, rb_max_x          ; mov vra_y_next, ra2.16a
/* [0x00001a88] */ 0x54404077, 0xd4024862, // and r1, r0, r1                ; mul24 r2, ra_width, v_x_mul
/* [0x00001a90] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00001a98] */ 0x8c827076, 0x10025803, // add r0, r0, r1                ; mov ra3, unif
/* [0x00001aa0] */ 0x8c427636, 0x120254e1, // add vrx_base_next, r3, r0     ; mov r1, ra_height
/* [0x00001aa8] */ 0x8d819eb6, 0x10025756, // sub rb_dma1, rb_dma1_base, r2 ; mov ra_wt_off_mul_l0, unif
/* [0x00001ab0] */ 0x8c5dc3ce, 0xdc025461, // add rb_i_tmu, r1, (3-4) - PREREAD ; v8min r1, r1, ra_blk_height
/* [0x00001ab8] */ 0x8c81f3f6, 0xd0039496, // add rb_lcount, r1, (3-4)      ; mov.ifc ra_wt_off_mul_l0, unif
/* [0x00001ac0] */ 0x918083f6, 0xd002581c, // shl r0, r1, v_dma_h_shift     ; mov ra_dest, unif
/* [0x00001ac8] */ 0x8c6670b6, 0x14024822, // add r0, r0, r2                ; mov r2, ra_fir_off_val
/* [0x00001ad0] */ 0x910cf1f6, 0xdc02480a, // shl r0, r0, v_dma_wh_shift    ; mov rb10, ra3.8c
/* [0x00001ad8] */ 0x8c59b1f6, 0x140246e1, // add ra_dma0, r0, rb_dma0_base ; mov r1, ra_wt_off_l0
/* [0x00001ae0] */ 0x5158a3d6, 0xd2024860, // shl r1, r1, i_wt_den_p5       ; mul24 r0, r2, ra_wt_mul_l0
/* [0x00001ae8] */ 0x8d667236, 0x14025320, // sub rb_wt_off, r1, r0         ; mov r0, ra_kmul_add
/* [0x00001af0] */ 0x8c59cc3f, 0xd21245a5, // add ra_wt_mul_l0, ra_wt_mul_l0, r0 ; mov r5rep, -4
/* [0x00001af8] */ 0x950e0dbf, 0x1e0252de, // mov rb11, ra3.8d              ; mov ra_link, unif
// :1
/* [0x00001b00] */ 0x8d151bf6, 0xb00269c4, // sub.setf -, r5, rb_i_tmu      ; mov rb4, ra5                  ; ldtmu1
/* [0x00001b08] */ 0x8e5539bf, 0x1202888f, // shr r2, r4, vrx_xshift        ; mov.ifz  vra_base, vrx_base_next
/* [0x00001b10] */ 0x8e4505f6, 0xd202c863, // shr r1, r2, v_v_shift         ; mov.ifnz r3, vra_y
/* [0x00001b18] */ 0x8c4c3ff6, 0x1202a9e3, // add.setf -, rb_ef, rb_ef      ; mov.ifz  r3, vra_y_next
/* [0x00001b20] */ 0x8c531789, 0xda124460, // add vra_y, r3, ra_k1          ; mov      r0, r1 << 15
/* [0x00001b28] */ 0x9353f792, 0xd803c8e1, // max r3, r3, ra_k0             ; mov.ifnc r1, r2 << 1
/* [0x00001b30] */ 0x929de7d2, 0x1003c8e0, // min r3, r3, rb_max_y          ; mov.ifnc r0, r2
/* [0x00001b38] */ 0x545d039f, 0x12024863, // and r1, r1, ra_pmax           ; mul24 r3, r3, rb_pitch
/* [0x00001b40] */ 0x8c5cfec6, 0x12024f20, // add vr_txs, vra_base, r3      ; v8min r0, r0, ra_pmax
/* [0x00001b48] */ 0x4c001bf0, 0xd8025963, // add r5rep, r5, 1              ; mul24      r3, ra0.8a,       r0
/* [0x00001b50] */ 0x4d01fef1, 0x1e0248a3, // sub r2, rb_fir_off_h, r3      ; mul24      r3, ra0.8d,       r1
/* [0x00001b58] */ 0x4d03e4f0, 0xda0248a3, // sub r2, r2, r3                ; mul24      r3, ra0.8b << 2,  r0 << 2  @ "mul_used", 0
/* [0x00001b60] */ 0x40034031, 0xda0109e3, // nop                           ; mul24.ifn  r3, ra0.8b << 12, r1 << 12 @ "mul_used", 0
/* [0x00001b68] */ 0x4c03c4f0, 0xdc0248a3, // add r2, r2, r3                ; mul24      r3, ra0.8c << 4,  r0 << 4  @ "mul_used", 0
/* [0x00001b70] */ 0x4c032b71, 0xdc0329e3, // add.setf -, r5, r5            ; mul24.ifn  r3, ra0.8c << 14, r1 << 14 @ "mul_used", 0
/* [0x00001b78] */ 0xffffff68, 0xf06809e7, // brr.anyn -, r:1b
/* [0x00001b80] */ 0x4c1ca4f7, 0x100248a0, // add r2, r2, r3                ; mul24 r0, ra7, rb10
/* [0x00001b88] */ 0x550c6ffe, 0x1a024161, // mov ra5, rb6                  ; mul24 r1, rb6, ra3.8b
/* [0x00001b90] */ 0x8f1c25f6, 0xd00241c6, // asr ra7, r2, v_bit_depth - 8  ; mov rb6, ra7
/* [0x00001b98] */ 0x4c0c423e, 0x18024860, // add r1, r1, r0                ; mul24 r0, rb4, ra3.8a
/* [0x00001ba0] */ 0x4d1cb237, 0x10024860, // sub r1, r1, r0                ; mul24 r0, ra7, rb11
/* [0x00001ba8] */ 0x0d9e7200, 0x10020867, // sub r1, r1, r0
/* [0x00001bb0] */ 0x8f5c63f6, 0xdc024863, // asr r1, r1, 6                 ; mov r3, ra_blk_height
/* [0x00001bb8] */ 0x4d592bce, 0x120269e0, // sub.setf -, r5, rb_lcount     ; mul24 r0, r1, ra_wt_mul_l0
/* [0x00001bc0] */ 0x4c64c1ce, 0x14024821, // add r0, r0, rb_wt_off         ; mul24 r1, r1, ra_kmul_add
/* [0x00001bc8] */ 0xed427073, 0x12024860, // sub r1, r0, r1                ; v8subs r0, ra_height, r3
/* [0x00001bd0] */ 0xffffff10, 0xf06809e7, // brr.anyn -, r:1b
/* [0x00001bd8] */ 0x0f9cb3c0, 0xd0020867, // asr r1, r1, i_wt_den_p6
/* [0x00001be0] */ 0x925f23bf, 0x12020867, // min r1, r1, ra_pmax           ; mov -, vw_wait
/* [0x00001be8] */ 0x5351039f, 0x18024c22, // max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch
/* [0x00001bf0] */ 0x956e7036, 0x10126431, // mov.setf ra_height, r0        ; mov vw_setup, ra_dma0
/* [0x00001bf8] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x00001c00] */ 0x929dd0ff, 0x10024831, // min r0, r0, r3                ; mov vw_setup, rb_dma1
/* [0x00001c08] */ 0x8d7270f6, 0x10024872, // sub r1, r0, r3                ; mov vw_addr, ra_dest
/* [0x00001c10] */ 0x119d73c0, 0xd0020867, // shl r1, r1, i_shift23
/* [0x00001c18] */ 0xfffffec8, 0xf0f809e7, // brr -, r:1b
/* [0x00001c20] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x00001c28] */ 0x0c6e7c40, 0x100206e7, // add ra_dma0, ra_dma0, r1
/* [0x00001c30] */ 0x8c71ccbf, 0x10024731, // add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init
// ::mc_filter_c10_b
/* [0x00001c38] */ 0x9581cff6, 0x10025c42, // mov vw_setup, rb_vpm_init     ; mov ra2, unif
/* [0x00001c40] */ 0x8c803ff6, 0x100269e3, // add.setf -, rb_ef, rb_ef      ; mov r3, unif
/* [0x00001c48] */ 0xf1082dc9, 0xd4024825, // shl r0, ra2.16b, v_x_shift    ; v8subs r5rep, r1, r1
/* [0x00001c50] */ 0x8c0821f6, 0x12225813, // add r0, r0, rb_elem_x         ; mov ra_y_next, ra2.16a
/* [0x00001c58] */ 0x8d810bf6, 0x10025850, // sub r1, r5, rb_pitch          ; mov ra_width_height, unif
/* [0x00001c60] */ 0x93567176, 0x14125815, // max r0, r0, r5                ; mov ra_xshift, ra_xshift_next
/* [0x00001c68] */ 0x9281a1f6, 0x10025800, // min r0, r0, rb_max_x          ; mov ra0, unif
/* [0x00001c70] */ 0x9481c1f6, 0xd0025802, // and r0, r0, -4                ; mov ra2, unif
/* [0x00001c78] */ 0x54404077, 0xd4024862, // and r1, r0, r1                ; mul24 r2, ra_width, v_x_mul
/* [0x00001c80] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00001c88] */ 0x8c427076, 0x12024821, // add r0, r0, r1                ; mov r1, ra_height
/* [0x00001c90] */ 0x8c9c163f, 0x10024680, // add ra_base_next, r3, r0      ; mov rb_xshift2, rb_xshift2_next
/* [0x00001c98] */ 0x8d819eb6, 0x10025756, // sub rb_dma1, rb_dma1_base, r2 ; mov ra_wt_off_mul_l0, unif
/* [0x00001ca0] */ 0x8c5dc3ce, 0xdc025461, // add rb_i_tmu, r1, (3-4) - PREREAD ; v8min r1, r1, ra_blk_height
/* [0x00001ca8] */ 0x8c59f3f6, 0xd4139496, // add rb_lcount, r1, (3-4)      ; mov.ifc ra_wt_mul_l0, ra_wt_off_l0
/* [0x00001cb0] */ 0x918083f6, 0xd0025803, // shl r0, r1, v_dma_h_shift     ; mov ra3, unif
/* [0x00001cb8] */ 0x8c8270b6, 0x10024823, // add r0, r0, r2                ; mov r3, unif
/* [0x00001cc0] */ 0x910cf1f6, 0xd2125813, // shl r0, r0, v_dma_wh_shift    ; mov ra_y2_next, ra3.16a
/* [0x00001cc8] */ 0x8c0db1f6, 0x140246e0, // add ra_dma0, r0, rb_dma0_base ; mov r0, ra3.16b
/* [0x00001cd0] */ 0x918021f6, 0xd0025801, // shl r0, r0, v_x_shift         ; mov ra1, unif
/* [0x00001cd8] */ 0x8c8021f6, 0x10025803, // add r0, r0, rb_elem_x         ; mov ra3, unif
/* [0x00001ce0] */ 0x8d810bf6, 0x10025852, // sub r1, r5, rb_pitch          ; mov ra_wt_off_mul_l1, unif
/* [0x00001ce8] */ 0x939de17f, 0x10025809, // max r0, r0, r5                ; mov ra9, rb_max_y
/* [0x00001cf0] */ 0x9265a1f6, 0x14024822, // min r0, r0, rb_max_x          ; mov r2, ra_kmul_add
/* [0x00001cf8] */ 0x9481c1f6, 0xd0039812, // and r0, r0, -4                ; mov.ifc ra_wt_off_mul_l1, unif
/* [0x00001d00] */ 0x949dc07f, 0xd0024865, // and r1, r0, r1                ; mov r5rep, -4
/* [0x00001d08] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00001d10] */ 0x8c827076, 0x1002581c, // add r0, r0, r1                ; mov ra_dest, unif
/* [0x00001d18] */ 0x8c667636, 0x140254e0, // add rb_base2_next, r3, r0     ; mov r0, ra_fir_off_val
/* [0x00001d20] */ 0x4c5a7c86, 0x121245a1, // add ra_wt_mul_l0, ra_wt_mul_l0, r2 ; mul24 r1, r0, ra_wt_mul_l0
/* [0x00001d28] */ 0x4c4a7c86, 0x121244a0, // add ra_wt_mul_l1, ra_wt_mul_l1, r2 ; mul24 r0, r0, ra_wt_mul_l1
/* [0x00001d30] */ 0x8c4a7076, 0x14024821, // add r0, r0, r1                ; mov r1, ra_wt_off_l1
/* [0x00001d38] */ 0x910cb3f6, 0xde02484b, // shl r1, r1, i_wt_den_p6       ; mov rb11, ra3.8d
/* [0x00001d40] */ 0x8d827236, 0x1002531e, // sub rb_wt_off, r1, r0         ; mov ra_link, unif
/* [0x00001d48] */ 0x95080ff6, 0x1e024287, // mov ra10, rb_xshift2          ; mov rb7,  ra2.8d
// :1
/* [0x00001d50] */ 0x0d9d1bc0, 0xa00229e7, // sub.setf -, r5, rb_i_tmu      ; nop                           ; ldtmu0
/* [0x00001d58] */ 0x8e5539bf, 0x1202888f, // shr r2, r4, ra_xshift         ; mov.ifz rb_base2, rb_base2_next
/* [0x00001d60] */ 0x8e4d05f6, 0xd0029851, // shr r1, r2, v_v_shift         ; mov.ifz ra_y_y2, ra_y_y2_next
/* [0x00001d68] */ 0x8c683ff6, 0x1002b9d8, // add.setf -, rb_ef, rb_ef      ; mov.ifz ra_base, ra_base_next
/* [0x00001d70] */ 0x8c441fb6, 0xd4224463, // add ra_y, 1, ra_y             ; mov r3, ra_y
/* [0x00001d78] */ 0x93531789, 0xd80248e0, // max r3, r3, ra_k0             ; mov      r0, r1 << 15
/* [0x00001d80] */ 0x9227f792, 0xd003c8e1, // min r3, r3, ra9               ; mov.ifnc r1, r2 << 1
/* [0x00001d88] */ 0x559d049f, 0x100e4823, // mov.ifnc r0, r2               ; mul24 r3, r3, rb_pitch
/* [0x00001d90] */ 0x8c618cc7, 0x10024e20, // add t0s, ra_base, r3          ; v8min r0, r0, rb_pmask
/* [0x00001d98] */ 0x540183f0, 0x18024862, // and r1, r1, rb_pmask          ; mul24      r2, ra0.8a,       r0
/* [0x00001da0] */ 0x4d01feb1, 0x1e0248a3, // sub r2, rb_fir_off_h, r2      ; mul24      r3, ra0.8d,       r1
/* [0x00001da8] */ 0x4d03e4f0, 0xda0248a3, // sub r2, r2, r3                ; mul24      r3, ra0.8b << 2,  r0 << 2  @ "mul_used", 0
/* [0x00001db0] */ 0x40034031, 0xda0109e3, // nop                           ; mul24.ifn  r3, ra0.8b << 12, r1 << 12 @ "mul_used", 0
/* [0x00001db8] */ 0x4c03c4f0, 0xdc0248a3, // add r2, r2, r3                ; mul24      r3, ra0.8c << 4,  r0 << 4  @ "mul_used", 0
/* [0x00001dc0] */ 0x40032031, 0xdc0109e3, // nop                           ; mul24.ifn  r3, ra0.8c << 14, r1 << 14 @ "mul_used", 0
/* [0x00001dc8] */ 0x4c0854fe, 0xb8025804, // add r0, r2, r3                ; mul24 ra4, rb5, ra2.8a        ; ldtmu1
/* [0x00001dd0] */ 0x8e2869bf, 0x10024885, // shr r2, r4, ra10              ; mov rb5, rb6
/* [0x00001dd8] */ 0x8e4505f6, 0xd2024863, // shr r1, r2, v_v_shift         ; mov r3, ra_y2
/* [0x00001de0] */ 0x8e1c21f6, 0xd00241c6, // shr ra7, r0, v_bit_depth - 8  ; mov rb6, ra7
/* [0x00001de8] */ 0x8c531789, 0xda124460, // add ra_y2, r3, ra_k1          ; mov      r0, r1 << 15
/* [0x00001df0] */ 0x9353f792, 0xd803c8e1, // max r3, r3, ra_k0             ; mov.ifnc r1, r2 << 1
/* [0x00001df8] */ 0x925de7ce, 0x120248e1, // min r3, r3, rb_max_y          ; v8min r1, r1, ra_pmax
/* [0x00001e00] */ 0x559d049f, 0x100e4823, // mov.ifnc r0, r2               ; mul24 r3, r3, rb_pitch
/* [0x00001e08] */ 0x8c5cfec6, 0x12024f20, // add t1s, rb_base2, r3         ; v8min r0, r0, ra_pmax
/* [0x00001e10] */ 0x4c041bf0, 0xd8025962, // add r5rep, r5, 1              ; mul24      r2, ra1.8a,       r0
/* [0x00001e18] */ 0x4d05feb1, 0x1e0248a3, // sub r2, rb_fir_off_h, r2      ; mul24      r3, ra1.8d,       r1
/* [0x00001e20] */ 0x4d07e4f0, 0xda0248a3, // sub r2, r2, r3                ; mul24      r3, ra1.8b << 2,  r0 << 2  @ "mul_used", 0
/* [0x00001e28] */ 0x40074031, 0xda0109e3, // nop                           ; mul24.ifn  r3, ra1.8b << 12, r1 << 12 @ "mul_used", 0
/* [0x00001e30] */ 0x4c07c6b0, 0xdc0248a3, // add r2, r3, r2                ; mul24      r3, ra1.8c << 4,  r0 << 4  @ "mul_used", 0
/* [0x00001e38] */ 0x4c072b71, 0xdc0329e3, // add.setf -, r5, r5            ; mul24.ifn  r3, ra1.8c << 14, r1 << 14 @ "mul_used", 0
/* [0x00001e40] */ 0xfffffef0, 0xf06809e7, // brr.anyn -, r:1b
/* [0x00001e48] */ 0x4c0c94fe, 0x180248a0, // add r2, r2, r3                ; mul24 r0, rb9,  ra3.8a
/* [0x00001e50] */ 0x550caffe, 0x1a025261, // mov rb9, rb10                 ; mul24 r1, rb10, ra3.8b
/* [0x00001e58] */ 0x8e2c25f6, 0xd00242ca, // shr ra11, r2, v_bit_depth - 8 ; mov rb10, ra11
/* [0x00001e60] */ 0x4d08523e, 0x1a0248a1, // sub r2, r1, r0                ; mul24 r1, rb5,  ra2.8b
/* [0x00001e68] */ 0x8d112bf6, 0x100269e0, // sub.setf -, r5, rb_lcount     ; mov r0, ra4
/* [0x00001e70] */ 0x4d08623e, 0x1c024860, // sub r1, r1, r0                ; mul24 r0, rb6,  ra2.8c
/* [0x00001e78] */ 0x4c1c7237, 0x10024860, // add r1, r1, r0                ; mul24 r0, ra7,  rb7
/* [0x00001e80] */ 0x4d0ca23e, 0x1c024860, // sub r1, r1, r0                ; mul24 r0, rb10, ra3.8c
/* [0x00001e88] */ 0x4c2cb437, 0x100248a0, // add r2, r2, r0                ; mul24 r0, ra11, rb11
/* [0x00001e90] */ 0x0d9e7400, 0x100208a7, // sub r2, r2, r0
/* [0x00001e98] */ 0x0e9c63c0, 0xd0020867, // shr r1, r1, 6
/* [0x00001ea0] */ 0x4e5865ce, 0xd20248a0, // shr r2, r2, 6                 ; mul24 r0, r1, ra_wt_mul_l0
/* [0x00001ea8] */ 0x4c4a7456, 0x120248a1, // add r2, r2, r1                ; mul24 r1, r2, ra_wt_mul_l1
/* [0x00001eb0] */ 0x4c667216, 0x14024862, // add r1, r1, r0                ; mul24 r2, r2, ra_kmul_add
/* [0x00001eb8] */ 0x8d5e72b6, 0x1c024863, // sub r1, r1, r2                ; mov r3, ra_blk_height
/* [0x00001ec0] */ 0xec40c3f3, 0x12024860, // add r1, r1, rb_wt_off         ; v8subs r0, ra_height, r3
/* [0x00001ec8] */ 0xfffffe68, 0xf06809e7, // brr.anyn -, r:1b
/* [0x00001ed0] */ 0x0f667380, 0x18020867, // asr r1, r1, ra_wt_den_p7
/* [0x00001ed8] */ 0x925f23bf, 0x12020867, // min r1, r1, ra_pmax           ; mov -, vw_wait
/* [0x00001ee0] */ 0x5351039f, 0x18024c22, // max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch
/* [0x00001ee8] */ 0x956e7036, 0x10126431, // mov.setf ra_height, r0        ; mov vw_setup, ra_dma0
/* [0x00001ef0] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x00001ef8] */ 0x929dd0ff, 0x10024831, // min r0, r0, r3                ; mov vw_setup, rb_dma1
/* [0x00001f00] */ 0x8d7270f6, 0x10024872, // sub r1, r0, r3                ; mov vw_addr, ra_dest
/* [0x00001f08] */ 0x119d73c0, 0xd0020867, // shl r1, r1, i_shift23
/* [0x00001f10] */ 0xfffffe20, 0xf0f809e7, // brr -, r:1b
/* [0x00001f18] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x00001f20] */ 0x0c6e7c40, 0x100206e7, // add ra_dma0, ra_dma0, r1
/* [0x00001f28] */ 0x8c71ccbf, 0x10024731, // add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init
// ::mc_sync10_q0
/* [0x00001f30] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00001f38] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00001f40] */ 0x00000010, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00001f48] */ 0x00000010, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00001f50] */ 0x00000010, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00001f58] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00001f60] */ 0x0000001c, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00001f68] */ 0x00000001, 0xe80009e7, // mov  dst, srel(i)
/* [0x00001f70] */ 0x0000000d, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync10_q1
/* [0x00001f78] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00001f80] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00001f88] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00001f90] */ 0x00000000, 0xe80009e7, // mov  dst, srel(i)
/* [0x00001f98] */ 0x00000011, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00001fa0] */ 0x00000002, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync10_q2
/* [0x00001fa8] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00001fb0] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00001fb8] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00001fc0] */ 0x00000000, 0xe80009e7, // mov  dst, srel(i)
/* [0x00001fc8] */ 0x00000012, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00001fd0] */ 0x00000003, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync10_q3
/* [0x00001fd8] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00001fe0] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00001fe8] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00001ff0] */ 0x00000000, 0xe80009e7, // mov  dst, srel(i)
/* [0x00001ff8] */ 0x00000013, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00002000] */ 0x009e7000, 0x100009e7, // nop
// ::mc_sync10_q4
/* [0x00002008] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00002010] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00002018] */ 0x00000014, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00002020] */ 0x00000014, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00002028] */ 0x00000014, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00002030] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00002038] */ 0x0000001d, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00002040] */ 0x00000005, 0xe80009e7, // mov  dst, srel(i)
/* [0x00002048] */ 0x0000000e, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync10_q5
/* [0x00002050] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00002058] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00002060] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00002068] */ 0x00000004, 0xe80009e7, // mov  dst, srel(i)
/* [0x00002070] */ 0x00000015, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00002078] */ 0x00000006, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync10_q6
/* [0x00002080] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00002088] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00002090] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00002098] */ 0x00000004, 0xe80009e7, // mov  dst, srel(i)
/* [0x000020a0] */ 0x00000016, 0xe80009e7, // mov  dst, sacq(i)
/* [0x000020a8] */ 0x00000007, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync10_q7
/* [0x000020b0] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x000020b8] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x000020c0] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x000020c8] */ 0x00000004, 0xe80009e7, // mov  dst, srel(i)
/* [0x000020d0] */ 0x00000017, 0xe80009e7, // mov  dst, sacq(i)
/* [0x000020d8] */ 0x009e7000, 0x100009e7, // nop
// ::mc_sync10_q8
/* [0x000020e0] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x000020e8] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x000020f0] */ 0x00000018, 0xe80009e7, // mov  dst, sacq(i)
/* [0x000020f8] */ 0x00000018, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00002100] */ 0x00000018, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00002108] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00002110] */ 0x0000001e, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00002118] */ 0x00000009, 0xe80009e7, // mov  dst, srel(i)
/* [0x00002120] */ 0x0000000c, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync10_q9
/* [0x00002128] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00002130] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00002138] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00002140] */ 0x00000008, 0xe80009e7, // mov  dst, srel(i)
/* [0x00002148] */ 0x00000019, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00002150] */ 0x0000000a, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync10_q10
/* [0x00002158] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00002160] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00002168] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00002170] */ 0x00000008, 0xe80009e7, // mov  dst, srel(i)
/* [0x00002178] */ 0x0000001a, 0xe80009e7, // mov  dst, sacq(i)
/* [0x00002180] */ 0x0000000b, 0xe80009e7, // mov  dst, srel(i)
// ::mc_sync10_q11
/* [0x00002188] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00002190] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00002198] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x000021a0] */ 0x00000008, 0xe80009e7, // mov  dst, srel(i)
/* [0x000021a8] */ 0x0000001b, 0xe80009e7, // mov  dst, sacq(i)
/* [0x000021b0] */ 0x009e7000, 0x100009e7, // nop
// ::mc_exit_c10_q0
// ::mc_exit_y10_q0
/* [0x000021b8] */ 0x00000002, 0xe00228e7, // mov.setf r3, PREREAD - 1
// :1
/* [0x000021c0] */ 0xffffffe0, 0xf03809e7, // brr.anynz -, r:1b
/* [0x000021c8] */ 0x009e7000, 0xa00009e7, // nop                   ; nop           ; ldtmu0
/* [0x000021d0] */ 0x009e7000, 0xb00009e7, // nop                   ; nop           ; ldtmu1
/* [0x000021d8] */ 0x0d9c17c0, 0xd00228e7, // sub.setf r3, r3, 1
/* [0x000021e0] */ 0x159f2fc0, 0x100009e7, // mov  -, vw_wait
/* [0x000021e8] */ 0x0000001c, 0xe80009e7, // mov  dst, sacq(i)
/* [0x000021f0] */ 0x009e7000, 0x300009e7, // nop                   ; nop           ; thrend
/* [0x000021f8] */ 0x00000001, 0xe00209a7, // mov interrupt, 1
/* [0x00002200] */ 0x009e7000, 0x100009e7, // nop
// ::mc_exit_c10_qn
// ::mc_exit_y10_qn
/* [0x00002208] */ 0x00000002, 0xe00228e7, // mov.setf r3, PREREAD - 1
// :1
/* [0x00002210] */ 0xffffffe0, 0xf03809e7, // brr.anynz -, r:1b
/* [0x00002218] */ 0x009e7000, 0xa00009e7, // nop                   ; nop           ; ldtmu0
/* [0x00002220] */ 0x009e7000, 0xb00009e7, // nop                   ; nop           ; ldtmu1
/* [0x00002228] */ 0x0d9c17c0, 0xd00228e7, // sub.setf r3, r3, 1
/* [0x00002230] */ 0x159f2fc0, 0x100009e7, // mov  -, vw_wait
/* [0x00002238] */ 0x009e7000, 0x300009e7, // nop                   ; nop           ; thrend
/* [0x00002240] */ 0x009e7000, 0x100009e7, // nop
/* [0x00002248] */ 0x009e7000, 0x100009e7, // nop
// ::mc_setup_y10_q0
/* [0x00002250] */ 0x0000000c, 0xe80009e7, // mov  dst, srel(i)
// ::mc_setup_y10_qn
/* [0x00002258] */ 0x95801ff6, 0xd0025900, // mov tmurs, 1                  ; mov ra0, unif
/* [0x00002260] */ 0x15827d80, 0x10020267, // mov ra9, unif
/* [0x00002268] */ 0x15827d80, 0x10020067, // mov ra1, unif
/* [0x00002270] */ 0xaaaaff00, 0xe6020827, // mov r0, [0,2,0,2,0,2,0,2,1,3,1,3,1,3,1,3]
/* [0x00002278] */ 0x9181e1f6, 0xd00250cb, // shl rb_ef, r0, i_shift30      ; mov ra11, unif
/* [0x00002280] */ 0xff800100, 0xe0020527, // mov ra_kff800100, 0xff800100
/* [0x00002288] */ 0x0000ffff, 0xe0021627, // mov rb_pmask, v_pmask
/* [0x00002290] */ 0x000803ff, 0xe00205e7, // mov ra_blk_height_pmax, ((1 << v_bit_depth) - 1) | (v_blk_height << 16)
/* [0x00002298] */ 0x00010000, 0xe00217e7, // mov rb_fir_off_h, (FIR_OFFSET << (v_bit_depth - 8))
/* [0x000022a0] */ 0x4000000c, 0xe0020667, // mov ra_fir_off_val_wt_den_p7, (FIR_OFFSET << 16) | (DENOM + 15 - v_bit_depth)
/* [0x000022a8] */ 0x050b0a00, 0xe0021567, // mov rb_y_coeffs_2, 0x050b0a00
/* [0x000022b0] */ 0x11283a40, 0xe00215a7, // mov rb_y_coeffs_3, 0x11283a40
/* [0x000022b8] */ 0x0a0b0500, 0xe00215e7, // mov rb_y_coeffs_5, 0x0a0b0500
/* [0x000022c0] */ 0x15827d80, 0x100200e7, // mov ra3, unif
/* [0x000022c8] */ 0x95803ff6, 0x10024754, // mov ra_ef, rb_ef              ; mov rb_xpitch, unif
/* [0x000022d0] */ 0x0d0c1dc0, 0xd4020827, // sub r0, ra3.16b, 1
/* [0x000022d8] */ 0x119c11c0, 0xd00216a7, // shl rb_max_x, r0, v_x_shift
/* [0x000022e0] */ 0x0d0c1dc0, 0xd20217a7, // sub rb_max_y, ra3.16a, 1
/* [0x000022e8] */ 0x959a0dbf, 0x100248d0, // mov r3, elem_num              ; mov rb_pitch, unif
/* [0x000022f0] */ 0xc0000000, 0xe0020867, // mov r1, vdw_setup_1(0)
/* [0x000022f8] */ 0x159d03c0, 0x10021667, // or  rb_dma1_base, r1, rb_pitch
/* [0x00002300] */ 0x0c027cc0, 0x14020827, // add r0, ra0.16b, r3
/* [0x00002308] */ 0x119c11c0, 0xd0020827, // shl r0, r0, v_x_shift
/* [0x00002310] */ 0x139c01c0, 0xd0020827, // max r0, r0, 0
/* [0x00002318] */ 0x129da1c0, 0x10020827, // min r0, r0, rb_max_x
/* [0x00002320] */ 0x119c31c0, 0xd0220567, // shl ra_xshift_next, r0, 3
/* [0x00002328] */ 0xf49dc1d2, 0xd0024822, // and r0, r0, -4                ; v8subs r2, r2, r2
/* [0x00002330] */ 0x0d9d05c0, 0x100208a7, // sub r2, r2, rb_pitch
/* [0x00002338] */ 0x149e7080, 0x10020867, // and r1, r0, r2
/* [0x00002340] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00002348] */ 0x0c9e7040, 0x10020827, // add r0, r0, r1
/* [0x00002350] */ 0x0c267c00, 0x10020627, // add ra_base, ra9, r0
/* [0x00002358] */ 0x0c067cc0, 0x14020827, // add r0, ra1.16b, r3
/* [0x00002360] */ 0x119c11c0, 0xd0020827, // shl r0, r0, v_x_shift
/* [0x00002368] */ 0x139c01c0, 0xd0020827, // max r0, r0, 0
/* [0x00002370] */ 0x129da1c0, 0x10020827, // min r0, r0, rb_max_x
/* [0x00002378] */ 0x119c31c0, 0xd0021067, // shl rb_xshift2_next, r0, 3
/* [0x00002380] */ 0x149dc1c0, 0xd0020827, // and r0, r0, -4
/* [0x00002388] */ 0x149e7080, 0x10020867, // and r1, r0, r2
/* [0x00002390] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00002398] */ 0x0c9e7040, 0x10020827, // add r0, r0, r1
/* [0x000023a0] */ 0x0c2e7c00, 0x100213e7, // add rb_base2, ra11, r0
/* [0x000023a8] */ 0x80027036, 0x120049e0, // nop                           ; mov r0, ra0.16a
/* [0x000023b0] */ 0x95043ff6, 0xd20248e2, // mov r3, PREREAD               ; mov r2, ra1.16a
// :1
/* [0x000023b8] */ 0x0d9c17c0, 0xd00228e7, // sub.setf r3, r3, 1
/* [0x000023c0] */ 0x139c01c0, 0xd0020867, // max r1, r0, 0
/* [0x000023c8] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_max_y
/* [0x000023d0] */ 0x4c51018f, 0x1a024821, // add r0, r0, ra_k1             ; mul24 r1, r1, rb_pitch
/* [0x000023d8] */ 0x8c627c40, 0x10225e11, // add t0s, ra_base, r1          ; mov ra_y, r0
/* [0x000023e0] */ 0x139c05c0, 0xd0020867, // max r1, r2, 0
/* [0x000023e8] */ 0xffffffb0, 0xf03809e7, // brr.anynz -, r:1b
/* [0x000023f0] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_max_y
/* [0x000023f8] */ 0x4c51058f, 0x1a0248a1, // add r2, r2, ra_k1             ; mul24 r1, r1, rb_pitch
/* [0x00002400] */ 0x8c9cfe52, 0x10125f11, // add t1s, rb_base2, r1         ; mov ra_y2, r2
/* [0x00002408] */ 0x159e6fc0, 0x100208a7, // mov r2, qpu_num
/* [0x00002410] */ 0x0f9c15c0, 0xd0020867, // asr r1, r2, 1
/* [0x00002418] */ 0x119c43c0, 0xd0020867, // shl r1, r1, 4
/* [0x00002420] */ 0x149c15c0, 0xd0020827, // and r0, r2, 1
/* [0x00002428] */ 0x159e7040, 0x10020827, // or  r0, r0, r1
/* [0x00002430] */ 0x00002900, 0xe0020867, // mov r1, vpm_setup(0, 2, h16p(0, 0))
/* [0x00002438] */ 0x0c9e7040, 0x10021727, // add r_vpm, r0, r1
/* [0x00002440] */ 0x80004002, 0xe0020867, // mov r1, vdw_setup_0(0, 0, dma_h16p(0,0,0))
/* [0x00002448] */ 0x119c61c0, 0xd0020827, // shl r0, r0, 6
/* [0x00002450] */ 0x0c9e7040, 0x100216e7, // add r_dma, r0, r1
/* [0x00002458] */ 0x15827d80, 0x100207a7, // mov ra_link, unif
/* [0x00002460] */ 0x00000000, 0xe0024208, // mov ra8,  0                   ; mov rb8,  0
/* [0x00002468] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00002470] */ 0x00000000, 0xe0024249, // mov ra9,  0                   ; mov rb9,  0
/* [0x00002478] */ 0x00000000, 0xe002428a, // mov ra10, 0                   ; mov rb10, 0
/* [0x00002480] */ 0x00000000, 0xe00242cb, // mov ra11, 0                   ; mov rb11, 0
// :per_block_setup_10
/* [0x00002488] */ 0x119c11c0, 0xd0020827, // shl r0, r0, v_x_shift
/* [0x00002490] */ 0x93567176, 0x14125815, // max r0, r0, r5                ; mov ra_xshift, ra_xshift_next
/* [0x00002498] */ 0x129da1c0, 0x10020827, // min r0, r0, rb_max_x
/* [0x000024a0] */ 0x119c31c0, 0xd0220567, // shl ra_xshift_next, r0, 3
/* [0x000024a8] */ 0x149dc1c0, 0xd0020827, // and r0, r0, -4
/* [0x000024b0] */ 0x8d810bf6, 0x1002589a, // sub r2, r5, rb_pitch          ; mov ra_base_next, unif
/* [0x000024b8] */ 0x940270b6, 0x12225853, // and r1, r0, r2                ; mov ra_y_next, ra0.16a
/* [0x000024c0] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x000024c8] */ 0x8c827076, 0x10025801, // add r0, r0, r1                ; mov ra1, unif
/* [0x000024d0] */ 0x0c6a7c00, 0x100206a7, // add ra_base_next, ra_base_next, r0
/* [0x000024d8] */ 0x0c067cc0, 0x14020827, // add r0, ra1.16b, r3
/* [0x000024e0] */ 0x119c11c0, 0xd0020827, // shl r0, r0, v_x_shift
/* [0x000024e8] */ 0x93067176, 0x12125813, // max r0, r0, r5                ; mov ra_y2_next, ra1.16a
/* [0x000024f0] */ 0x9281a1f6, 0x10024813, // min r0, r0, rb_max_x          ; mov rb_base2_next, unif
/* [0x000024f8] */ 0x119c31c0, 0xd0021067, // shl rb_xshift2_next, r0, 3
/* [0x00002500] */ 0x9481c1f6, 0xd0025810, // and r0, r0, -4                ; mov ra_width_height, unif
/* [0x00002508] */ 0x949dc0bf, 0x10024871, // and r1, r0, r2                ; mov vw_setup, rb_vpm_init
/* [0x00002510] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00002518] */ 0x4c402077, 0xd4024821, // add r0, r0, r1                ; mul24 r1, ra_width, v_x_mul
/* [0x00002520] */ 0x0c9d3e00, 0x100214e7, // add rb_base2_next, rb_base2_next, r0
/* [0x00002528] */ 0x8d419e76, 0x12025760, // sub rb_dma1, rb_dma1_base, r1 ; mov r0, ra_height
/* [0x00002530] */ 0x8c5dc1c6, 0xdc025460, // add rb_i_tmu, r0, (7-8) - PREREAD ; v8min r0, r0, ra_blk_height
/* [0x00002538] */ 0x0c9df1c0, 0xd00214a7, // add rb_lcount, r0, (7-8)
/* [0x00002540] */ 0x916481f6, 0xd4024823, // shl r0, r0, v_dma_h_shift     ; mov r3, ra_kmul_add
/* [0x00002548] */ 0x0c9e7040, 0x10020827, // add r0, r0, r1
/* [0x00002550] */ 0x9164f1f6, 0xd4024822, // shl r0, r0, v_dma_wh_shift    ; mov r2, ra_fir_off_val
/* [0x00002558] */ 0x8c81b1f6, 0x100246e0, // add ra_dma0, r0, rb_dma0_base ; mov r0, unif
/* [0x00002560] */ 0x918101f6, 0xd00a5816, // shl.ifnn r0, r0, i_shift16    ; mov ra_wt_off_mul_l0, unif
/* [0x00002568] */ 0x915031f6, 0xde024205, // shl ra8, r0, 3                ; mov rb5, ra_k255
/* [0x00002570] */ 0x01040400, 0xe0020867, // mov r1, 0x01040400
/* [0x00002578] */ 0x10227380, 0x1e5200a7, // ror ra2.8b, r1, ra8.8d
/* [0x00002580] */ 0x10227380, 0x1c520027, // ror ra0.8b, r1, ra8.8c
/* [0x00002588] */ 0x10215f80, 0x1e6200a7, // ror ra2.8c, rb_y_coeffs_2, ra8.8d
/* [0x00002590] */ 0x10215f80, 0x1c620027, // ror ra0.8c, rb_y_coeffs_2, ra8.8c
/* [0x00002598] */ 0x00010100, 0xe0020867, // mov r1,0x00010100
/* [0x000025a0] */ 0x902203bf, 0x1e025812, // ror r0, r1, ra8.8d            ; mov ra_wt_off_mul_l1, unif
/* [0x000025a8] */ 0x90205387, 0x1c424004, // ror ra0.8a, r1, ra8.8c        ; v8min rb4, r0, rb5
/* [0x000025b0] */ 0x914883f6, 0xd0031856, // shl r1, r1, 8                 ; mov.ifn ra_wt_off_mul_l0, ra_wt_off_mul_l1
/* [0x000025b8] */ 0x902203bf, 0x1e02581c, // ror r0, r1, ra8.8d            ; mov ra_dest, unif
/* [0x000025c0] */ 0x90205387, 0x1c72404b, // ror ra1.8d, r1, ra8.8c        ; v8min rb11, r0, rb5
/* [0x000025c8] */ 0x10216f80, 0x1e7200a7, // ror ra2.8d, rb_y_coeffs_3, ra8.8d
/* [0x000025d0] */ 0x10216f80, 0x1c720027, // ror ra0.8d, rb_y_coeffs_3, ra8.8c
/* [0x000025d8] */ 0x10217f80, 0x1e5200e7, // ror ra3.8b, rb_y_coeffs_5, ra8.8d
/* [0x000025e0] */ 0x10217f80, 0x1c520067, // ror ra1.8b, rb_y_coeffs_5, ra8.8c
/* [0x000025e8] */ 0x04040100, 0xe0020867, // mov r1,0x04040100
/* [0x000025f0] */ 0x10227380, 0x1e6200e7, // ror ra3.8c, r1, ra8.8d
/* [0x000025f8] */ 0x902183bf, 0xdc624065, // ror ra1.8c, r1, ra8.8c        ; mov r5rep, -8
/* [0x00002600] */ 0x00000000, 0xf0f7c9e7, // bra -, ra_link
/* [0x00002608] */ 0x3a281100, 0xe0020867, // mov r1,0x3a281100
/* [0x00002610] */ 0x902203bf, 0x1e02581e, // ror r0, r1, ra8.8d            ; mov ra_link, unif
/* [0x00002618] */ 0x90205387, 0x1c424048, // ror ra1.8a, r1, ra8.8c        ; v8min rb8, r0, rb5
// ::mc_filter_y10_pxx
/* [0x00002620] */ 0xfffffe48, 0xf0f807a7, // brr ra_link, r:per_block_setup_10
/* [0x00002628] */ 0x959a0ff6, 0x10024023, // mov ra0, unif                 ; mov r3, elem_num
/* [0x00002630] */ 0xec9c3fd2, 0x100269e5, // add.setf -, rb_ef, rb_ef      ; v8subs r5rep, r2, r2
/* [0x00002638] */ 0x8c001cff, 0x14024800, // add r0, ra0.16b, r3           ; mov rb_xshift2, rb_xshift2_next
/* [0x00002640] */ 0x1158adc0, 0xd4020867, // shl r1, ra_wt_off_l0, i_wt_den_p5
/* [0x00002648] */ 0x4c5a7cd6, 0x121245a0, // add ra_wt_mul_l0, ra_wt_mul_l0, r3 ; mul24 r0, r2, ra_wt_mul_l0
/* [0x00002650] */ 0x8d9c423f, 0x1042531d, // sub rb_wt_off, r1, r0         ; mov ra_ef.8a, rb4
// :1
/* [0x00002658] */ 0x4c745dbe, 0x100279c4, // add.setf -, ra_ef, ra_ef      ; mul24 ra4, rb5, ra_ef
/* [0x00002660] */ 0x93440dff, 0xd40248a1, // max r2, ra_y, 0               ; mov r1, 0
/* [0x00002668] */ 0x9251e5f6, 0x1a0248a3, // min r2, r2, rb_max_y          ; mov r3, ra_k1
/* [0x00002670] */ 0x4c450cd7, 0xa4224462, // add ra_y, ra_y, r3            ; mul24 r2, r2, rb_pitch        ; ldtmu0
/* [0x00002678] */ 0x8c606cbf, 0x10024e05, // add t0s, ra_base, r2          ; mov rb5,  rb6
/* [0x00002680] */ 0x8e5479bf, 0x12024806, // shr r0, r4, ra_xshift         ; mov rb6,  rb7
/* [0x00002688] */ 0x93458c47, 0xb20248a0, // max r2, ra_y2, r1             ; v8min r0, r0, rb_pmask        ; ldtmu1
/* [0x00002690] */ 0x8e2009f6, 0x10024847, // shr r1, r4, rb_xshift2        ; mov rb7, ra8
/* [0x00002698] */ 0x925de5ce, 0x120248a1, // min r2, r2, rb_max_y          ; v8min r1, r1, ra_pmax
/* [0x000026a0] */ 0x4c450cd7, 0x12124462, // add ra_y2, ra_y2, r3          ; mul24 r2, r2, rb_pitch
/* [0x000026a8] */ 0x8c24feb6, 0x10025f08, // add t1s, rb_base2, r2         ; mov ra8,  ra9
/* [0x000026b0] */ 0x4c038af1, 0xd8025962, // add r5rep, r5, r3     ; mul24      r2, ra0.8a << 8,  r1 << 8  @ "mul_used", 0
/* [0x000026b8] */ 0x5501fff0, 0x180348e2, // mov r3, rb_fir_off_h  ; mul24.ifnn r2, ra0.8a,       r0
/* [0x000026c0] */ 0x4d03f6b0, 0xda0248a3, // sub r2, r3, r2        ; mul24      r3, ra0.8b << 1,  r0 << 1  @ "mul_used", 0
/* [0x000026c8] */ 0x40037031, 0xda0109e3, // nop                   ; mul24.ifn  r3, ra0.8b << 9,  r1 << 9  @ "mul_used", 0
/* [0x000026d0] */ 0x4c03e4f0, 0xdc0248a3, // add r2, r2, r3        ; mul24      r3, ra0.8c << 2,  r0 << 2  @ "mul_used", 0
/* [0x000026d8] */ 0x40036031, 0xdc0109e3, // nop                   ; mul24.ifn  r3, ra0.8c << 10, r1 << 10 @ "mul_used", 0
/* [0x000026e0] */ 0x4d03d4f0, 0xde0248a3, // sub r2, r2, r3        ; mul24      r3, ra0.8d << 3,  r0 << 3  @ "mul_used", 0
/* [0x000026e8] */ 0x40035031, 0xde0109e3, // nop                   ; mul24.ifn  r3, ra0.8d << 11, r1 << 11 @ "mul_used", 0
/* [0x000026f0] */ 0x4c07c4f0, 0xd80248a3, // add r2, r2, r3        ; mul24      r3, ra1.8a << 4,  r0 << 4  @ "mul_used", 0
/* [0x000026f8] */ 0x40074031, 0xd80109e3, // nop                   ; mul24.ifn  r3, ra1.8a << 12, r1 << 12 @ "mul_used", 0
/* [0x00002700] */ 0x4c07b4f0, 0xda0248a3, // add r2, r2, r3        ; mul24      r3, ra1.8b << 5,  r0 << 5  @ "mul_used", 0
/* [0x00002708] */ 0x40073031, 0xda0109e3, // nop                   ; mul24.ifn  r3, ra1.8b << 13, r1 << 13 @ "mul_used", 0
/* [0x00002710] */ 0x4d07a4f0, 0xdc0248a3, // sub r2, r2, r3        ; mul24      r3, ra1.8c << 6,  r0 << 6  @ "mul_used", 0
/* [0x00002718] */ 0x40072031, 0xdc0109e3, // nop                   ; mul24.ifn  r3, ra1.8c << 14, r1 << 14 @ "mul_used", 0
/* [0x00002720] */ 0x4c0794f0, 0xde0248a3, // add r2, r2, r3        ; mul24      r3, ra1.8d << 7,  r0 << 7  @ "mul_used", 0
/* [0x00002728] */ 0x4c071b71, 0xde0329e3, // add.setf -, r5, r5    ; mul24.ifn  r3, ra1.8d << 15, r1 << 15 @ "mul_used", 0
/* [0x00002730] */ 0xffffff08, 0xf06809e7, // brr.anyn -, r:1b
/* [0x00002738] */ 0x4d0854fe, 0x1a0248a1, // sub r2, r2, r3                ; mul24 r1, rb5,  ra2.8b
/* [0x00002740] */ 0x550caffe, 0x1a024260, // mov ra9,  rb10                ; mul24 r0, rb10, ra3.8b
/* [0x00002748] */ 0x8f2c25f6, 0xd00242ca, // asr ra11, r2, v_bit_depth - 8 ; mov rb10, ra11
/* [0x00002750] */ 0x4d08623e, 0x1c024860, // sub r1, r1, r0                ; mul24 r0, rb6,  ra2.8c
/* [0x00002758] */ 0x4d08723e, 0x1e024860, // sub r1, r1, r0                ; mul24 r0, rb7,  ra2.8d
/* [0x00002760] */ 0x4c208237, 0x10024860, // add r1, r1, r0                ; mul24 r0, ra8,  rb8
/* [0x00002768] */ 0x4c0ca23e, 0x1c024860, // add r1, r1, r0                ; mul24 r0, rb10, ra3.8c
/* [0x00002770] */ 0x4c2cb237, 0x10024860, // add r1, r1, r0                ; mul24 r0, ra11, rb11
/* [0x00002778] */ 0x8d5d1bf6, 0x1c0269e3, // sub.setf -, r5, rb_i_tmu      ; mov r3, ra_blk_height
/* [0x00002780] */ 0x8d1133bf, 0x1002884f, // sub r1, r1, ra4               ; mov.ifz rb_base2, rb_base2_next
/* [0x00002788] */ 0x8d6a7236, 0x10029858, // sub r1, r1, r0                ; mov.ifz ra_base, ra_base_next
/* [0x00002790] */ 0x8f4c63f6, 0xd0029851, // asr r1, r1, 6                 ; mov.ifz ra_y_y2, ra_y_y2_next
/* [0x00002798] */ 0x4d592bce, 0x120269e0, // sub.setf -, r5, rb_lcount     ; mul24 r0, r1, ra_wt_mul_l0
/* [0x000027a0] */ 0x4c64c1ce, 0x14024821, // add r0, r0, rb_wt_off         ; mul24 r1, r1, ra_kmul_add
/* [0x000027a8] */ 0xed427073, 0x12024860, // sub r1, r0, r1                ; v8subs r0, ra_height, r3
/* [0x000027b0] */ 0xfffffe88, 0xf06809e7, // brr.anyn -, r:1b
/* [0x000027b8] */ 0x0f9cb3c0, 0xd0020867, // asr r1, r1, i_wt_den_p6
/* [0x000027c0] */ 0x925f23bf, 0x12020867, // min r1, r1, ra_pmax           ; mov -, vw_wait
/* [0x000027c8] */ 0x5351039f, 0x18024c22, // max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch
/* [0x000027d0] */ 0x956e7036, 0x10126431, // mov.setf ra_height, r0        ; mov vw_setup, ra_dma0
/* [0x000027d8] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x000027e0] */ 0x929dd0ff, 0x10024831, // min r0, r0, r3                ; mov vw_setup, rb_dma1
/* [0x000027e8] */ 0x8d7270f6, 0x10024872, // sub r1, r0, r3                ; mov vw_addr, ra_dest
/* [0x000027f0] */ 0x119d73c0, 0xd0020867, // shl r1, r1, i_shift23
/* [0x000027f8] */ 0xfffffe40, 0xf0f809e7, // brr -, r:1b
/* [0x00002800] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x00002808] */ 0x0c6e7c40, 0x100206e7, // add ra_dma0, ra_dma0, r1
/* [0x00002810] */ 0x8c71ccbf, 0x10024731, // add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init
// ::mc_filter_y10_p00
/* [0x00002818] */ 0x959a0ff6, 0x10024020, // mov ra0, unif                 ; mov r0, elem_num
/* [0x00002820] */ 0xf5567dad, 0x14124565, // mov ra_xshift, ra_xshift_next ; v8subs r5rep, r5, r5
/* [0x00002828] */ 0x8c020c3f, 0x1402581a, // add r0, ra0.16b, r0           ; mov ra_base_next, unif
/* [0x00002830] */ 0x119c11c0, 0xd0020827, // shl r0, r0, v_x_shift
/* [0x00002838] */ 0x93027176, 0x12225813, // max r0, r0, r5                ; mov ra_y_next, ra0.16a
/* [0x00002840] */ 0x9281a1f6, 0x10025810, // min r0, r0, rb_max_x          ; mov ra_width_height, unif
/* [0x00002848] */ 0x119c31c0, 0xd0220567, // shl ra_xshift_next, r0, 3
/* [0x00002850] */ 0x149dc1c0, 0xd0020827, // and r0, r0, -4
/* [0x00002858] */ 0x8d810bf6, 0x10025896, // sub r2, r5, rb_pitch          ; mov ra_wt_off_mul_l0, unif
/* [0x00002860] */ 0x149e7080, 0x10020867, // and r1, r0, r2
/* [0x00002868] */ 0x569d404f, 0x10024821, // xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
/* [0x00002870] */ 0x8c827076, 0x1002581c, // add r0, r0, r1                ; mov ra_dest, unif
/* [0x00002878] */ 0x8c69cc3f, 0x100246b1, // add ra_base_next, ra_base_next, r0 ; mov vw_setup, rb_vpm_init
/* [0x00002880] */ 0x11401dc0, 0xd4020867, // shl r1, ra_width, v_x_shift
/* [0x00002888] */ 0x8d419e76, 0x12025760, // sub rb_dma1, rb_dma1_base, r1 ; mov r0, ra_height
/* [0x00002890] */ 0x8d5c31c6, 0xdc025460, // sub rb_i_tmu, r0, PREREAD     ; v8min r0, r0, ra_blk_height
/* [0x00002898] */ 0x919c81c0, 0xd0024812, // shl r0, r0, v_dma_h_shift     ; mov rb_lcount, r0
/* [0x000028a0] */ 0x0c9e7040, 0x10020827, // add r0, r0, r1
/* [0x000028a8] */ 0x1158edc0, 0xd4021327, // shl rb_wt_off, ra_wt_off_l0, DENOM + 7
/* [0x000028b0] */ 0x9180f1f6, 0xd002581e, // shl r0, r0, v_dma_wh_shift    ; mov ra_link, unif
/* [0x000028b8] */ 0x0c9db1c0, 0x100206e7, // add ra_dma0, r0, rb_dma0_base
// :1
/* [0x000028c0] */ 0xcd511bee, 0x1a0269e5, // sub.setf -, r5, rb_i_tmu      ; v8adds r5rep, r5, ra_k1
/* [0x000028c8] */ 0x804e7036, 0xa42099d1, // nop                           ; mov.ifz ra_y, ra_y_next       ; ldtmu0
/* [0x000028d0] */ 0x8e5509bf, 0x12024823, // shr r0, r4, ra_xshift         ; mov r3, rb_pitch
/* [0x000028d8] */ 0x13440dc0, 0xd40208a7, // max r2, ra_y, 0
/* [0x000028e0] */ 0x9269e5f6, 0x10029898, // min r2, r2, rb_max_y          ; mov.ifz ra_base, ra_base_next
/* [0x000028e8] */ 0x4c441dd3, 0xd4224462, // add ra_y, ra_y, 1             ; mul24 r2, r2, r3
/* [0x000028f0] */ 0x8c618c87, 0x10024e20, // add t0s, ra_base, r2          ; v8min r0, r0, rb_pmask
/* [0x000028f8] */ 0x4d592bc6, 0x120269e1, // sub.setf -, r5, rb_lcount     ; mul24 r1, r0, ra_wt_mul_l0
/* [0x00002900] */ 0x915c83f6, 0xdc024863, // shl r1, r1, 8                 ; mov r3, ra_blk_height
/* [0x00002908] */ 0xec40c3f3, 0x12024860, // add r1, r1, rb_wt_off         ; v8subs r0, ra_height, r3
/* [0x00002910] */ 0xffffff90, 0xf06809e7, // brr.anyn -, r:1b
/* [0x00002918] */ 0x0f9cf3c0, 0xd0020867, // asr r1, r1, DENOM + 8
/* [0x00002920] */ 0x925f23bf, 0x12020867, // min r1, r1, ra_pmax           ; mov -, vw_wait
/* [0x00002928] */ 0x5351039f, 0x18024c22, // max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch
/* [0x00002930] */ 0x956e7036, 0x10126431, // mov.setf ra_height, r0 ; mov vw_setup, ra_dma0
/* [0x00002938] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x00002940] */ 0x929dd0ff, 0x10024831, // min r0, r0, r3        ; mov vw_setup, rb_dma1
/* [0x00002948] */ 0x8d7270f6, 0x10024872, // sub r1, r0, r3        ; mov vw_addr, ra_dest
/* [0x00002950] */ 0x119d73c0, 0xd0020867, // shl r1, r1, i_shift23
/* [0x00002958] */ 0xffffff48, 0xf0f809e7, // brr -, r:1b
/* [0x00002960] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x00002968] */ 0x0c6e7c40, 0x100206e7, // add ra_dma0, ra_dma0, r1
/* [0x00002970] */ 0x8c71ccbf, 0x10024731, // add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init
// ::mc_filter_y10_bxx
/* [0x00002978] */ 0xfffffaf0, 0xf0f807a7, // brr ra_link, r:per_block_setup_10
/* [0x00002980] */ 0x959a0ff6, 0x10024023, // mov ra0, unif                 ; mov r3, elem_num
/* [0x00002988] */ 0xec9c3fd2, 0x100269e5, // add.setf -, rb_ef, rb_ef      ; v8subs r5rep, r2, r2
/* [0x00002990] */ 0x8c001cff, 0x14024800, // add r0, ra0.16b, r3           ; mov rb_xshift2, rb_xshift2_next
/* [0x00002998] */ 0x1158bdc0, 0xd4020867, // shl r1, ra_wt_off_l0, i_wt_den_p6
/* [0x000029a0] */ 0x4c5a7cd6, 0x121245a0, // add ra_wt_mul_l0, ra_wt_mul_l0, r3 ; mul24 r0, r2, ra_wt_mul_l0
/* [0x000029a8] */ 0x4d4a7216, 0x12024860, // sub r1, r1, r0                ; mul24 r0, r2, ra_wt_mul_l1
/* [0x000029b0] */ 0x8d9c423f, 0x1042531d, // sub rb_wt_off, r1, r0         ; mov ra_ef.8a, rb4
// :1
/* [0x000029b8] */ 0x4c745dbe, 0x100279c4, // add.setf -, ra_ef, ra_ef      ; mul24 ra4, rb5, ra_ef
/* [0x000029c0] */ 0x93440dff, 0xd40248a1, // max r2, ra_y, 0               ; mov r1, 0
/* [0x000029c8] */ 0x9251e5f6, 0x1a0248a3, // min r2, r2, rb_max_y          ; mov r3, ra_k1
/* [0x000029d0] */ 0x4c450cd7, 0xa4224462, // add ra_y, ra_y, r3            ; mul24 r2, r2, rb_pitch        ; ldtmu0
/* [0x000029d8] */ 0x8c606cbf, 0x10024e05, // add t0s, ra_base, r2          ; mov rb5,  rb6
/* [0x000029e0] */ 0x8e5479bf, 0x12024806, // shr r0, r4, ra_xshift         ; mov rb6,  rb7
/* [0x000029e8] */ 0x93458c47, 0xb20248a0, // max r2, ra_y2, r1             ; v8min r0, r0, rb_pmask        ; ldtmu1
/* [0x000029f0] */ 0x8e2009f6, 0x10024847, // shr r1, r4, rb_xshift2        ; mov rb7, ra8
/* [0x000029f8] */ 0x925de5ce, 0x120248a1, // min r2, r2, rb_max_y          ; v8min r1, r1, ra_pmax
/* [0x00002a00] */ 0x4c450cd7, 0x12124462, // add ra_y2, ra_y2, r3          ; mul24 r2, r2, rb_pitch
/* [0x00002a08] */ 0x8c24feb6, 0x10025f08, // add t1s, rb_base2, r2         ; mov ra8,  ra9
/* [0x00002a10] */ 0x4c038af1, 0xd8025962, // add r5rep, r5, r3     ; mul24      r2, ra0.8a << 8,  r1 << 8  @ "mul_used", 0
/* [0x00002a18] */ 0x5501fff0, 0x180348e2, // mov r3, rb_fir_off_h  ; mul24.ifnn r2, ra0.8a,       r0
/* [0x00002a20] */ 0x4d03f6b0, 0xda0248a3, // sub r2, r3, r2        ; mul24      r3, ra0.8b << 1,  r0 << 1  @ "mul_used", 0
/* [0x00002a28] */ 0x40037031, 0xda0109e3, // nop                   ; mul24.ifn  r3, ra0.8b << 9,  r1 << 9  @ "mul_used", 0
/* [0x00002a30] */ 0x4c03e4f0, 0xdc0248a3, // add r2, r2, r3        ; mul24      r3, ra0.8c << 2,  r0 << 2  @ "mul_used", 0
/* [0x00002a38] */ 0x40036031, 0xdc0109e3, // nop                   ; mul24.ifn  r3, ra0.8c << 10, r1 << 10 @ "mul_used", 0
/* [0x00002a40] */ 0x4d03d4f0, 0xde0248a3, // sub r2, r2, r3        ; mul24      r3, ra0.8d << 3,  r0 << 3  @ "mul_used", 0
/* [0x00002a48] */ 0x40035031, 0xde0109e3, // nop                   ; mul24.ifn  r3, ra0.8d << 11, r1 << 11 @ "mul_used", 0
/* [0x00002a50] */ 0x4c07c4f0, 0xd80248a3, // add r2, r2, r3        ; mul24      r3, ra1.8a << 4,  r0 << 4  @ "mul_used", 0
/* [0x00002a58] */ 0x40074031, 0xd80109e3, // nop                   ; mul24.ifn  r3, ra1.8a << 12, r1 << 12 @ "mul_used", 0
/* [0x00002a60] */ 0x4c07b4f0, 0xda0248a3, // add r2, r2, r3        ; mul24      r3, ra1.8b << 5,  r0 << 5  @ "mul_used", 0
/* [0x00002a68] */ 0x40073031, 0xda0109e3, // nop                   ; mul24.ifn  r3, ra1.8b << 13, r1 << 13 @ "mul_used", 0
/* [0x00002a70] */ 0x4d07a4f0, 0xdc0248a3, // sub r2, r2, r3        ; mul24      r3, ra1.8c << 6,  r0 << 6  @ "mul_used", 0
/* [0x00002a78] */ 0x40072031, 0xdc0109e3, // nop                   ; mul24.ifn  r3, ra1.8c << 14, r1 << 14 @ "mul_used", 0
/* [0x00002a80] */ 0x4c0794f0, 0xde0248a3, // add r2, r2, r3        ; mul24      r3, ra1.8d << 7,  r0 << 7  @ "mul_used", 0
/* [0x00002a88] */ 0x4c071b71, 0xde0329e3, // add.setf -, r5, r5    ; mul24.ifn  r3, ra1.8d << 15, r1 << 15 @ "mul_used", 0
/* [0x00002a90] */ 0xffffff08, 0xf06809e7, // brr.anyn -, r:1b
/* [0x00002a98] */ 0x4d0854fe, 0x1a0248a1, // sub r2, r2, r3                ; mul24 r1, rb5,  ra2.8b
/* [0x00002aa0] */ 0x550caffe, 0x1a024260, // mov ra9,  rb10                ; mul24 r0, rb10, ra3.8b
/* [0x00002aa8] */ 0x8f2c25f6, 0xd00242ca, // asr ra11, r2, v_bit_depth - 8 ; mov rb10, ra11
/* [0x00002ab0] */ 0x4d08623e, 0x1c024860, // sub r1, r1, r0                ; mul24 r0, rb6,  ra2.8c
/* [0x00002ab8] */ 0x4d08723e, 0x1e024860, // sub r1, r1, r0                ; mul24 r0, rb7,  ra2.8d
/* [0x00002ac0] */ 0x4c208237, 0x10024860, // add r1, r1, r0                ; mul24 r0, ra8,  rb8
/* [0x00002ac8] */ 0x4c0ca23e, 0x1c024860, // add r1, r1, r0                ; mul24 r0, rb10, ra3.8c
/* [0x00002ad0] */ 0x4c2cb237, 0x10024860, // add r1, r1, r0                ; mul24 r0, ra11, rb11
/* [0x00002ad8] */ 0x0d127380, 0x10020867, // sub r1, r1, ra4
/* [0x00002ae0] */ 0x8d9cc23f, 0x10024862, // sub r1, r1, r0                ; mov r2, rb_wt_off
/* [0x00002ae8] */ 0x0f9c63c0, 0xd0020867, // asr r1, r1, 6
/* [0x00002af0] */ 0x4d591bce, 0x120269e0, // sub.setf -, r5, rb_i_tmu      ; mul24 r0, r1, ra_wt_mul_l0
/* [0x00002af8] */ 0x55653fce, 0x140453e1, // mov.ifz rb_base2, rb_base2_next ; mul24 r1, r1, ra_kmul_add
/* [0x00002b00] */ 0x8d4e7076, 0x10029851, // sub r1, r0, r1                ; mov.ifz ra_y_y2, ra_y_y2_next
/* [0x00002b08] */ 0x8d692bf6, 0x1002b9d8, // sub.setf -, r5, rb_lcount     ; mov.ifz ra_base, ra_base_next
/* [0x00002b10] */ 0x8c9f8289, 0xd0024860, // add r1, r1, r2                ; mov r0, r1 << 8
/* [0x00002b18] */ 0x8c5e7236, 0x1c024863, // add r1, r1, r0                ; mov r3, ra_blk_height
/* [0x00002b20] */ 0xfffffe78, 0xf06809e7, // brr.anyn -, r:1b
/* [0x00002b28] */ 0x4f65039f, 0x18024862, // asr r1, r1, ra_wt_den_p7      ; mul24 r2, r3, rb_pitch
/* [0x00002b30] */ 0x925f23bf, 0x12020867, // min r1, r1, ra_pmax           ; mov -, vw_wait
/* [0x00002b38] */ 0xf34003f3, 0xd2024c20, // max vpm, r1, 0                ; v8subs r0, ra_height, r3
/* [0x00002b40] */ 0x956e7036, 0x10126431, // mov.setf ra_height, r0        ; mov vw_setup, ra_dma0
/* [0x00002b48] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x00002b50] */ 0x929dd0ff, 0x10024831, // min r0, r0, r3                ; mov vw_setup, rb_dma1
/* [0x00002b58] */ 0x8d7270f6, 0x10024872, // sub r1, r0, r3                ; mov vw_addr, ra_dest
/* [0x00002b60] */ 0x119d73c0, 0xd0020867, // shl r1, r1, i_shift23
/* [0x00002b68] */ 0xfffffe30, 0xf0f809e7, // brr -, r:1b
/* [0x00002b70] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x00002b78] */ 0x0c6e7c40, 0x100206e7, // add ra_dma0, ra_dma0, r1
/* [0x00002b80] */ 0x8c71ccbf, 0x10024731, // add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init
// ::mc_filter_y10_b00
/* [0x00002b88] */ 0xfffff8e0, 0xf0f807a7, // brr ra_link, r:per_block_setup_10
/* [0x00002b90] */ 0x959a0ff6, 0x10024023, // mov ra0, unif                 ; mov r3, elem_num
/* [0x00002b98] */ 0xec9c3fd2, 0x100269e5, // add.setf -, rb_ef, rb_ef      ; v8subs r5rep, r2, r2
/* [0x00002ba0] */ 0x8c001cff, 0x14024800, // add r0, ra0.16b, r3           ; mov rb_xshift2, rb_xshift2_next
/* [0x00002ba8] */ 0x00000001, 0xe00208a7, // mov r2, 1
/* [0x00002bb0] */ 0x8c591eb6, 0x10025461, // add rb_i_tmu, rb_i_tmu, r2    ; mov r1, ra_wt_off_mul_l0
/* [0x00002bb8] */ 0xf158fded, 0xd4025325, // shl rb_wt_off, ra_wt_off_l0, DENOM + 8 ; v8subs r5quad, r5, r5
/* [0x00002bc0] */ 0x809f8009, 0xd000d9d6, // nop                           ; mov.ifnz ra_wt_off_mul_l0, r1 << 8
// :1
/* [0x00002bc8] */ 0x0d9d1bc0, 0xb00229e7, // sub.setf -, r5, rb_i_tmu      ; nop                           ; ldtmu1
/* [0x00002bd0] */ 0x8e4c09f6, 0xa0029851, // shr r1, r4, rb_xshift2        ; mov.ifz ra_y_y2, ra_y_y2_next ; ldtmu0
/* [0x00002bd8] */ 0x8e5509bf, 0x12024823, // shr r0, r4, ra_xshift         ; mov r3, rb_pitch
/* [0x00002be0] */ 0x13440dc0, 0xd40208a7, // max r2, ra_y, 0
/* [0x00002be8] */ 0x9269e5f6, 0x10029898, // min r2, r2, rb_max_y          ; mov.ifz ra_base, ra_base_next
/* [0x00002bf0] */ 0x4c441dd3, 0xd4224462, // add ra_y, ra_y, 1             ; mul24 r2, r2, r3
/* [0x00002bf8] */ 0x8c613cbf, 0x10028e0f, // add t0s, ra_base, r2          ; mov.ifz rb_base2, rb_base2_next
/* [0x00002c00] */ 0x13440dc0, 0xd20208a7, // max r2, ra_y2, 0
/* [0x00002c08] */ 0x129de5c0, 0x100208a7, // min r2, r2, rb_max_y
/* [0x00002c10] */ 0x4c441dd3, 0xd2124462, // add ra_y2, ra_y2, 1           ; mul24 r2, r2, r3
/* [0x00002c18] */ 0x8c5cfe86, 0x12024f20, // add t1s, rb_base2, r2         ; v8min r0, r0, ra_pmax
/* [0x00002c20] */ 0x545983c6, 0x12024860, // and r1, r1, rb_pmask          ; mul24 r0, r0, ra_wt_mul_l0
/* [0x00002c28] */ 0x4d492bce, 0x120269e1, // sub.setf -, r5, rb_lcount     ; mul24 r1, r1, ra_wt_mul_l1
/* [0x00002c30] */ 0xcc52706e, 0x1a024865, // add r1, r0, r1                ; v8adds r5rep, r5, ra_k1
/* [0x00002c38] */ 0x915c83f6, 0xdc024863, // shl r1, r1, 8                 ; mov r3, ra_blk_height
/* [0x00002c40] */ 0xec40c3f3, 0x12024860, // add r1, r1, rb_wt_off         ; v8subs r0, ra_height, r3
/* [0x00002c48] */ 0xffffff60, 0xf06809e7, // brr.anyn -, r:1b
/* [0x00002c50] */ 0x0f9d03c0, 0xd0020867, // asr r1, r1, (DENOM + 9) - 32
/* [0x00002c58] */ 0x925f23bf, 0x12020867, // min r1, r1, ra_pmax           ; mov -, vw_wait
/* [0x00002c60] */ 0x5351039f, 0x18024c22, // max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch
/* [0x00002c68] */ 0x956e7036, 0x10126431, // mov.setf ra_height, r0        ; mov vw_setup, ra_dma0
/* [0x00002c70] */ 0x00000000, 0xf027c9e7, // bra.anyz -, ra_link
/* [0x00002c78] */ 0x929dd0ff, 0x10024831, // min r0, r0, r3                ; mov vw_setup, rb_dma1
/* [0x00002c80] */ 0x8d7270f6, 0x10024872, // sub r1, r0, r3                ; mov vw_addr, ra_dest
/* [0x00002c88] */ 0x119d73c0, 0xd0020867, // shl r1, r1, i_shift23
/* [0x00002c90] */ 0xffffff18, 0xf0f809e7, // brr -, r:1b
/* [0x00002c98] */ 0x0c9d2e00, 0x100214a7, // add rb_lcount, rb_lcount, r0
/* [0x00002ca0] */ 0x0c6e7c40, 0x100206e7, // add ra_dma0, ra_dma0, r1
/* [0x00002ca8] */ 0x8c71ccbf, 0x10024731, // add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init
// ::mc_end
};
#ifdef __HIGHC__
#pragma Align_to(8, ff_hevc_rpi_shader)
#endif
