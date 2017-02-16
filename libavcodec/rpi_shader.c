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
// ::mc_setup_uv
/* [0x00000000] */ 0x15827d80, 0x100207e7, // mov ra31, unif
/* [0x00000008] */ 0x0c9a0f80, 0x10020427, // add ra_x, unif, elem_num
/* [0x00000010] */ 0x15827d80, 0x10020767, // mov ra_y, unif
/* [0x00000018] */ 0x15827d80, 0x10020627, // mov ra_frame_base, unif
/* [0x00000020] */ 0x009e7000, 0x100009e7, // nop
/* [0x00000028] */ 0x0d620f80, 0x10020667, // sub ra_u2v_ref_offset, unif, ra_frame_base
/* [0x00000030] */ 0x0d801dc0, 0xd0021667, // sub rb25,unif,1
/* [0x00000038] */ 0x0d801dc0, 0xd00217a7, // sub rb30,unif,1
/* [0x00000040] */ 0x15827d80, 0x10021427, // mov rb16, unif
/* [0x00000048] */ 0x15827d80, 0x10020827, // mov r0, unif
/* [0x00000050] */ 0xc0000000, 0xe0020867, // mov r1, vdw_setup_1(0)
/* [0x00000058] */ 0x0c9e7200, 0x10021627, // add rb24, r1, r0
/* [0x00000060] */ 0x00010000, 0xe0020127, // mov ra4, 0x10000
/* [0x00000068] */ 0x00000001, 0xe0020527, // mov ra_k1, 1
/* [0x00000070] */ 0x00000100, 0xe00205a7, // mov ra_k256, 256
/* [0x00000078] */ 0x00000040, 0xe00207a7, // mov ra30, 64
/* [0x00000080] */ 0xffffff00, 0xe0021527, // mov rb20, 0xffffff00
/* [0x00000088] */ 0x000000ff, 0xe00215a7, // mov rb_k255, 255
/* [0x00000090] */ 0x00000018, 0xe00215e7, // mov rb23, 24
/* [0x00000098] */ 0x00000000, 0xe0020227, // mov ra8, 0
/* [0x000000a0] */ 0x00000000, 0xe0020267, // mov ra9, 0
/* [0x000000a8] */ 0x00000000, 0xe00202a7, // mov ra10, 0
/* [0x000000b0] */ 0x00000000, 0xe00202e7, // mov ra11, 0
/* [0x000000b8] */ 0x00000000, 0xe0020327, // mov ra12, 0
/* [0x000000c0] */ 0x00000000, 0xe0020367, // mov ra13, 0
/* [0x000000c8] */ 0x00000000, 0xe00203a7, // mov ra14, 0
/* [0x000000d0] */ 0x00000000, 0xe00203e7, // mov ra15, 0
/* [0x000000d8] */ 0x15427d80, 0x10020827, // mov r0, ra_x
/* [0x000000e0] */ 0x937401f6, 0xd0024821, // max r0, r0, 0; mov r1, ra_y
/* [0x000000e8] */ 0x926191f6, 0x10024823, // min r0, r0, rb_frame_width_minus_1 ; mov r3, ra_frame_base
/* [0x000000f0] */ 0x916431f6, 0xd00244e2, // shl ra_xshift_next, r0, 3 ; mov r2, ra_u2v_ref_offset
/* [0x000000f8] */ 0x0c9c13c0, 0xd0020767, // add ra_y, r1, 1
/* [0x00000100] */ 0x0c9e70c0, 0x10020827, // add r0, r0, r3
/* [0x00000108] */ 0x149dc1c0, 0xd0020827, // and r0, r0, ~3
/* [0x00000110] */ 0x939c03c0, 0xd0025850, // max r1, r1, 0 ; mov ra_x, r0
/* [0x00000118] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_frame_height_minus_1
/* [0x00000120] */ 0x4c9d040f, 0x100248a1, // add r2, r2, r0 ; mul24 r1, r1, rb_pitch
/* [0x00000128] */ 0x8c9e7052, 0x10025e18, // add t0s, r0, r1 ; mov ra_frame_base, r2
/* [0x00000130] */ 0x0c9e7440, 0x10020f27, // add t1s, r2, r1
/* [0x00000138] */ 0x00000009, 0xe00208a7, // mov r2, 9
/* [0x00000140] */ 0x0c827580, 0x10021367, // add rb13, r2, unif
/* [0x00000148] */ 0x15827d80, 0x100009e7, // mov -, unif
/* [0x00000150] */ 0x15827d80, 0x100208a7, // mov r2, unif
/* [0x00000158] */ 0x119c15c0, 0xd00208a7, // shl r2, r2, 1
/* [0x00000160] */ 0x149cf5c0, 0xd00208a7, // and r2, r2, 15
/* [0x00000168] */ 0x159e7480, 0x10020867, // mov r1, r2
/* [0x00000170] */ 0x0f9c23c0, 0xd0020867, // asr r1, r1, 2
/* [0x00000178] */ 0x119c63c0, 0xd0020867, // shl r1, r1, 6
/* [0x00000180] */ 0x159e7480, 0x10020827, // mov r0, r2
/* [0x00000188] */ 0x149c31c0, 0xd0020827, // and r0, r0, 3
/* [0x00000190] */ 0x0c9e7040, 0x10020827, // add r0, r0, r1
/* [0x00000198] */ 0x00004800, 0xe0020867, // mov r1, vpm_setup(0, 4, h8p(0, 0))
/* [0x000001a0] */ 0x0c9e7040, 0x10021727, // add rb28, r0, r1
/* [0x000001a8] */ 0x0f9c11c0, 0xd00208a7, // asr r2, r0, 1
/* [0x000001b0] */ 0x00002900, 0xe0020867, // mov r1, vpm_setup(0, 2, h16p(0, 0))
/* [0x000001b8] */ 0x0c9e7440, 0x10021567, // add rb21, r2, r1
/* [0x000001c0] */ 0x80004004, 0xe0020867, // mov r1, vdw_setup_0(0, 0, dma_h8p(0,0,0))
/* [0x000001c8] */ 0x119c51c0, 0xd0020827, // shl r0, r0, 5
/* [0x000001d0] */ 0x0c9e7040, 0x100216e7, // add rb27, r0, r1
/* [0x000001d8] */ 0x13740dc0, 0xd0020867, // max r1, ra_y, 0
/* [0x000001e0] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_frame_height_minus_1
/* [0x000001e8] */ 0x0c741dc0, 0xd0020767, // add ra_y, ra_y, 1
/* [0x000001f0] */ 0x00000000, 0xf0f7e9e7, // bra -, ra31
/* [0x000001f8] */ 0x409d000f, 0x100049e1, // nop ; mul24 r1, r1, rb_pitch
/* [0x00000200] */ 0x0c427380, 0x10020e27, // add t0s, r1, ra_x
/* [0x00000208] */ 0x0c627380, 0x10020f27, // add t1s, r1, ra_frame_base
// ::mc_filter_uv
/* [0x00000210] */ 0x15827d80, 0x100207e7, // mov ra31, unif
/* [0x00000218] */ 0x0c9a0f80, 0x10020827, // add r0, unif, elem_num
/* [0x00000220] */ 0x938001f6, 0xd0024821, // max r0, r0, 0         ; mov r1, unif
/* [0x00000228] */ 0x928191f6, 0x10024823, // min r0, r0, rb_frame_width_minus_1 ; mov r3, unif
/* [0x00000230] */ 0x8d4e0ef6, 0x10025891, // sub r2, unif, r3      ; mov ra_xshift, ra_xshift_next
/* [0x00000238] */ 0x119c31c0, 0xd00204e7, // shl ra_xshift_next, r0, 3
/* [0x00000240] */ 0x0c9e70c0, 0x10020827, // add r0, r0, r3
/* [0x00000248] */ 0x149dc1c0, 0xd00214e7, // and rb_x_next, r0, ~3
/* [0x00000250] */ 0x159e7240, 0x10020727, // mov ra_y_next, r1
/* [0x00000258] */ 0x0c9d3e80, 0x100206a7, // add ra_frame_base_next, rb_x_next, r2
/* [0x00000260] */ 0x9581cff6, 0x10025c60, // mov vw_setup, rb28    ; mov r0, unif
/* [0x00000268] */ 0x0e9d01c0, 0xd0020867, // shr r1, r0, i_shift16
/* [0x00000270] */ 0x0d9d8e40, 0x10021767, // sub rb29, rb24, r1
/* [0x00000278] */ 0x149d61c0, 0x10020827, // and r0, r0, rb_k255
/* [0x00000280] */ 0x0c9c11c0, 0xd0021467, // add rb17, r0, 1
/* [0x00000288] */ 0x0c9c31c0, 0xd00214a7, // add rb18, r0, 3
/* [0x00000290] */ 0x119c71c0, 0xd0020827, // shl r0, r0, 7
/* [0x00000298] */ 0x0c9e7040, 0x10020827, // add r0, r0, r1
/* [0x000002a0] */ 0x119d01c0, 0xd0020827, // shl r0, r0, i_shift16
/* [0x000002a8] */ 0x0c9db1c0, 0x100216a7, // add rb26, r0, rb27
/* [0x000002b0] */ 0x95800dbf, 0xd0024823, // mov r0, unif          ; mov r3, 0
/* [0x000002b8] */ 0x4f5971c6, 0x100240e0, // asr ra3, r0, rb23;      mul24 r0, r0, ra_k256
/* [0x000002c0] */ 0x4f5971c6, 0x100240a0, // asr ra2, r0, rb23;      mul24 r0, r0, ra_k256
/* [0x000002c8] */ 0x4f5971c6, 0x10024060, // asr ra1, r0, rb23;      mul24 r0, r0, ra_k256
/* [0x000002d0] */ 0x8f8171f6, 0x10024020, // asr ra0, r0, rb23;      mov r0, unif
/* [0x000002d8] */ 0x4f5971c6, 0x100252e0, // asr rb11, r0, rb23;     mul24 r0, r0, ra_k256
/* [0x000002e0] */ 0x4f5971c6, 0x100252a0, // asr rb10, r0, rb23;     mul24 r0, r0, ra_k256
/* [0x000002e8] */ 0x4f5971c6, 0x10025260, // asr rb9, r0, rb23;      mul24 r0, r0, ra_k256
/* [0x000002f0] */ 0x0f9d71c0, 0x10021227, // asr rb8, r0, rb23
/* [0x000002f8] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x00000300] */ 0x15827d80, 0x10020827, // mov r0,      unif
/* [0x00000308] */ 0x15827d80, 0x10060827, // mov.ifnz r0, unif
/* [0x00000310] */ 0x4f1101c6, 0xd0024860, // asr r1, r0, i_shift16 ; mul24 r0, r0, ra4
/* [0x00000318] */ 0x119cd3c0, 0x10020867, // shl r1, r1, rb13
/* [0x00000320] */ 0x0f9c13c0, 0xd0021327, // asr rb12, r1, 1
/* [0x00000328] */ 0x0f9cf1c0, 0xd00213a7, // asr rb14, r0, 15
// :uvloop
/* [0x00000330] */ 0xcd5117de, 0xa00269e3, // sub.setf -, r3, rb17      ; v8adds r3, r3, ra_k1          ; ldtmu0
/* [0x00000338] */ 0x8e4539bf, 0xb0029810, // shr r0, r4, ra_xshift     ; mov.ifz ra_x, rb_x_next       ; ldtmu1
/* [0x00000340] */ 0x956a7d9b, 0x1004461f, // mov.ifz ra_frame_base, ra_frame_base_next ; mov rb31, r3
/* [0x00000348] */ 0x95710dbf, 0x10044763, // mov.ifz ra_y, ra_y_next   ; mov r3, rb_pitch
/* [0x00000350] */ 0xee454987, 0x10024860, // shr r1, r4, ra_xshift    ; v8subs r0, r0, rb20
/* [0x00000358] */ 0x13740dc0, 0xd00208a7, // max r2, ra_y, 0
/* [0x00000360] */ 0x129de5c0, 0x100208a7, // min r2, r2, rb_frame_height_minus_1
/* [0x00000368] */ 0x4c741dd3, 0xd0024762, // add ra_y, ra_y, 1         ; mul24 r2, r2, r3
/* [0x00000370] */ 0xec414c8f, 0x10024e21, // add t0s, ra_x, r2    ; v8subs r1, r1, rb20
/* [0x00000378] */ 0x0c627c80, 0x10020f27, // add t1s, ra_frame_base, r2
/* [0x00000380] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x00000388] */ 0x40027006, 0x100049e2, // nop                  ; mul24 r2, r0, ra0
/* [0x00000390] */ 0x40038031, 0xd000c9e2, // nop                  ; mul24.ifnz r2, ra0 << 8, r1 << 8
/* [0x00000398] */ 0x4007f030, 0xd00049e3, // nop                  ; mul24      r3, ra1 << 1, r0 << 1
/* [0x000003a0] */ 0x40077031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra1 << 9, r1 << 9
/* [0x000003a8] */ 0x4c0be4f0, 0xd00248a3, // add r2, r2, r3       ; mul24    r3, ra2 << 2, r0 << 2
/* [0x000003b0] */ 0x400b6031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra2 << 10, r1 << 10
/* [0x000003b8] */ 0x4c0fd4f0, 0xd00248a3, // add r2, r2, r3       ; mul24    r3, ra3 << 3, r0 << 3
/* [0x000003c0] */ 0x400f5031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra3 << 11, r1 << 11
/* [0x000003c8] */ 0x8c9df4ff, 0x10024823, // add r0, r2, r3       ; mov r3, rb31
/* [0x000003d0] */ 0x8d3447f6, 0xd00279cc, // sub.setf -, r3, 4    ; mov ra12, ra13
/* [0x000003d8] */ 0xffffff38, 0xf06809e7, // brr.anyn -, r:uvloop
/* [0x000003e0] */ 0x55389db7, 0x10024361, // mov ra13, ra14          ; mul24 r1, ra14, rb9
/* [0x000003e8] */ 0x153e7d80, 0x100203a7, // mov ra14, ra15
/* [0x000003f0] */ 0x55308037, 0x100243e0, // mov ra15, r0            ; mul24 r0, ra12, rb8
/* [0x000003f8] */ 0x4c38a237, 0x10024860, // add r1, r1, r0          ; mul24 r0, ra14, rb10
/* [0x00000400] */ 0x4c3cb237, 0x10024860, // add r1, r1, r0          ; mul24 r0, ra15, rb11
/* [0x00000408] */ 0x8c9f223f, 0x10020867, // add r1, r1, r0          ; mov -, vw_wait
/* [0x00000410] */ 0x4d5927ce, 0x100269e1, // sub.setf -, r3, rb18    ; mul24 r1, r1, ra_k256
/* [0x00000418] */ 0x0f9ce3c0, 0xd0020867, // asr r1, r1, 14
/* [0x00000420] */ 0x409ce00f, 0x100049e1, // nop                     ; mul24 r1, r1, rb14
/* [0x00000428] */ 0x119c83c0, 0xd0020867, // shl r1, r1, 8
/* [0x00000430] */ 0x0c9cc3c0, 0x10020867, // add r1, r1, rb12
/* [0x00000438] */ 0xfffffed8, 0xf06809e7, // brr.anyn -, r:uvloop
/* [0x00000440] */ 0x0f9cd3c0, 0x10020867, // asr r1, r1, rb13
/* [0x00000448] */ 0x129d63c0, 0x10020867, // min r1, r1, rb_k255
/* [0x00000450] */ 0x139c03c0, 0xd0020c27, // max vpm, r1, 0
/* [0x00000458] */ 0x159dafc0, 0x10021c67, // mov vw_setup, rb26
/* [0x00000460] */ 0x159ddfc0, 0x10021c67, // mov vw_setup, rb29
/* [0x00000468] */ 0x15827d80, 0x10021ca7, // mov vw_addr, unif
/* [0x00000470] */ 0x00000010, 0xe0020827, // mov r0, 16
/* [0x00000478] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000480] */ 0x00000000, 0xf0f7e9e7, // bra -, ra31
/* [0x00000488] */ 0x0c9dae00, 0x10021c67, // add vw_setup, rb26, r0
/* [0x00000490] */ 0x159ddfc0, 0x10021c67, // mov vw_setup, rb29
/* [0x00000498] */ 0x15827d80, 0x10021ca7, // mov vw_addr, unif
// ::mc_filter_uv_b0
/* [0x000004a0] */ 0x15827d80, 0x100207e7, // mov ra31, unif
/* [0x000004a8] */ 0x0c9a0f80, 0x10020827, // add r0, unif, elem_num
/* [0x000004b0] */ 0x938001f6, 0xd0024821, // max r0, r0, 0                ; mov r1, unif
/* [0x000004b8] */ 0x928191f6, 0x10024823, // min r0, r0, rb_frame_width_minus_1 ; mov r3, unif
/* [0x000004c0] */ 0x8d4e0ef6, 0x10025891, // sub r2, unif, r3             ; mov ra_xshift, ra_xshift_next
/* [0x000004c8] */ 0x119c31c0, 0xd00204e7, // shl ra_xshift_next, r0, 3
/* [0x000004d0] */ 0x0c9e70c0, 0x10020827, // add r0, r0, r3
/* [0x000004d8] */ 0x149dc1c0, 0xd00214e7, // and rb_x_next, r0, ~3
/* [0x000004e0] */ 0x159e7240, 0x10020727, // mov ra_y_next, r1
/* [0x000004e8] */ 0x0c9d3e80, 0x100206a7, // add ra_frame_base_next, rb_x_next, r2
/* [0x000004f0] */ 0x95815ff6, 0x10025c60, // mov vw_setup, rb21           ; mov r0, unif
/* [0x000004f8] */ 0x0e9d01c0, 0xd0020867, // shr r1, r0, i_shift16
/* [0x00000500] */ 0x0d9d8e40, 0x10021767, // sub rb29, rb24, r1
/* [0x00000508] */ 0x149d61c0, 0x10020827, // and r0, r0, rb_k255
/* [0x00000510] */ 0x0c9c11c0, 0xd0021467, // add rb17, r0, 1
/* [0x00000518] */ 0x0c9c31c0, 0xd00214a7, // add rb18, r0, 3
/* [0x00000520] */ 0x119c71c0, 0xd0020827, // shl r0, r0, 7
/* [0x00000528] */ 0x0c9e7040, 0x10020827, // add r0, r0, r1
/* [0x00000530] */ 0x119d01c0, 0xd0020827, // shl r0, r0, i_shift16
/* [0x00000538] */ 0x0c9db1c0, 0x100216a7, // add rb26, r0, rb27
/* [0x00000540] */ 0x95800dbf, 0xd0024823, // mov r0, unif          ; mov r3, 0
/* [0x00000548] */ 0x4f5971c6, 0x10224060, // asr ra1.16b, r0, rb23;      mul24 r0, r0, ra_k256
/* [0x00000550] */ 0x4f5971c6, 0x10124060, // asr ra1.16a, r0, rb23;      mul24 r0, r0, ra_k256
/* [0x00000558] */ 0x4f5971c6, 0x10224020, // asr ra0.16b, r0, rb23;      mul24 r0, r0, ra_k256
/* [0x00000560] */ 0x8f8171f6, 0x10124020, // asr ra0.16a, r0, rb23;      mov r0, unif
/* [0x00000568] */ 0x4f5971c6, 0x100252e0, // asr rb11, r0, rb23;     mul24 r0, r0, ra_k256
/* [0x00000570] */ 0x4f5971c6, 0x100252a0, // asr rb10, r0, rb23;     mul24 r0, r0, ra_k256
/* [0x00000578] */ 0x4f5971c6, 0x10025260, // asr rb9, r0, rb23;      mul24 r0, r0, ra_k256
/* [0x00000580] */ 0x0f9d71c0, 0x10021227, // asr rb8, r0, rb23
/* [0x00000588] */ 0xfffffff8, 0xe0021967, // mov r5rep, -8
/* [0x00000590] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x00000598] */ 0x15827d80, 0x100213a7, // mov      rb14, unif
/* [0x000005a0] */ 0x15827d80, 0x100613a7, // mov.ifnz rb14, unif
// :uvloop_b0
/* [0x000005a8] */ 0xcd5117de, 0xa00269e3, // sub.setf -, r3, rb17      ; v8adds r3, r3, ra_k1          ; ldtmu0
/* [0x000005b0] */ 0x8e4539bf, 0xb0029810, // shr r0, r4, ra_xshift     ; mov.ifz ra_x, rb_x_next       ; ldtmu1
/* [0x000005b8] */ 0x956a7d9b, 0x1004461f, // mov.ifz ra_frame_base, ra_frame_base_next ; mov rb31, r3
/* [0x000005c0] */ 0x95710dbf, 0x10044763, // mov.ifz ra_y, ra_y_next   ; mov r3, rb_pitch
/* [0x000005c8] */ 0xee454987, 0x10024860, // shr r1, r4, ra_xshift    ; v8subs r0, r0, rb20
/* [0x000005d0] */ 0x13740dc0, 0xd00208a7, // max r2, ra_y, 0
/* [0x000005d8] */ 0x129de5c0, 0x100208a7, // min r2, r2, rb_frame_height_minus_1
/* [0x000005e0] */ 0x4c741dd3, 0xd0024762, // add ra_y, ra_y, 1         ; mul24 r2, r2, r3
/* [0x000005e8] */ 0xec414c8f, 0x10024e21, // add t0s, ra_x, r2    ; v8subs r1, r1, rb20
/* [0x000005f0] */ 0x0c627c80, 0x10020f27, // add t1s, ra_frame_base, r2
/* [0x000005f8] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x00000600] */ 0x40027006, 0x120049e2, // nop                  ; mul24 r2, r0, ra0.16a
/* [0x00000608] */ 0x40038031, 0xd200c9e2, // nop                  ; mul24.ifnz r2, ra0.16a << 8, r1 << 8
/* [0x00000610] */ 0x4003f030, 0xd40049e3, // nop                  ; mul24      r3, ra0.16b << 1, r0 << 1
/* [0x00000618] */ 0x40037031, 0xd400c9e3, // nop                  ; mul24.ifnz r3, ra0.16b << 9, r1 << 9
/* [0x00000620] */ 0x4c07e4f0, 0xd20248a3, // add r2, r2, r3       ; mul24    r3, ra1.16a << 2, r0 << 2
/* [0x00000628] */ 0x40076031, 0xd200c9e3, // nop                  ; mul24.ifnz r3, ra1.16a << 10, r1 << 10
/* [0x00000630] */ 0x4c07d4f0, 0xd40248a3, // add r2, r2, r3       ; mul24    r3, ra1.16b << 3, r0 << 3
/* [0x00000638] */ 0x40075031, 0xd400c9e3, // nop                  ; mul24.ifnz r3, ra1.16b << 11, r1 << 11
/* [0x00000640] */ 0x8c9df4ff, 0x10024823, // add r0, r2, r3       ; mov r3, rb31
/* [0x00000648] */ 0x8d3447f6, 0xd00279cc, // sub.setf -, r3, 4    ; mov ra12, ra13
/* [0x00000650] */ 0xffffff38, 0xf06809e7, // brr.anyn -, r:uvloop_b0
/* [0x00000658] */ 0x55389db7, 0x10024361, // mov ra13, ra14          ; mul24 r1, ra14, rb9
/* [0x00000660] */ 0x153e7d80, 0x100203a7, // mov ra14, ra15
/* [0x00000668] */ 0x55308037, 0x100243e0, // mov ra15, r0            ; mul24 r0, ra12, rb8
/* [0x00000670] */ 0x4c38a237, 0x10024860, // add r1, r1, r0          ; mul24 r0, ra14, rb10
/* [0x00000678] */ 0x0d9d27c0, 0x100229e7, // sub.setf -, r3, rb18
/* [0x00000680] */ 0xffffff08, 0xf06809e7, // brr.anyn -, r:uvloop_b0
/* [0x00000688] */ 0x4c3cb237, 0x10024860, // add r1, r1, r0          ; mul24 r0, ra15, rb11
/* [0x00000690] */ 0x8c9f223f, 0x10020867, // add r1, r1, r0          ; mov -, vw_wait
/* [0x00000698] */ 0x0f9c63c0, 0xd0020c27, // asr vpm, r1, 6
/* [0x000006a0] */ 0x00000000, 0xf0f7e9e7, // bra -, ra31
/* [0x000006a8] */ 0x15827d80, 0x100009e7, // mov -, unif
/* [0x000006b0] */ 0x15827d80, 0x100009e7, // mov -, unif
/* [0x000006b8] */ 0x009e7000, 0x100009e7, // nop
// ::mc_filter_uv_b
/* [0x000006c0] */ 0x15827d80, 0x100207e7, // mov ra31, unif
/* [0x000006c8] */ 0x954dcdbf, 0x10024471, // mov ra_xshift, ra_xshift_next      ; mov vw_setup, rb28
/* [0x000006d0] */ 0x0c9a0f80, 0x10020827, // add r0, unif, elem_num
/* [0x000006d8] */ 0x938001f6, 0xd002581c, // max r0, r0, 0                      ; mov ra_y_next, unif
/* [0x000006e0] */ 0x928191f6, 0x10024823, // min r0, r0, rb_frame_width_minus_1 ; mov r3, unif
/* [0x000006e8] */ 0x4d808cc7, 0xd0025893, // sub r2, unif, r3                   ; mul24 ra_xshift_next, r0, 8
/* [0x000006f0] */ 0x0c9e70c0, 0x10020827, // add r0, r0, r3
/* [0x000006f8] */ 0x9481c1f6, 0xd00254e0, // and rb_x_next, r0, ~3              ; mov r0, unif
/* [0x00000700] */ 0x0e9d01c0, 0xd0020867, // shr r1, r0, i_shift16
/* [0x00000708] */ 0x0c9d3e80, 0x100206a7, // add ra_frame_base_next, rb_x_next, r2
/* [0x00000710] */ 0x0d9d8e40, 0x10021767, // sub rb29, rb24, r1
/* [0x00000718] */ 0x149d61c0, 0x10020827, // and r0, r0, rb_k255
/* [0x00000720] */ 0x0c9c11c0, 0xd0021467, // add rb17, r0, 1
/* [0x00000728] */ 0x0c9c31c0, 0xd00214a7, // add rb18, r0, 3
/* [0x00000730] */ 0x119c71c0, 0xd0020827, // shl r0, r0, 7
/* [0x00000738] */ 0x119d51c0, 0xd00208e7, // shl r3, r0, i_shift21
/* [0x00000740] */ 0x0e9c87c0, 0xd00208e7, // shr r3, r3, 8
/* [0x00000748] */ 0x0c9e7040, 0x10020827, // add r0, r0, r1
/* [0x00000750] */ 0x119d01c0, 0xd0020827, // shl r0, r0, i_shift16
/* [0x00000758] */ 0x0c9db1c0, 0x100216a7, // add rb26, r0, rb27
/* [0x00000760] */ 0x0c9d57c0, 0x10020c67, // add vr_setup, r3, rb21
/* [0x00000768] */ 0x95800dbf, 0xd0024823, // mov r0, unif          ; mov r3, 0
/* [0x00000770] */ 0x4f5971c6, 0x100240e0, // asr ra3, r0, rb23;      mul24 r0, r0, ra_k256
/* [0x00000778] */ 0x4f5971c6, 0x100240a0, // asr ra2, r0, rb23;      mul24 r0, r0, ra_k256
/* [0x00000780] */ 0x4f5971c6, 0x10024060, // asr ra1, r0, rb23;      mul24 r0, r0, ra_k256
/* [0x00000788] */ 0x8f8171f6, 0x10024020, // asr ra0, r0, rb23;      mov r0, unif
/* [0x00000790] */ 0x4f5971c6, 0x100252e0, // asr rb11, r0, rb23;     mul24 r0, r0, ra_k256
/* [0x00000798] */ 0x4f5971c6, 0x100252a0, // asr rb10, r0, rb23;     mul24 r0, r0, ra_k256
/* [0x000007a0] */ 0x4f5971c6, 0x10025260, // asr rb9, r0, rb23;      mul24 r0, r0, ra_k256
/* [0x000007a8] */ 0x0f9d71c0, 0x10021227, // asr rb8, r0, rb23
/* [0x000007b0] */ 0xfffffff8, 0xe0021967, // mov r5rep, -8
/* [0x000007b8] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x000007c0] */ 0x15827d80, 0x10020827, // mov      r0, unif
/* [0x000007c8] */ 0x15827d80, 0x10060827, // mov.ifnz r0, unif
/* [0x000007d0] */ 0x0f9d01c0, 0xd0020867, // asr r1, r0, i_shift16
/* [0x000007d8] */ 0x119cd3c0, 0x10020867, // shl r1, r1, rb13
/* [0x000007e0] */ 0x4f1013c6, 0xd0025320, // asr rb12, r1, 1           ; mul24 r0, r0, ra4
/* [0x000007e8] */ 0x0f9d01c0, 0xd00204a7, // asr ra18, r0, i_shift16
// :uvloop_b
/* [0x000007f0] */ 0xcd5117de, 0xa00269e3, // sub.setf -, r3, rb17      ; v8adds r3, r3, ra_k1          ; ldtmu0
/* [0x000007f8] */ 0x8e4539bf, 0xb0029810, // shr r0, r4, ra_xshift     ; mov.ifz ra_x, rb_x_next       ; ldtmu1
/* [0x00000800] */ 0x956a7d9b, 0x1004461f, // mov.ifz ra_frame_base, ra_frame_base_next ; mov rb31, r3
/* [0x00000808] */ 0x95710dbf, 0x10044763, // mov.ifz ra_y, ra_y_next   ; mov r3, rb_pitch
/* [0x00000810] */ 0xee454987, 0x10024860, // shr r1, r4, ra_xshift     ; v8subs r0, r0, rb20
/* [0x00000818] */ 0x13740dc0, 0xd00208a7, // max r2, ra_y, 0
/* [0x00000820] */ 0x129de5c0, 0x100208a7, // min r2, r2, rb_frame_height_minus_1
/* [0x00000828] */ 0x4c741dd3, 0xd0024762, // add ra_y, ra_y, 1         ; mul24 r2, r2, r3
/* [0x00000830] */ 0xec414c8f, 0x10024e21, // add t0s, ra_x, r2         ; v8subs r1, r1, rb20
/* [0x00000838] */ 0x0c627c80, 0x10020f27, // add t1s, ra_frame_base, r2
/* [0x00000840] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x00000848] */ 0x40027006, 0x100049e2, // nop                  ; mul24 r2, r0, ra0
/* [0x00000850] */ 0x40038031, 0xd000c9e2, // nop                  ; mul24.ifnz r2, ra0 << 8, r1 << 8
/* [0x00000858] */ 0x4007f030, 0xd00049e3, // nop                  ; mul24      r3, ra1 << 1, r0 << 1
/* [0x00000860] */ 0x40077031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra1 << 9, r1 << 9
/* [0x00000868] */ 0x4c0be4f0, 0xd00248a3, // add r2, r2, r3       ; mul24    r3, ra2 << 2, r0 << 2
/* [0x00000870] */ 0x400b6031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra2 << 10, r1 << 10
/* [0x00000878] */ 0x4c0fd4f0, 0xd00248a3, // add r2, r2, r3       ; mul24    r3, ra3 << 3, r0 << 3
/* [0x00000880] */ 0x400f5031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra3 << 11, r1 << 11
/* [0x00000888] */ 0x8c9df4ff, 0x10024823, // add r0, r2, r3       ; mov r3, rb31
/* [0x00000890] */ 0x8d3447f6, 0xd00279cc, // sub.setf -, r3, 4    ; mov ra12, ra13
/* [0x00000898] */ 0xffffff38, 0xf06809e7, // brr.anyn -, r:uvloop_b
/* [0x000008a0] */ 0x55389db7, 0x10024361, // mov ra13, ra14          ; mul24 r1, ra14, rb9
/* [0x000008a8] */ 0x153e7d80, 0x100203a7, // mov ra14, ra15
/* [0x000008b0] */ 0x55308037, 0x100243e0, // mov ra15, r0            ; mul24 r0, ra12, rb8
/* [0x000008b8] */ 0x4c38a237, 0x10024860, // add r1, r1, r0          ; mul24 r0, ra14, rb10
/* [0x000008c0] */ 0x4c3cb237, 0x10024860, // add r1, r1, r0          ; mul24 r0, ra15, rb11
/* [0x000008c8] */ 0x4c13023e, 0x10024860, // add r1, r1, r0          ; mul24 r0, vpm, ra4
/* [0x000008d0] */ 0x4d5927ce, 0x100269e1, // sub.setf -, r3, rb18    ; mul24 r1, r1, ra_k256
/* [0x000008d8] */ 0x0f9ce3c0, 0xd0020867, // asr r1, r1, 14
/* [0x000008e0] */ 0x4f4901ce, 0xd0024821, // asr r0, r0, i_shift16   ; mul24 r1, r1, ra18
/* [0x000008e8] */ 0x409ce007, 0x100049e0, // nop                     ; mul24 r0, r0, rb14
/* [0x000008f0] */ 0x8c9f223f, 0x10020867, // add r1, r1, r0          ; mov -, vw_wait
/* [0x000008f8] */ 0x119c83c0, 0xd0020867, // shl r1, r1, 8
/* [0x00000900] */ 0x0c9cc3c0, 0x10020867, // add r1, r1, rb12
/* [0x00000908] */ 0xfffffec8, 0xf06809e7, // brr.anyn -, r:uvloop_b
/* [0x00000910] */ 0x0f9cd3c0, 0x10020867, // asr r1, r1, rb13
/* [0x00000918] */ 0x129d63c0, 0x10020867, // min r1, r1, rb_k255
/* [0x00000920] */ 0x139c03c0, 0xd0020c27, // max vpm, r1, 0
/* [0x00000928] */ 0x159dafc0, 0x10021c67, // mov vw_setup, rb26
/* [0x00000930] */ 0x159ddfc0, 0x10021c67, // mov vw_setup, rb29
/* [0x00000938] */ 0x15827d80, 0x10021ca7, // mov vw_addr, unif
/* [0x00000940] */ 0x00000010, 0xe0020827, // mov r0, 16
/* [0x00000948] */ 0x159f2fc0, 0x100009e7, // mov -, vw_wait
/* [0x00000950] */ 0x00000000, 0xf0f7e9e7, // bra -, ra31
/* [0x00000958] */ 0x0c9dae00, 0x10021c67, // add vw_setup, rb26, r0
/* [0x00000960] */ 0x159ddfc0, 0x10021c67, // mov vw_setup, rb29
/* [0x00000968] */ 0x15827d80, 0x10021ca7, // mov vw_addr, unif
// ::mc_exit
/* [0x00000970] */ 0x159f2fc0, 0x100009e7, // mov  -, vw_wait
/* [0x00000978] */ 0x00000000, 0xe80009e7, // mov -,srel(0)
/* [0x00000980] */ 0x009e7000, 0xa00009e7, // ldtmu0
/* [0x00000988] */ 0x009e7000, 0xb00009e7, // ldtmu1
/* [0x00000990] */ 0x009e7000, 0xa00009e7, // ldtmu0
/* [0x00000998] */ 0x009e7000, 0xb00009e7, // ldtmu1
/* [0x000009a0] */ 0x009e7000, 0x300009e7, // nop        ; nop ; thrend
/* [0x000009a8] */ 0x009e7000, 0x100009e7, // nop        ; nop
/* [0x000009b0] */ 0x009e7000, 0x100009e7, // nop        ; nop
// ::mc_interrupt_exit8
/* [0x000009b8] */ 0x159f2fc0, 0x100009e7, // mov  -, vw_wait
/* [0x000009c0] */ 0x009e7000, 0xa00009e7, // ldtmu0
/* [0x000009c8] */ 0x009e7000, 0xb00009e7, // ldtmu1
/* [0x000009d0] */ 0x009e7000, 0xa00009e7, // ldtmu0
/* [0x000009d8] */ 0x009e7000, 0xb00009e7, // ldtmu1
/* [0x000009e0] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x000009e8] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x000009f0] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x000009f8] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x00000a00] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x00000a08] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x00000a10] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x00000a18] */ 0x009e7000, 0x300009e7, // nop        ; nop ; thrend
/* [0x00000a20] */ 0x00000001, 0xe00209a7, // mov interrupt, 1; nop
/* [0x00000a28] */ 0x009e7000, 0x100009e7, // nop        ; nop
// ::mc_setup
/* [0x00000a30] */ 0x00000010, 0xe00208e7, // mov r3, 16
/* [0x00000a38] */ 0x15827d80, 0x10020227, // mov ra8, unif
/* [0x00000a40] */ 0x15827d80, 0x10020267, // mov ra9, unif
/* [0x00000a48] */ 0x15827d80, 0x100202a7, // mov ra10, unif
/* [0x00000a50] */ 0x15827d80, 0x100202e7, // mov ra11, unif
/* [0x00000a58] */ 0x15827d80, 0x10020867, // mov r1, unif
/* [0x00000a60] */ 0x119e72c0, 0x10020827, // shl r0,r1,r3
/* [0x00000a68] */ 0x0f9e72c0, 0x10020867, // asr r1,r1,r3
/* [0x00000a70] */ 0x0f9e70c0, 0x10020827, // asr r0,r0,r3
/* [0x00000a78] */ 0x0d9c13c0, 0xd0021667, // sub rb_frame_width_minus_1,r1,1
/* [0x00000a80] */ 0x0d9c11c0, 0xd00217a7, // sub rb_frame_height_minus_1,r0,1
/* [0x00000a88] */ 0x15827d80, 0x10021427, // mov rb_pitch, unif
/* [0x00000a90] */ 0x15827d80, 0x10020827, // mov r0, unif
/* [0x00000a98] */ 0xc0000000, 0xe0020867, // mov r1, vdw_setup_1(0)
/* [0x00000aa0] */ 0x0c9e7200, 0x10021627, // add rb24, r1, r0
/* [0x00000aa8] */ 0x15227d80, 0x10020867, // mov r1, ra8
/* [0x00000ab0] */ 0x119e72c0, 0x10020827, // shl r0,r1,r3
/* [0x00000ab8] */ 0x0f9e72c0, 0x10020867, // asr r1,r1,r3
/* [0x00000ac0] */ 0x0f9e70c0, 0x10020827, // asr r0,r0,r3
/* [0x00000ac8] */ 0x0c9a7180, 0x10020827, // add r0, r0, elem_num
/* [0x00000ad0] */ 0x139c01c0, 0xd0020827, // max r0, r0, 0
/* [0x00000ad8] */ 0x922591f6, 0x10024822, // min r0, r0, rb_frame_width_minus_1 ; mov r2, ra9
/* [0x00000ae0] */ 0x119c31c0, 0xd00204e7, // shl ra_xshift_next, r0, 3
/* [0x00000ae8] */ 0x0c9c13c0, 0xd0020767, // add ra_y, r1, 1
/* [0x00000af0] */ 0x149dc1c0, 0xd0020827, // and r0, r0, ~3
/* [0x00000af8] */ 0x0c9e7400, 0x100208a7, // add r2, r2, r0
/* [0x00000b00] */ 0x139c03c0, 0xd0020867, // max r1, r1, 0
/* [0x00000b08] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_frame_height_minus_1
/* [0x00000b10] */ 0x409d000f, 0x100049e1, // nop             ; mul24 r1, r1, rb_pitch
/* [0x00000b18] */ 0x8c9e7452, 0x10025e18, // add t0s, r2, r1 ; mov ra_frame_base, r2
/* [0x00000b20] */ 0x152a7d80, 0x10020867, // mov r1, ra10
/* [0x00000b28] */ 0x119e72c0, 0x10020827, // shl r0,r1,r3
/* [0x00000b30] */ 0x0f9e72c0, 0x10020867, // asr r1,r1,r3
/* [0x00000b38] */ 0x0f9e70c0, 0x10020827, // asr r0,r0,r3
/* [0x00000b40] */ 0x0c9a7180, 0x10020827, // add r0, r0, elem_num
/* [0x00000b48] */ 0x139c01c0, 0xd0020827, // max r0, r0, 0
/* [0x00000b50] */ 0x922d91f6, 0x10024822, // min r0, r0, rb_frame_width_minus_1 ; mov r2, ra11
/* [0x00000b58] */ 0x119c31c0, 0xd0021067, // shl rx_xshift2_next, r0, 3
/* [0x00000b60] */ 0x0c9c13c0, 0xd0120567, // add ra_y2, r1, 1
/* [0x00000b68] */ 0x149dc1c0, 0xd0020827, // and r0, r0, ~3
/* [0x00000b70] */ 0x0c9e7400, 0x100208a7, // add r2, r2, r0
/* [0x00000b78] */ 0x139c03c0, 0xd0020867, // max r1, r1, 0
/* [0x00000b80] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_frame_height_minus_1
/* [0x00000b88] */ 0x409d000f, 0x100049e1, // nop             ; mul24 r1, r1, rb_pitch
/* [0x00000b90] */ 0x8c9e7452, 0x10025f19, // add t1s, r2, r1 ; mov ra_frame_base2, r2
/* [0x00000b98] */ 0x00000001, 0xe0020527, // mov ra_k1, 1
/* [0x00000ba0] */ 0x00000100, 0xe00205a7, // mov ra_k256, 256
/* [0x00000ba8] */ 0x00000040, 0xe00207a7, // mov ra30, 64
/* [0x00000bb0] */ 0xffffff00, 0xe0021527, // mov rb20, 0xffffff00
/* [0x00000bb8] */ 0x000000ff, 0xe00215a7, // mov rb_k255, 255
/* [0x00000bc0] */ 0x00000018, 0xe00215e7, // mov rb23, 24
/* [0x00000bc8] */ 0x00000000, 0xe0020227, // mov ra8, 0
/* [0x00000bd0] */ 0x00000000, 0xe0020267, // mov ra9, 0
/* [0x00000bd8] */ 0x00000000, 0xe00202a7, // mov ra10, 0
/* [0x00000be0] */ 0x00000000, 0xe00202e7, // mov ra11, 0
/* [0x00000be8] */ 0x00000000, 0xe0020327, // mov ra12, 0
/* [0x00000bf0] */ 0x00000000, 0xe0020367, // mov ra13, 0
/* [0x00000bf8] */ 0x00000000, 0xe00203a7, // mov ra14, 0
/* [0x00000c00] */ 0x00000000, 0xe00203e7, // mov ra15, 0
/* [0x00000c08] */ 0x159e6fc0, 0x100208a7, // mov r2, qpu_num
/* [0x00000c10] */ 0x159e7480, 0x10020867, // mov r1, r2
/* [0x00000c18] */ 0x0f9c23c0, 0xd0020867, // asr r1, r1, 2
/* [0x00000c20] */ 0x119c63c0, 0xd0020867, // shl r1, r1, 6
/* [0x00000c28] */ 0x159e7480, 0x10020827, // mov r0, r2
/* [0x00000c30] */ 0x149c31c0, 0xd0020827, // and r0, r0, 3
/* [0x00000c38] */ 0x0c9e7040, 0x10020827, // add r0, r0, r1
/* [0x00000c40] */ 0x00004800, 0xe0020867, // mov r1, vpm_setup(0, 4, h8p(0, 0))
/* [0x00000c48] */ 0x0c9e7040, 0x10021727, // add rb28, r0, r1
/* [0x00000c50] */ 0x80004004, 0xe0020867, // mov r1, vdw_setup_0(0, 0, dma_h8p(0,0,0))
/* [0x00000c58] */ 0x119c51c0, 0xd0020827, // shl r0, r0, 5
/* [0x00000c60] */ 0x0c9e7040, 0x100216e7, // add rb27, r0, r1
/* [0x00000c68] */ 0x0c809dc0, 0xd0021367, // add rb13, unif, 9
/* [0x00000c70] */ 0x15827d80, 0x100009e7, // mov -, unif
/* [0x00000c78] */ 0x13740dc0, 0xd0020867, // max r1, ra_y, 0
/* [0x00000c80] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_frame_height_minus_1
/* [0x00000c88] */ 0x0c741dc0, 0xd0020767, // add ra_y, ra_y, 1
/* [0x00000c90] */ 0x409d000f, 0x100049e1, // nop ; mul24 r1, r1, rb_pitch
/* [0x00000c98] */ 0x0c627380, 0x10020e27, // add t0s, r1, ra_frame_base
/* [0x00000ca0] */ 0x13540dc0, 0xd2020867, // max r1, ra_y2, 0
/* [0x00000ca8] */ 0x129de3c0, 0x10020867, // min r1, r1, rb_frame_height_minus_1
/* [0x00000cb0] */ 0x0c541dc0, 0xd2120567, // add ra_y2, ra_y2, 1
/* [0x00000cb8] */ 0x409d000f, 0x100049e1, // nop ; mul24 r1, r1, rb_pitch
/* [0x00000cc0] */ 0x0c667380, 0x10020f27, // add t1s, r1, ra_frame_base2
// :per_block_setup
/* [0x00000cc8] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x00000cd0] */ 0x15827d80, 0x100207e7, // mov ra31, unif
/* [0x00000cd8] */ 0x959a0ff6, 0x10024061, // mov ra1, unif  ; mov r1, elem_num
/* [0x00000ce0] */ 0x154e7d80, 0x10020467, // mov ra_xshift, ra_xshift_next
/* [0x00000ce8] */ 0x159c1fc0, 0x10021027, // mov rx_xshift2, rx_xshift2_next
/* [0x00000cf0] */ 0x0c067c40, 0x12020827, // add r0, ra1.16a, r1
/* [0x00000cf8] */ 0x139c01c0, 0xd0020827, // max r0, r0, 0
/* [0x00000d00] */ 0x928191f6, 0x10024822, // min r0, r0, rb_frame_width_minus_1 ; mov r2, unif
/* [0x00000d08] */ 0x119c31c0, 0xd00204e7, // shl ra_xshift_next, r0, 3
/* [0x00000d10] */ 0x15067d80, 0x14020727, // mov ra_y_next, ra1.16b
/* [0x00000d18] */ 0x9481c1f6, 0xd0025801, // and r0, r0, ~3                     ; mov ra1, unif
/* [0x00000d20] */ 0x0c9e7400, 0x100206a7, // add ra_frame_base_next, r2, r0
/* [0x00000d28] */ 0x0c067c40, 0x12020827, // add r0, ra1.16a, r1
/* [0x00000d30] */ 0x139c01c0, 0xd0020827, // max r0, r0, 0
/* [0x00000d38] */ 0x928191f6, 0x10024822, // min r0, r0, rb_frame_width_minus_1 ; mov r2, unif
/* [0x00000d40] */ 0x119c31c0, 0xd0021067, // shl rx_xshift2_next, r0, 3
/* [0x00000d48] */ 0x15067d80, 0x14220567, // mov ra_y2_next, ra1.16b
/* [0x00000d50] */ 0x149dc1c0, 0xd0020827, // and r0, r0, ~3
/* [0x00000d58] */ 0x0c9e7400, 0x100214e7, // add rx_frame_base2_next, r2, r0
/* [0x00000d60] */ 0x00000010, 0xe00208e7, // mov r3, 16
/* [0x00000d68] */ 0x15827d80, 0x10020067, // mov ra1, unif
/* [0x00000d70] */ 0x159dcfc0, 0x10021c67, // mov vw_setup, rb28
/* [0x00000d78] */ 0x0d058f80, 0x14021767, // sub rb29, rb24, ra1.16b
/* [0x00000d80] */ 0x0c045dc0, 0xd2021467, // add rb17, ra1.16a, 5
/* [0x00000d88] */ 0x0c047dc0, 0xd20214a7, // add rb18, ra1.16a, 7
/* [0x00000d90] */ 0x11047dc0, 0xd2020827, // shl r0,   ra1.16a, 7
/* [0x00000d98] */ 0x0c067180, 0x14020827, // add r0,   r0, ra1.16b
/* [0x00000da0] */ 0x119d01c0, 0xd0020827, // shl r0,   r0, i_shift16
/* [0x00000da8] */ 0x0c9db1c0, 0x100216a7, // add rb26, r0, rb27
/* [0x00000db0] */ 0x17827d80, 0x10020827, // not r0, unif
/* [0x00000db8] */ 0x119e70c0, 0x10040827, // shl.ifz r0, r0, r3
/* [0x00000dc0] */ 0x4f5971c6, 0x10024260, // asr ra9, r0, rb23;      mul24 r0, r0, ra_k256
/* [0x00000dc8] */ 0x0f9d71c0, 0x10020227, // asr ra8, r0, rb23
/* [0x00000dd0] */ 0x11243dc0, 0xd0020267, // shl ra9,ra9,3
/* [0x00000dd8] */ 0x11203dc0, 0xd0020227, // shl ra8,ra8,3
/* [0x00000de0] */ 0x00ffff00, 0xe0020867, // mov r1,0xffff00
/* [0x00000de8] */ 0x11227380, 0x10020827, // shl r0, r1, ra8
/* [0x00000df0] */ 0x0f9d71c0, 0x10020027, // asr ra0, r0, rb23
/* [0x00000df8] */ 0x11267380, 0x10020827, // shl r0, r1, ra9
/* [0x00000e00] */ 0x0f9d71c0, 0x10020327, // asr ra12, r0, rb23
/* [0x00000e08] */ 0x01040400, 0xe0020867, // mov r1,0x1040400
/* [0x00000e10] */ 0x11227380, 0x10020827, // shl r0, r1, ra8
/* [0x00000e18] */ 0x0f9d71c0, 0x10020067, // asr ra1, r0, rb23
/* [0x00000e20] */ 0x11267380, 0x10020827, // shl r0, r1, ra9
/* [0x00000e28] */ 0x0f9d71c0, 0x10020367, // asr ra13, r0, rb23
/* [0x00000e30] */ 0xfbf5f600, 0xe0020867, // mov r1,0xfbf5f600
/* [0x00000e38] */ 0x11227380, 0x10020827, // shl r0, r1, ra8
/* [0x00000e40] */ 0x0f9d71c0, 0x100200a7, // asr ra2, r0, rb23
/* [0x00000e48] */ 0x11267380, 0x10020827, // shl r0, r1, ra9
/* [0x00000e50] */ 0x0f9d71c0, 0x100203a7, // asr ra14, r0, rb23
/* [0x00000e58] */ 0x11283a40, 0xe0020867, // mov r1,0x11283a40
/* [0x00000e60] */ 0x11227380, 0x10020827, // shl r0, r1, ra8
/* [0x00000e68] */ 0x0f9d71c0, 0x100200e7, // asr ra3, r0, rb23
/* [0x00000e70] */ 0x11267380, 0x10020827, // shl r0, r1, ra9
/* [0x00000e78] */ 0x0f9d71c0, 0x100203e7, // asr ra15, r0, rb23
/* [0x00000e80] */ 0x3a281100, 0xe0020867, // mov r1,0x3a281100
/* [0x00000e88] */ 0x11227380, 0x10020827, // shl r0, r1, ra8
/* [0x00000e90] */ 0x0f9d71c0, 0x10020127, // asr ra4, r0, rb23
/* [0x00000e98] */ 0x11267380, 0x10020827, // shl r0, r1, ra9
/* [0x00000ea0] */ 0x0f9d71c0, 0x10021127, // asr rb4, r0, rb23
/* [0x00000ea8] */ 0xf6f5fb00, 0xe0020867, // mov r1,0xf6f5fb00
/* [0x00000eb0] */ 0x11227380, 0x10020827, // shl r0, r1, ra8
/* [0x00000eb8] */ 0x0f9d71c0, 0x10020167, // asr ra5, r0, rb23
/* [0x00000ec0] */ 0x11267380, 0x10020827, // shl r0, r1, ra9
/* [0x00000ec8] */ 0x0f9d71c0, 0x10021167, // asr rb5, r0, rb23
/* [0x00000ed0] */ 0x04040100, 0xe0020867, // mov r1,0x4040100
/* [0x00000ed8] */ 0x11227380, 0x10020827, // shl r0, r1, ra8
/* [0x00000ee0] */ 0x0f9d71c0, 0x100201a7, // asr ra6, r0, rb23
/* [0x00000ee8] */ 0x11267380, 0x10020827, // shl r0, r1, ra9
/* [0x00000ef0] */ 0x0f9d71c0, 0x100211a7, // asr rb6, r0, rb23
/* [0x00000ef8] */ 0xffff0000, 0xe0020867, // mov r1,0xffff0000
/* [0x00000f00] */ 0x11227380, 0x10020827, // shl r0, r1, ra8
/* [0x00000f08] */ 0x0f9d71c0, 0x100201e7, // asr ra7, r0, rb23
/* [0x00000f10] */ 0x11267380, 0x10020827, // shl r0, r1, ra9
/* [0x00000f18] */ 0x0f9d71c0, 0x100211e7, // asr rb7, r0, rb23
/* [0x00000f20] */ 0x95800dbf, 0xd0024823, // mov r0, unif ; mov r3, 0
/* [0x00000f28] */ 0x1180ddc0, 0x10020867, // shl r1, unif, rb13
/* [0x00000f30] */ 0x00000000, 0xf0f7e9e7, // bra -, ra31
/* [0x00000f38] */ 0x0f9d01c0, 0xd00204a7, // asr ra18, r0, i_shift16
/* [0x00000f40] */ 0x119d01c0, 0xd0020827, // shl r0, r0, i_shift16
/* [0x00000f48] */ 0x0f9c93c0, 0xd0021327, // asr rb12, r1, 9
// ::mc_filter
/* [0x00000f50] */ 0x0f9cf1c0, 0xd00213a7, // asr rb14, r0, 15
// :yloop
/* [0x00000f58] */ 0xcd5117de, 0xa00269e3, // sub.setf -, r3, rb17      ; v8adds r3, r3, ra_k1                           ; ldtmu0
/* [0x00000f60] */ 0x8e4539bf, 0xb0029819, // shr r0, r4, ra_xshift     ; mov.ifz ra_frame_base2, rx_frame_base2_next    ; ldtmu1
/* [0x00000f68] */ 0x956a7d9b, 0x1004461f, // mov.ifz ra_frame_base, ra_frame_base_next ; mov rb31, r3
/* [0x00000f70] */ 0x95710dbf, 0x10044763, // mov.ifz ra_y, ra_y_next   ; mov r3, rb_pitch
/* [0x00000f78] */ 0x8e5409f6, 0x14129855, // shr r1, r4, rx_xshift2    ; mov.ifz ra_y2, ra_y2_next
/* [0x00000f80] */ 0x13740dc0, 0xd00208a7, // max r2, ra_y, 0
/* [0x00000f88] */ 0x129de5c0, 0x100208a7, // min r2, r2, rb_frame_height_minus_1
/* [0x00000f90] */ 0x4c741dd3, 0xd0024762, // add ra_y, ra_y, 1            ; mul24 r2, r2, r3
/* [0x00000f98] */ 0xec614c87, 0x10024e20, // add t0s, ra_frame_base, r2   ; v8subs r0, r0, rb20
/* [0x00000fa0] */ 0x13540dc0, 0xd20208a7, // max r2, ra_y2, 0
/* [0x00000fa8] */ 0x129de5c0, 0x100208a7, // min r2, r2, rb_frame_height_minus_1
/* [0x00000fb0] */ 0x4c541dd3, 0xd2124562, // add ra_y2, ra_y2, 1          ; mul24 r2, r2, r3
/* [0x00000fb8] */ 0xec654c8f, 0x10024f21, // add t1s, ra_frame_base2, r2  ; v8subs r1, r1, rb20
/* [0x00000fc0] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x00000fc8] */ 0x40027006, 0x100049e2, // nop                  ; mul24 r2, r0, ra0
/* [0x00000fd0] */ 0x40038031, 0xd000c9e2, // nop                  ; mul24.ifnz r2, ra0 << 8, r1 << 8
/* [0x00000fd8] */ 0x4007f030, 0xd00049e3, // nop                  ; mul24      r3, ra1 << 1, r0 << 1
/* [0x00000fe0] */ 0x40077031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra1 << 9, r1 << 9
/* [0x00000fe8] */ 0x4c0be4f0, 0xd00248a3, // add r2, r2, r3       ; mul24    r3, ra2 << 2, r0 << 2
/* [0x00000ff0] */ 0x400b6031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra2 << 10, r1 << 10
/* [0x00000ff8] */ 0x4c0fd4f0, 0xd00248a3, // add r2, r2, r3       ; mul24    r3, ra3 << 3, r0 << 3
/* [0x00001000] */ 0x400f5031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra3 << 11, r1 << 11
/* [0x00001008] */ 0x4c13c4f0, 0xd00248a3, // add r2, r2, r3       ; mul24    r3, ra4 << 4, r0 << 4
/* [0x00001010] */ 0x40134031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra4 << 12, r1 << 12
/* [0x00001018] */ 0x4c17b4f0, 0xd00248a3, // add r2, r2, r3       ; mul24    r3, ra5 << 5, r0 << 5
/* [0x00001020] */ 0x40173031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra5 << 13, r1 << 13
/* [0x00001028] */ 0x4c1ba4f0, 0xd00248a3, // add r2, r2, r3       ; mul24    r3, ra6 << 6, r0 << 6
/* [0x00001030] */ 0x401b2031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra6 << 14, r1 << 14
/* [0x00001038] */ 0x4c1f94f0, 0xd00248a3, // add r2, r2, r3       ; mul24    r3, ra7 << 7, r0 << 7
/* [0x00001040] */ 0x401f1031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra7 << 15, r1 << 15
/* [0x00001048] */ 0x8c9df4ff, 0x10024823, // add r0, r2, r3       ; mov r3, rb31
/* [0x00001050] */ 0x8d2087f6, 0xd00269e1, // sub.setf -, r3, 8       ; mov r1,   ra8
/* [0x00001058] */ 0x95249dbf, 0x10024208, // mov ra8,  ra9           ; mov rb8,  rb9
/* [0x00001060] */ 0xfffffed8, 0xf06809e7, // brr.anyn -, r:yloop
/* [0x00001068] */ 0x9528adbf, 0x10024249, // mov ra9,  ra10          ; mov rb9,  rb10
/* [0x00001070] */ 0x952cbdbf, 0x1002428a, // mov ra10, ra11          ; mov rb10, rb11
/* [0x00001078] */ 0x959e7009, 0x100242cb, // mov ra11, r0            ; mov rb11, r1
/* [0x00001080] */ 0x4030803e, 0x100049e1, // nop                     ; mul24 r1, rb8,  ra12
/* [0x00001088] */ 0x4034903e, 0x100049e0, // nop                     ; mul24 r0, rb9,  ra13
/* [0x00001090] */ 0x4c38a23e, 0x10024860, // add r1, r1, r0          ; mul24 r0, rb10, ra14
/* [0x00001098] */ 0x4c3cb23e, 0x10024860, // add r1, r1, r0          ; mul24 r0, rb11, ra15
/* [0x000010a0] */ 0x4c204237, 0x10024860, // add r1, r1, r0          ; mul24 r0, ra8,  rb4
/* [0x000010a8] */ 0x4c245237, 0x10024860, // add r1, r1, r0          ; mul24 r0, ra9,  rb5
/* [0x000010b0] */ 0x4c286237, 0x10024860, // add r1, r1, r0          ; mul24 r0, ra10, rb6
/* [0x000010b8] */ 0x4c2c7237, 0x10024860, // add r1, r1, r0          ; mul24 r0, ra11, rb7
/* [0x000010c0] */ 0x8c9f223f, 0x10020867, // add r1, r1, r0          ; mov -, vw_wait
/* [0x000010c8] */ 0x4d5927ce, 0x100269e1, // sub.setf -, r3, rb18    ; mul24 r1, r1, ra_k256
/* [0x000010d0] */ 0x0f9ce3c0, 0xd0020867, // asr r1, r1, 14
/* [0x000010d8] */ 0x409ce00f, 0x100049e1, // nop                     ; mul24 r1, r1, rb14
/* [0x000010e0] */ 0x0c9cc3c0, 0x10020867, // add r1, r1, rb12
/* [0x000010e8] */ 0x119c83c0, 0xd0020867, // shl r1, r1, 8
/* [0x000010f0] */ 0xfffffe48, 0xf06809e7, // brr.anyn -, r:yloop
/* [0x000010f8] */ 0x0f9cd3c0, 0x10020867, // asr r1, r1, rb13
/* [0x00001100] */ 0x129d63c0, 0x10020867, // min r1, r1, rb_k255
/* [0x00001108] */ 0x139c03c0, 0xd0020c27, // max vpm, r1, 0
/* [0x00001110] */ 0xfffffb98, 0xf0f809e7, // brr -, r:per_block_setup
/* [0x00001118] */ 0x159dafc0, 0x10021c67, // mov vw_setup, rb26
/* [0x00001120] */ 0x159ddfc0, 0x10021c67, // mov vw_setup, rb29
/* [0x00001128] */ 0x15827d80, 0x10021ca7, // mov vw_addr, unif
// ::mc_filter_b
/* [0x00001130] */ 0x0f9d01c0, 0xd00213a7, // asr rb14, r0, i_shift16
// :yloopb
/* [0x00001138] */ 0xcd5117de, 0xa00269e3, // sub.setf -, r3, rb17      ; v8adds r3, r3, ra_k1                           ; ldtmu0
/* [0x00001140] */ 0x8e4539bf, 0xb0029819, // shr r0, r4, ra_xshift     ; mov.ifz ra_frame_base2, rx_frame_base2_next    ; ldtmu1
/* [0x00001148] */ 0x956a7d9b, 0x1004461f, // mov.ifz ra_frame_base, ra_frame_base_next ; mov rb31, r3
/* [0x00001150] */ 0x95710dbf, 0x10044763, // mov.ifz ra_y, ra_y_next   ; mov r3, rb_pitch
/* [0x00001158] */ 0x8e5409f6, 0x14129855, // shr r1, r4, rx_xshift2    ; mov.ifz ra_y2, ra_y2_next
/* [0x00001160] */ 0x13740dc0, 0xd00208a7, // max r2, ra_y, 0
/* [0x00001168] */ 0x129de5c0, 0x100208a7, // min r2, r2, rb_frame_height_minus_1
/* [0x00001170] */ 0x4c741dd3, 0xd0024762, // add ra_y, ra_y, 1            ; mul24 r2, r2, r3
/* [0x00001178] */ 0xec614c87, 0x10024e20, // add t0s, ra_frame_base, r2   ; v8subs r0, r0, rb20
/* [0x00001180] */ 0x13540dc0, 0xd20208a7, // max r2, ra_y2, 0
/* [0x00001188] */ 0x129de5c0, 0x100208a7, // min r2, r2, rb_frame_height_minus_1
/* [0x00001190] */ 0x4c541dd3, 0xd2124562, // add ra_y2, ra_y2, 1          ; mul24 r2, r2, r3
/* [0x00001198] */ 0xec654c8f, 0x10024f21, // add t1s, ra_frame_base2, r2  ; v8subs r1, r1, rb20
/* [0x000011a0] */ 0x0000ff00, 0xe20229e7, // mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
/* [0x000011a8] */ 0x40027006, 0x100049e2, // nop                  ; mul24 r2, r0, ra0
/* [0x000011b0] */ 0x40038031, 0xd000c9e2, // nop                  ; mul24.ifnz r2, ra0 << 8, r1 << 8
/* [0x000011b8] */ 0x4007f030, 0xd00049e3, // nop                  ; mul24      r3, ra1 << 1, r0 << 1
/* [0x000011c0] */ 0x40077031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra1 << 9, r1 << 9
/* [0x000011c8] */ 0x4c0be4f0, 0xd00248a3, // add r2, r2, r3       ; mul24    r3, ra2 << 2, r0 << 2
/* [0x000011d0] */ 0x400b6031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra2 << 10, r1 << 10
/* [0x000011d8] */ 0x4c0fd4f0, 0xd00248a3, // add r2, r2, r3       ; mul24    r3, ra3 << 3, r0 << 3
/* [0x000011e0] */ 0x400f5031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra3 << 11, r1 << 11
/* [0x000011e8] */ 0x4c13c4f0, 0xd00248a3, // add r2, r2, r3       ; mul24    r3, ra4 << 4, r0 << 4
/* [0x000011f0] */ 0x40134031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra4 << 12, r1 << 12
/* [0x000011f8] */ 0x4c17b4f0, 0xd00248a3, // add r2, r2, r3       ; mul24    r3, ra5 << 5, r0 << 5
/* [0x00001200] */ 0x40173031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra5 << 13, r1 << 13
/* [0x00001208] */ 0x4c1ba4f0, 0xd00248a3, // add r2, r2, r3       ; mul24    r3, ra6 << 6, r0 << 6
/* [0x00001210] */ 0x401b2031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra6 << 14, r1 << 14
/* [0x00001218] */ 0x4c1f94f0, 0xd00248a3, // add r2, r2, r3       ; mul24    r3, ra7 << 7, r0 << 7
/* [0x00001220] */ 0x401f1031, 0xd000c9e3, // nop                  ; mul24.ifnz r3, ra7 << 15, r1 << 15
/* [0x00001228] */ 0x8c9df4ff, 0x10024823, // add r0, r2, r3       ; mov r3, rb31
/* [0x00001230] */ 0x8d2087f6, 0xd00269e1, // sub.setf -, r3, 8       ; mov r1,   ra8
/* [0x00001238] */ 0x95249dbf, 0x10024208, // mov ra8,  ra9           ; mov rb8,  rb9
/* [0x00001240] */ 0xfffffed8, 0xf06809e7, // brr.anyn -, r:yloopb
/* [0x00001248] */ 0x9528adbf, 0x10024249, // mov ra9,  ra10          ; mov rb9,  rb10
/* [0x00001250] */ 0x952cbdbf, 0x1002428a, // mov ra10, ra11          ; mov rb10, rb11
/* [0x00001258] */ 0x959e7009, 0x100242cb, // mov ra11, r0            ; mov rb11, r1
/* [0x00001260] */ 0x4030803e, 0x100049e1, // nop                     ; mul24 r1, rb8,  ra12
/* [0x00001268] */ 0x4034903e, 0x100049e0, // nop                     ; mul24 r0, rb9,  ra13
/* [0x00001270] */ 0x4c38a23e, 0x10024860, // add r1, r1, r0          ; mul24 r0, rb10, ra14
/* [0x00001278] */ 0x4c3cb23e, 0x10024860, // add r1, r1, r0          ; mul24 r0, rb11, ra15
/* [0x00001280] */ 0x4c204237, 0x10024860, // add r1, r1, r0          ; mul24 r0, ra8,  rb4
/* [0x00001288] */ 0x4c245237, 0x10024860, // add r1, r1, r0          ; mul24 r0, ra9,  rb5
/* [0x00001290] */ 0x4c286237, 0x10024860, // add r1, r1, r0          ; mul24 r0, ra10, rb6
/* [0x00001298] */ 0x4c2c7237, 0x10024860, // add r1, r1, r0          ; mul24 r0, ra11, rb7
/* [0x000012a0] */ 0x8c9cc23f, 0x10024862, // add r1, r1, r0          ; mov r2, rb12
/* [0x000012a8] */ 0x4d5927ce, 0x100269e1, // sub.setf -, r3, rb18    ; mul24 r1, r1, ra_k256
/* [0x000012b0] */ 0x0f9ce3c0, 0xd0020867, // asr r1, r1, 14
/* [0x000012b8] */ 0x409ce00f, 0x100049e0, // nop                     ; mul24 r0, r1, rb14
/* [0x000012c0] */ 0x4c4b808e, 0xd0024821, // add r0, r0, r2          ; mul24 r1, r1 << 8, ra18 << 8
/* [0x000012c8] */ 0x8c9f223f, 0x10020867, // add r1, r1, r0          ; mov -, vw_wait
/* [0x000012d0] */ 0x119c83c0, 0xd0020867, // shl r1, r1, 8
/* [0x000012d8] */ 0xfffffe40, 0xf06809e7, // brr.anyn -, r:yloopb
/* [0x000012e0] */ 0x0f9cd3c0, 0x10020867, // asr r1, r1, rb13
/* [0x000012e8] */ 0x129d63c0, 0x10020867, // min r1, r1, rb_k255
/* [0x000012f0] */ 0x139c03c0, 0xd0020c27, // max vpm, r1, 0
/* [0x000012f8] */ 0xfffff9b0, 0xf0f809e7, // brr -, r:per_block_setup
/* [0x00001300] */ 0x159dafc0, 0x10021c67, // mov vw_setup, rb26
/* [0x00001308] */ 0x159ddfc0, 0x10021c67, // mov vw_setup, rb29
/* [0x00001310] */ 0x15827d80, 0x10021ca7, // mov vw_addr, unif
// ::mc_interrupt_exit12
/* [0x00001318] */ 0x159f2fc0, 0x100009e7, // mov  -, vw_wait
/* [0x00001320] */ 0x009e7000, 0xa00009e7, // ldtmu0
/* [0x00001328] */ 0x009e7000, 0xa00009e7, // ldtmu0
/* [0x00001330] */ 0x009e7000, 0xb00009e7, // ldtmu1
/* [0x00001338] */ 0x009e7000, 0xb00009e7, // ldtmu1
/* [0x00001340] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x00001348] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x00001350] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x00001358] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x00001360] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x00001368] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x00001370] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x00001378] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x00001380] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x00001388] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x00001390] */ 0x00000010, 0xe80009e7, // mov -,sacq(0)
/* [0x00001398] */ 0x009e7000, 0x300009e7, // nop        ; nop ; thrend
/* [0x000013a0] */ 0x00000001, 0xe00209a7, // mov interrupt, 1; nop
/* [0x000013a8] */ 0x009e7000, 0x100009e7, // nop        ; nop
// ::mc_exit1
/* [0x000013b0] */ 0x159f2fc0, 0x100009e7, // mov  -, vw_wait
/* [0x000013b8] */ 0x009e7000, 0xa00009e7, // ldtmu0
/* [0x000013c0] */ 0x009e7000, 0xb00009e7, // ldtmu1
/* [0x000013c8] */ 0x009e7000, 0xa00009e7, // ldtmu0
/* [0x000013d0] */ 0x009e7000, 0xb00009e7, // ldtmu1
/* [0x000013d8] */ 0x009e7000, 0x300009e7, // nop        ; nop ; thrend
/* [0x000013e0] */ 0x00000001, 0xe00209a7, // mov interrupt, 1; nop
/* [0x000013e8] */ 0x009e7000, 0x100009e7, // nop        ; nop
// ::mc_end
};
#ifdef __HIGHC__
#pragma Align_to(8, rpi_shader)
#endif
