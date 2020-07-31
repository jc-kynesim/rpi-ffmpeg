# Copyright (c) 2017 Raspberry Pi (Trading) Ltd.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the copyright holder nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Written by Peter de Rivaz, John Cox



# Inter pred asm
#
# Logic here should be good to 14 bits without modification
# but only 8 & 10 are currently instantiated & tested
# 15 & 16 bits have different shift1, shift2 calc & I also suspect overflow
# in _p00 & _b00

# The @ "mul_used", 0 annotations that occur by various mul blocks suppress
# the warning that we are using rotation & ra/rb registers. r0..3 can be
# rotated through all 16 elems ra regs can only be rotated through their
# local 4.  As it happens this is what is wanted here as we do not want the
# constants from the other half of the calc.

# Number limits in P/B calculation
#
# In order to avoid issues with mul24 being an unsigned 24->32 bit multiplier
# we offset our intermediates s.t. they always end up +ve before the next
# multiply (may be -ve whilst summing but that doesn't matter).
#
# Range calc for up to 14 bits (Y-B pred):
#
# denom: [0, 7]
# bmax = (1 << bits) - 1
# off: [-(1 << (bits-1)), (1 << (bits-1)) - 1]
#
# wt_mul: [-128, 255]
# wt_off = off * 2 + 1: [-bmax, bmax]
#
# pel: [0, bmax]
# H-filter: [(-22*pel + 88*pel) >> (bits-8) + 0x4000] = [0x2a00, 0x97ff]
# V-filter: [(-22*hf + 88*hf) >> 6] = [0x580, 0xc28e]
# mul_t = (V_L0 + V_l1) * (wt_mul + 128): [0, 0x24624e6]
# mul_t - (V_l0 + V_l1)* 128: [-0xc28e00, 0x18396e4]
# adj_wt_off = (wt_off << ((denom + 6) - (bits - 8))) - 0x4000 * (wt_mul * 2):
#  [wt_off << (21 - bits)] - [wt_mul << 15] = [-0x1fffff, 0x1fffff] - [-0x400000, 0x7f8000]
#
# This all looks good and is mostly bit depth independant - and as we manage
# to do unsigned multiplies everywhere (now) this should be good for any bit
# depth up to 14 (we could probably do 16 - but that requires a few tweaks
# to the shifts we don't currently have logic for)

# PREREAD is the number of requests that we have sitting in the TMU request
# queue.
#
# There are 8 slots availible in the TMU request Q for tm0s requests, but
# only 4 output FIFO entries and overflow is bad (corruption or crash)
# (If threaded then only 2 out FIFO entries, but we aren't.)
# In s/w we are effectively limited to the min vertical read which is >= 4
# so output FIFO is the limit.
#
# As the test for read-next is is the main part of the Luma loop (rather than
# the preload FIFO part) we are limited to min_luma_height - 1
# Min_luma_height is 4 so we can only have a preload of 3
# Beware that min_chroma_height (and_width) is 2 so we can't do the same trick
# in chroma without abandoning preload pretty much entirely (which would be bad)
#
# Timing tests vs preload of 4 suggests this doesn't hurt us much
# Could have preread 4 for Chroma but when tested it didn't help

.set PREREAD,                      3

# Offset added (effectively) at the exit of the H FIR filter
# This is enough to force the result +ve
# Is good if it is a power of 2 as that allows for >> without loss
#
# Worst case for a single Y FIR is *-22 so we need an offset of 256*22
# But we need twice offset to survive both H & V = 256*22*2 = 0x2c00
# Round up to next power of 2

.set FIR_OFFSET,                   0x4000

# Block heights - 8 & 16 are the only numbers we currently support

.set C_BLK_HEIGHT_8,               16
.set C_BLK_HEIGHT_16,              8
.set Y_BLK_HEIGHT_8,               16
.set Y_BLK_HEIGHT_16,              8

# QPU counts - depend on block size
# If we have a 2-byte format & block_size > 8 then can only afford
# 8 QPUs
# These numbers must match the numbers in ff_hevc_rpi_shader_cmd.h

.set N_QPU_8,                      12
.set N_QPU_16,                     12

# Value to add to the weight multiplier to convert it into an unsigned value
# Should be power of two for convienience

.set LOG2_MUL_ADD,                 14
.set MUL_ADD,                      (1 << LOG2_MUL_ADD)

# Fixed denom (max that it can be set to)
.set DENOM,                        7

# register allocation
#

# ra0-3
# Used as temp and may be loop filter coeffs (split into .8s)
# or temp in loop. Check usage on an individual basis.

# ra4-11
# V FIFO / temp / free

# -- free --                       ra12

# -- free --                       ra13

# -- free --                       ra14

# -- free --                       ra15

# uniform: width:height
.set ra_width_height,              ra16
.set ra_width,                     ra16.16b
.set ra_height,                    ra16.16a

# y:y2 same layout as y_y2_next so we can update both together
.set ra_y_y2,                      ra17
.set ra_y2,                        ra17.16a
.set ra_y,                         ra17.16b

# uniform: L1 weight (U on left, V on right)
# Only used in Y B
.set ra_wt_off_mul_l1,             ra18
.set ra_wt_off_l1,                 ra18.16b
.set ra_wt_mul_l1,                 ra18.16a

# y_next:y2_next same layout as y_y2 so we can update both together
.set ra_y_y2_next,                 ra19
.set ra_y_next,                    ra19.16b
.set ra_y2_next,                   ra19.16a

# Setup: consts - subdivide a single register
.set ra_kff800100,                 ra20
.set ra_k256,                      ra20.16a
.set ra_k0,                        ra20.8a
.set ra_k1,                        ra20.8b
.set ra_k128,                      ra20.8c
.set ra_k255,                      ra20.8d

# Loop: xshifts
.set ra_xshift,                    ra21.16a
.set ra_xshift_next,               ra21.16b

# Loop var: L0 weight (U on left, V on right)
# _off_ is not used in loop as we want to modify it before use
.set ra_wt_off_mul_l0,             ra22
.set ra_wt_mul_l0,                 ra22.16a
.set ra_wt_off_l0,                 ra22.16b

# Max pel value (for 8 bit we can get away with sat ops but not 9+)
# * Could merge with rb_pmask. For 10 bit Logically pmask needs 0xff in the
#   2nd byte   but as the source should never be > 3 there 0x3ff should do
.set ra_blk_height_pmax,           ra23
.set ra_pmax,                      ra23.16a
.set ra_blk_height,                ra23.8c
# --free --                        ra23.8d

# Loop:  src frame base (L0)
.set ra_base,                      ra24

# Misc  offsets
.set ra_fir_off_val_wt_den_p7,     ra25
.set ra_wt_den_p7,                 ra25.8a
# -- free --                       ra25.8b
.set ra_fir_off_val,               ra25.16b

# As it happens these constants are the same
.if FIR_OFFSET == MUL_ADD
# Weight multiplier unsigned add
.set ra_kmul_add,                  ra_fir_off_val
.else
.error "FIR_OFFSET != MUL_ADD: Need new register & init"
.endif

# Loop: next src frame base (L0)
.set ra_base_next,                 ra26

# Loop: height<<23 + width<<16 + vdw_setup_0
.set ra_dma0,                      ra27

# Loop: destination address
.set ra_dest,                      ra28

# Setup: Dup of rb_ef
# Lo bits are used as Y coeff 0 as that lefts us combine test & coeff mul
# (top bits are ignored by mul24)
.set ra_ef,                        ra29

# Use an even numbered register as a link register to avoid corrupting flags
.set ra_link,                      ra30

# -- free --                       ra31

.set rb_xshift2,                   rb0
.set rb_xshift2_next,              rb1

# C:  (elem & 1) == 0 ? elem * 2 : (elem + 4) * 2
.set rb_elem_x,                    rb2

# El Flags
# After adding to self we to have el even/odd on nc/c and lo/hi on nn/n
# Duped into ra_ef as sometimes that is easier to use
.set rb_ef,                        rb3

# rb4-11
# Loop: V filter FIFO or V filter coeff

# Loop var: offset to add before shift (round + weighting offsets)
# Exact value varies by loop
.set rb_wt_off,                    rb12

# -- free --                       rb13

# -- free --                       rb14

# Loop: src frame base (L1)
.set rb_base2,                     rb15

# Line pitch (128 for sand128)
.set rb_pitch,                     rb16

# Loop count - 2 (set up TMU for next xfer)
.set rb_i_tmu,                     rb17

# Loop count for min(height, 16)
# Y will reset & loop again if height > 16
.set rb_lcount,                    rb18

# frame_base2_next
.set rb_base2_next,                rb19

# Setup: Height of Y+C in sand, (x&mask)*xpitch will give
# offset to the slice
.set rb_xpitch,                    rb20

# These 3 consts each save 1 instruction in Y loop setup
# so whilst they are worthwhile they should be the 1st to die if we need
# another b reg
.set rb_y_coeffs_2,                rb21                         # 0x050b0a00
.set rb_y_coeffs_3,                rb22                         # 0x11283a40
.set rb_y_coeffs_5,                rb23                         # 0x0a0b0500

# Setup: 0xff (8-bit) / 0xffff (9+ bit)
.set rb_pmask,                     rb24

# vdw_setup_1(dst_pitch)
.set rb_dma1_base,                 rb25

# Setup: pic width - 1
# In bytes so 8 bit luma is (width - 1)*1, 16 bit chroma is (width -1)*4 etc.
.set rb_max_x,                     rb26

# vdw_setup_0 (depends on QPU number)
.set rb_dma0_base,                 rb27

# Setup: vw_setup value to reset VPM write pointer
.set rb_vpm_init,                  rb28

# Loop: vdw_setup_1(dst_pitch-width) = stride
.set rb_dma1,                      rb29

# Setup: pic_height - 1
.set rb_max_y,                     rb30

# Setup: FIR H offset
.set rb_fir_off_h,                 rb31


# With shifts only the bottom 5 bits are considered so -16=16, -15=17 etc.
.set i_shift16,                    -16
.set i_shift21,                    -11
.set i_shift23,                     -9
.set i_shift30,                     -2

# Much of the setup code is common between Y & C
# Macros that express this - obviously these can't be overlapped
# so are probably unsuitable for loop code

.macro m_calc_dma_regs, v_bit_depth, v_blk_height, r_vpm, r_dma
  mov r2, qpu_num
.if v_bit_depth <= 8
  # 8 bit version
  asr r1, r2, 2
  shl r1, r1, 6
  and r0, r2, 3
  or  r0, r0, r1

  mov r1, vpm_setup(0, 4, h8p(0, 0))   # 4 is stride - stride acts on ADDR which is Y[5:0],B[1:0] for 8 bit
  add r_vpm, r0, r1  # VPM 8bit storage

  mov r1, vdw_setup_0(0, 0, dma_h8p(0,0,0)) # height,width added later
  shl r0, r0, 5

.else
  # 16 bit version
  # Limited to 8 QPUs if blk height > 8
  asr r1, r2, 1
.if v_blk_height <= 8
  shl r1, r1, 4
.else
  shl r1, r1, 5
.endif
  and r0, r2, 1
  or  r0, r0, r1

  mov r1, vpm_setup(0, 2, h16p(0, 0))   # 2 is stride - stride acts on ADDR
  add r_vpm, r0, r1

  # X = H * 8 so the YH from VPMVCD_WR_SETUP[ADDR] drops into
  # XY VPMVCD_WR_SETUP[VPMBASE] if shifted left 3 (+ 3 for pos of field in reg)
  mov r1, vdw_setup_0(0, 0, dma_h16p(0,0,0))    # height,width added later
  shl r0, r0, 6
.endif
  add r_dma, r0, r1  # DMA out
.endm


.macro m_setup_q0
  srel -, 12
.endm

# Code start label
::mc_start

################################################################################
# mc_setup_c
#
# typedef struct qpu_mc_pred_c_s_s {
#     int16_t y;
#     int16_t x;
#     uint32_t base;
#     uint32_t pic_cw;            // C Width (== Y width / 2)
#     uint32_t pic_ch;            // C Height (== Y Height / 2)
#     uint32_t stride2;
#     uint32_t stride1;
#     uint32_t wdenom;
#     int16_t y2;
#     int16_t x2;
#     uint32_t base2;
#     uint32_t next_fn;
# } qpu_mc_pred_c_s_t;

.macro m_setup_c, v_bit_depth

# Cannot use mul24 on x as x might be -ve, so must use shift
.if v_bit_depth <= 8
.set v_x_shift,         1
.set v_pmask,           0xff
.set v_blk_height,      C_BLK_HEIGHT_8
.else
.set v_x_shift,         2
.set v_pmask,           0xffff
.set v_blk_height,      C_BLK_HEIGHT_16
.endif

  mov tmurs, 1                  ; mov ra0, unif                 # No TMU swap ; x_y

  mov r0, [0,2,0,2,0,2,0,2,1,3,1,3,1,3,1,3]
  shl rb_ef, r0, i_shift30      ; mov ra_base, unif             # ; ref_c_base

# Read image dimensions
  sub r0, unif, 1                                               # pic c width
  shl rb_max_x, r0, v_x_shift                                   # rb_max_x in bytes
  sub rb_max_y, unif, 1                                         # pic c height

# load constants
  mov ra_kff800100, 0xff800100
  mov rb_pmask, v_pmask
  mov ra_blk_height_pmax, ((1 << v_bit_depth) - 1) | (v_blk_height << 16)
  mov rb_fir_off_h, (FIR_OFFSET << (v_bit_depth - 8))
  mov ra_fir_off_val_wt_den_p7, (FIR_OFFSET << 16) | (DENOM + 15 - v_bit_depth)

# get source pitch
  mov ra_ef, rb_ef              ; mov rb_xpitch, unif           # ; stride2
  mov rb_pitch, unif                                            # stride1
  mov r1, vdw_setup_1(0)                                        # [rb_pitch delay] Merged with dst_stride shortly
  add rb_dma1_base, r1, rb_pitch                                # vdw_setup_1

  and r0, 1, elem_num
  nop                           ; mul24 r0, r0, 5
.if v_bit_depth <= 8
  add rb_elem_x, r0, elem_num
.else
  add r0, r0, elem_num
  add rb_elem_x, r0, r0
.endif

# Compute base address for first and second access
# ra_base ends up with t0s base
# ra_base2 ends up with t1s base

  shl r0, ra0.16b, v_x_shift                                    # [rb_elem_x delay]
  add r0, r0, rb_elem_x                                         # Add elem no to x to get X for this slice
  max r0, r0, 0                 ; mov ra_y, ra0.16a             # ; stash Y
  min r0, r0, rb_max_x

# Get shift
# Shift will always calculate as 0 for 9+ bit
# Ideally we can optimize the shift out of the code in these cases but for now
# it is tidier to leave it in
.if v_bit_depth <= 8
  shl ra_xshift_next, r0, 3
.else
  mov ra_xshift_next, 0         ; mov rb_xshift2_next, 0
.endif

# In a single 32 bit word we get 1 or 2 UV pairs so mask bottom bits of xs if we need to

.if v_bit_depth <= 8
  and r0, r0, -4
.endif
  sub r1, ra_k0, rb_pitch
  and r1, r0, r1
  xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1                ; mov ra0, unif                 # ; next_x2_y2
  add ra_base, ra_base, r0

# Compute part of VPM to use for DMA output
# * We only get 8 QPUs if 16 bit - maybe reduce height and auto-loop?
  m_calc_dma_regs v_bit_depth, v_blk_height, rb_vpm_init, rb_dma0_base

# And again for L1, but only worrying about frame2 stuff

# Compute base address for first and second access
# ra_base ends up with t0s base
# rb_base2 ends up with t1s base

  shl r0, ra0.16b, v_x_shift
  add r0, r0, rb_elem_x         ; mov ra_y2, ra0.16a            # Add QPU slice offset
  max r0, r0, 0                 ; mov rb_base2, unif            # ref_c_base2
  min r0, r0, rb_max_x

# Get shift (already zero if 9+ bit so ignore)
.if v_bit_depth <= 8
  shl rb_xshift2_next, r0, 3
.endif

# In a single 32 bit word we get 2 UV pairs so mask bottom bit of xs

.if v_bit_depth <= 8
  and r0, r0, -4
.endif
  sub r1, ra_k0, rb_pitch
  and r1, r0, r1                ; mov r3, PREREAD
  xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1                ; mov r2, ra_y2
  add rb_base2, rb_base2, r0    ; mov r0, ra_y

# Do preloads
# r0 = ra_y, r2 = ra_y2, r3 = PREREAD

:1
  sub.setf r3, r3, 1
  max r1, r0, 0
  min r1, r1, rb_max_y
  add r0, r0, ra_k1             ; mul24 r1, r1, rb_pitch
  add t0s, ra_base, r1          ; mov ra_y, r0

  max r1, r2, 0
  brr.anynz -, r:1b
  min r1, r1, rb_max_y
  add r2, r2, ra_k1             ; mul24 r1, r1, rb_pitch
  add t1s, rb_base2, r1         ; mov ra_y2, r2
# >>> .anynz 1b

  mov ra_link, unif                                             # link
# touch registers to keep simulator happy (and fills in delay slots)
  mov ra4, 0                    ; mov rb4, 0
  bra -, ra_link
  mov ra5, 0                    ; mov rb5, 0
  mov ra6, 0                    ; mov rb6, 0
  mov ra7, 0                    ; mov rb7, 0
# >>> ra_link
.endm

::mc_setup_c_q0
  m_setup_q0
::mc_setup_c_qn
  m_setup_c 8

################################################################################
#
# mc_filter_c_p
#
# typedef struct qpu_mc_pred_c_p_s {
#     int16_t y;
#     int16_t x;
#     uint32_t base;
#     uint16_t h;
#     uint16_t w;
#     uint32_t coeffs_x;
#     uint32_t coeffs_y;
#     uint32_t wo_u;
#     uint32_t wo_v;
#     uint32_t dst_addr_c;
#     uint32_t next_fn;
# } qpu_mc_pred_c_p_t;

.macro m_filter_c_p, v_tmu, v_bit_depth

.if v_bit_depth <= 8
.set v_x_shift,         1
.set v_x_mul,           2
.set v_v_shift,         8
# Shifts to get width & height in the right place in rb_dma0
.set v_dma_h_shift,     7
.set v_dma_wh_shift,    i_shift16
.else
.set v_x_shift,         2
.set v_x_mul,           4
.set v_v_shift,         i_shift16
# Shifts to get width & height in the right place in rb_dma0
.set v_dma_h_shift,     8
.set v_dma_wh_shift,    15
.endif

.if v_tmu == 0
.set vrx_xshift,        rb_xshift2              # b side more convienient
.set vrx_xshift_next,   ra_xshift_next
.set vra_y_next,        ra_y_next
.set vrx_base_next,     ra_base_next
.set vra_y,             ra_y
.set vra_base,          ra_base
.set vr_txs,            t0s
.else
.set vrx_xshift,        ra_xshift               # a side more convienient
.set vrx_xshift_next,   rb_xshift2_next
.set vra_y_next,        ra_y2_next
.set vrx_base_next,     rb_base2_next
.set vra_y,             ra_y2
.set vra_base,          rb_base2
.set vr_txs,            t1s
.endif

# denom shift values
.set i_wt_den_p5,                  (DENOM + 13 - v_bit_depth)
.set i_wt_den_p6,                  (DENOM + 14 - v_bit_depth)

# per-channel shifts were calculated on the *previous* invocation
# get base addresses and per-channel shifts for *next* invocation
  mov vw_setup, rb_vpm_init     ; mov ra2, unif                 # ; x_y

  add.setf -, rb_ef, rb_ef      ; mov r3, unif                  # [ra2 delay] ; base

  shl r0, ra2.16b, v_x_shift    ; v8subs r5rep, r0, r0          # r5 = 0
  add r0, r0, rb_elem_x         ; mov ra_width_height, unif     # r1=pitch2 mask ; width_height
  sub r1, r5, rb_pitch          ; mov ra0, unif                 # ; H filter coeffs
  max r0, r0, r5                ; mov vrx_xshift, vrx_xshift_next
  min r0, r0, rb_max_x          ; mov vra_y_next, ra2.16a

.if v_bit_depth <= 8
  shl vrx_xshift_next, r0, 3
  and r0, r0, -4
.endif
  and r1, r0, r1                ; mul24 r2, ra_width, v_x_mul   # r2=w*2 (we are working in pel pairs)  ** x*2 already calced!
  xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1                ; mov ra3, unif                 # ; V filter coeffs
  add vrx_base_next, r3, r0     ; mov r1, ra_height

# set up VPM write
  sub rb_dma1, rb_dma1_base, r2 ; mov ra_wt_off_mul_l0, unif    # Compute vdw_setup1(dst_pitch-width) ; U offset/weight
  add rb_i_tmu, r1, (3-4) - PREREAD ; v8min r1, r1, ra_blk_height
  add rb_lcount, r1, (3-4)      ; mov.ifc ra_wt_off_mul_l0, unif # ; V offset/weight

# Misc final setup...

  shl r0, r1, v_dma_h_shift     ; mov ra_dest, unif             # ; dst_addr
  add r0, r0, r2                ; mov r2, ra_fir_off_val        # Combine width and height of destination area (r0=h<<8, r2=w*2)
  shl r0, r0, v_dma_wh_shift    ; mov rb10, ra3.8c              # Shift into bits 16 upwards of the vdw_setup0 register
  add ra_dma0, r0, rb_dma0_base ; mov r1, ra_wt_off_l0          # ; r1=weight
  shl r1, r1, i_wt_den_p5       ; mul24 r0, r2, ra_wt_mul_l0
  sub rb_wt_off, r1, r0         ; mov r0, ra_kmul_add
  add ra_wt_mul_l0, ra_wt_mul_l0, r0 ; mov r5rep, -4            # ; loop counter (V FIFO fill = 4)
  mov rb11, ra3.8d              ; mov ra_link, unif             # ; Link

# r5           = -4                     (loop counter)
# ra_wt_mul_l0 = weight L0 + 128        (now unsigned)
# rb_wt_off    = (offset * 2 + 1) << (wt_den + 5)
# rb31         = FIR value offset

# FIFO: rb4, ra5, rb6, ra7
# Coeffs in ra3.8a, ra3.8b, rb10, rb11

# We want (r0r1)
# U0U3 : V0V3 : U1U4 : V1V4 : U2U5 : V2U5 : ...
# We fetch (after shift)
#  C0  :  C3  :  C1  :  C4  :  C2  :  C5  : ...

:1
# retrieve texture results and pick out bytes
# then submit two more texture requests

.if v_tmu == 0
  sub.setf -, r5, rb_i_tmu      ; mov rb4, ra5                  ; ldtmu0
  shr r2, r4, vrx_xshift        ; mov.ifz  r3, vra_y_next
  shr r1, r2, v_v_shift         ; mov.ifnz r3, vra_y
  add.setf -, rb_ef, rb_ef      ; mov.ifz  vra_base, vrx_base_next
.else
  sub.setf -, r5, rb_i_tmu      ; mov rb4, ra5                  ; ldtmu1
  shr r2, r4, vrx_xshift        ; mov.ifz  vra_base, vrx_base_next
  shr r1, r2, v_v_shift         ; mov.ifnz r3, vra_y
  add.setf -, rb_ef, rb_ef      ; mov.ifz  r3, vra_y_next       # [r1 << delay]
.endif

  add vra_y, r3, ra_k1          ; mov      r0, r1 << 15
  max r3, r3, ra_k0             ; mov.ifnc r1, r2 << 1
  min r3, r3, rb_max_y          ; mov.ifnc r0, r2

  and r1, r1, ra_pmax           ; mul24 r3, r3, rb_pitch
.if v_tmu == 0
  add vr_txs, vra_base, r3      ; v8min r0, r0, rb_pmask        # ; mask bytes
.else
  add vr_txs, vra_base, r3      ; v8min r0, r0, ra_pmax         # ; mask bytes
.endif

# apply horizontal filter
# The filter coeffs for the two halves of this are the same (unlike in the
# Y case) so it doesn't matter which ra0 we get them from
# Also as the two halves are locked together we don't need to separate the 1st
# r0 mul or the last r1 mul as they are valid for all QPUs

  add r5rep, r5, 1              ; mul24      r3, ra0.8a,       r0
  sub r2, rb_fir_off_h, r3      ; mul24      r3, ra0.8d,       r1
  sub r2, r2, r3                ; mul24      r3, ra0.8b << 2,  r0 << 2  @ "mul_used", 0
  nop                           ; mul24.ifn  r3, ra0.8b << 12, r1 << 12 @ "mul_used", 0
  add r2, r2, r3                ; mul24      r3, ra0.8c << 4,  r0 << 4  @ "mul_used", 0
  add.setf -, r5, r5            ; mul24.ifn  r3, ra0.8c << 14, r1 << 14 @ "mul_used", 0

# V filter = - r4 * a + r5 * b + r6 * c - r7 * d (post FIFO shift)
# We would like to save the r5->r4 shift but we need a delay slot
# for both r7 & r6 which we can't find anything to put in if we have
# already multiplied r4 & r5!
  brr.anyn -, r:1b
  add r2, r2, r3                ; mul24 r0, ra7, rb10           # r6 post
  mov ra5, rb6                  ; mul24 r1, rb6, ra3.8b         # r5 post
  asr ra7, r2, v_bit_depth - 8  ; mov rb6, ra7
# >>> .anyn 1b

  add r1, r1, r0                ; mul24 r0, rb4, ra3.8a         # [ra7 delay]
  sub r1, r1, r0                ; mul24 r0, ra7, rb11
  sub r1, r1, r0

  asr r1, r1, 6                 ; mov r3, ra_blk_height         # ; NxtLoop
  sub.setf -, r5, rb_lcount     ; mul24 r0, r1, ra_wt_mul_l0
  add r0, r0, rb_wt_off         ; mul24 r1, r1, ra_kmul_add
  sub r1, r0, r1                ; v8subs r0, ra_height, r3      # ; NxtLoop
  brr.anyn -, r:1b
  asr r1, r1, i_wt_den_p6
  min r1, r1, ra_pmax           ; mov -, vw_wait
  max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch        # ; NxtLoop
# >>> .anyn 1b

# r0 = remaining height (min 0)
# r2 = r3 * rb_pitch
# r3 = block_height

# If looping again then we consumed 16 height last loop
# rb_dma1 (stride) remains constant
# rb_i_tmu remains const (based on total height)
# recalc ra_dma0, rb_lcount based on new segment height

  mov.setf ra_height, r0        ; mov vw_setup, ra_dma0         # VDW setup 0

# DMA out
  bra.anyz -, ra_link
  min r0, r0, r3                ; mov vw_setup, rb_dma1         # Stride
  sub r1, r0, r3                ; mov vw_addr, ra_dest          # start the VDW
  shl r1, r1, i_shift23
# >>> .anyz ra_link

# Here r1 = cur_blk_height - 16 so it will be 0 or -ve
# We add to dma0 to reduce the number of output lines in the final block
  brr -, r:1b
  add rb_lcount, rb_lcount, r0
  add ra_dma0, ra_dma0, r1
  add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init     # ; Reset our VDM write pointer
# >>> 1b
.endm

::mc_filter_c_p
  m_filter_c_p 0, 8

::mc_filter_c_p_l1
  m_filter_c_p 1, 8

################################################################################
#
# mc_filter_c_b
#
# typedef struct qpu_mc_pred_c_b_s {
#     int16_t y;
#     int16_t x;
#     uint32_t base;
#     uint16_t h;
#     uint16_t w;
#     uint32_t coeffs_x1;
#     uint32_t coeffs_y1;
#     int16_t weight_u1;
#     int16_t weight_v1;
#     int16_t y2;
#     int16_t x2;
#     uint32_t base2;
#     uint32_t coeffs_x2;
#     uint32_t coeffs_y2;
#     uint32_t wo_u2;
#     uint32_t wo_v2;
#     uint32_t dst_addr_c;
#     uint32_t next_fn;
# } qpu_mc_pred_c_b_t;

.macro m_filter_c_b, v_bit_depth

.if v_bit_depth <= 8
.set v_x_shift,         1
.set v_v_shift,         8
# Shifts to get width & height in the right place in ra_dma0
.set v_dma_h_shift,     7
.set v_dma_wh_shift,    i_shift16
.else
.set v_x_shift,         2
.set v_v_shift,         i_shift16
# Shifts to get width & height in the right place in ra_dma0
.set v_dma_h_shift,     8
.set v_dma_wh_shift,    15
.endif
.set v_x_mul,           (1 << v_x_shift)

# denom shift values
.set i_wt_den_p5,                  (DENOM + 13 - v_bit_depth)
.set i_wt_den_p6,                  (DENOM + 14 - v_bit_depth)

# per-channel shifts were calculated on the *previous* invocation

# get base addresses and per-channel shifts for *next* invocation
  mov vw_setup, rb_vpm_init     ; mov ra2, unif                 # ; x_y

  add.setf -, rb_ef, rb_ef      ; mov r3, unif                  # [ra2 delay] ; r3=base

  shl r0, ra2.16b, v_x_shift    ; v8subs r5rep, r1, r1          # x ; r5=0
  add r0, r0, rb_elem_x         ; mov ra_y_next, ra2.16a
  sub r1, r5, rb_pitch          ; mov ra_width_height, unif     # r1=pitch2 mask ; width_height
  max r0, r0, r5                ; mov ra_xshift, ra_xshift_next
  min r0, r0, rb_max_x          ; mov ra0, unif                 # ; L0 H filter coeffs

.if v_bit_depth <= 8
  shl ra_xshift_next, r0, 3
.endif

  and r0, r0, -4                ; mov ra2, unif                 # ; L0 V filter coeffs
  and r1, r0, r1                ; mul24 r2, ra_width, v_x_mul   # r2=x*2 (we are working in pel pairs)
  xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1                ; mov r1, ra_height             # Add stripe offsets ; r1=height
  add ra_base_next, r3, r0      ; mov rb_xshift2, rb_xshift2_next # ; xshift2 used because B

# set up VPM write

  sub rb_dma1, rb_dma1_base, r2 ; mov ra_wt_off_mul_l0, unif    # Compute vdw_setup1(dst_pitch-width) ; U weight
  add rb_i_tmu, r1, (3-4) - PREREAD ; v8min r1, r1, ra_blk_height
  add rb_lcount, r1, (3-4)      ; mov.ifc ra_wt_mul_l0, ra_wt_off_l0 # ; V weight

  shl r0, r1, v_dma_h_shift     ; mov ra3, unif                 # ; x2_y2
  add r0, r0, r2                ; mov r3, unif                  # [ra3 delay] ; base
  shl r0, r0, v_dma_wh_shift    ; mov ra_y2_next, ra3.16a       # Shift into bits 16 upwards of the vdw_setup0 register
  add ra_dma0, r0, rb_dma0_base ; mov r0, ra3.16b               # r0=x

# L1 - uniform layout could possibly be optimized

  shl r0, r0, v_x_shift         ; mov ra1, unif                 # r0=x<<shift ; L1 H filter coeffs
  add r0, r0, rb_elem_x         ; mov ra3, unif                 # ; L1 V filter coeffs
  sub r1, r5, rb_pitch          ; mov ra_wt_off_mul_l1, unif    # [ra3 delay] r1=pitch2 mask ; U offset/weight
  max r0, r0, r5                ; mov ra9, rb_max_y
  min r0, r0, rb_max_x          ; mov r2, ra_kmul_add

.if v_bit_depth <= 8
  shl rb_xshift2_next, r0, 3
.endif

  and r0, r0, -4                ; mov.ifc ra_wt_off_mul_l1, unif # ; V offset/weight
  and r1, r0, r1                ; mov r5rep, -4
  xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1                ; mov ra_dest, unif             #  Add stripe offsets ; dst_addr
  add rb_base2_next, r3, r0     ; mov r0, ra_fir_off_val

  add ra_wt_mul_l0, ra_wt_mul_l0, r2 ; mul24 r1, r0, ra_wt_mul_l0
  add ra_wt_mul_l1, ra_wt_mul_l1, r2 ; mul24 r0, r0, ra_wt_mul_l1
  add r0, r0, r1                ; mov r1, ra_wt_off_l1          # ; L0 off unset
  shl r1, r1, i_wt_den_p6       ; mov rb11, ra3.8d
  sub rb_wt_off, r1, r0         ; mov ra_link, unif             # ; link

  mov ra10, rb_xshift2          ; mov rb7,  ra2.8d

# r5        loop counter (-4)
# ra0       H coeffs L0
# ra1       H coeffs L1
# ra2       V coeffs L0
# ra3       V coeffs L1
# ra9       rb_max_y alias
# ra10      rb_xshift2 alias

:1
# retrieve texture results and pick out bytes
# then submit two more texture requests
  sub.setf -, r5, rb_i_tmu      ; nop                           ; ldtmu0
  shr r2, r4, ra_xshift         ; mov.ifz rb_base2, rb_base2_next
  shr r1, r2, v_v_shift         ; mov.ifz ra_y_y2, ra_y_y2_next
  add.setf -, rb_ef, rb_ef      ; mov.ifz ra_base, ra_base_next # [ra_y delay]
  add ra_y, 1, ra_y             ; mov r3, ra_y

  max r3, r3, ra_k0             ; mov      r0, r1 << 15
  min r3, r3, ra9               ; mov.ifnc r1, r2 << 1

  mov.ifnc r0, r2               ; mul24 r3, r3, rb_pitch
  add t0s, ra_base, r3          ; v8min r0, r0, rb_pmask        # ; masks bytes

# L0 H-filter (-ra4*, +rb5, +rb6, -ra7)

  and r1, r1, rb_pmask          ; mul24      r2, ra0.8a,       r0
  sub r2, rb_fir_off_h, r2      ; mul24      r3, ra0.8d,       r1
  sub r2, r2, r3                ; mul24      r3, ra0.8b << 2,  r0 << 2  @ "mul_used", 0
  nop                           ; mul24.ifn  r3, ra0.8b << 12, r1 << 12 @ "mul_used", 0
  add r2, r2, r3                ; mul24      r3, ra0.8c << 4,  r0 << 4  @ "mul_used", 0
  nop                           ; mul24.ifn  r3, ra0.8c << 14, r1 << 14 @ "mul_used", 0

  add r0, r2, r3                ; mul24 ra4, rb5, ra2.8a        ; ldtmu1

  shr r2, r4, ra10              ; mov rb5, rb6
  shr r1, r2, v_v_shift         ; mov r3, ra_y2
  shr ra7, r0, v_bit_depth - 8  ; mov rb6, ra7                  # [r1 << delay]

  add ra_y2, r3, ra_k1          ; mov      r0, r1 << 15
  max r3, r3, ra_k0             ; mov.ifnc r1, r2 << 1
  min r3, r3, rb_max_y          ; v8min r1, r1, ra_pmax

  mov.ifnc r0, r2               ; mul24 r3, r3, rb_pitch
  add t1s, rb_base2, r3         ; v8min r0, r0, ra_pmax         # ; masks bytes

# L1 H-filter (-r0*, +rb9, +rb10, -ra11)

  add r5rep, r5, 1              ; mul24      r2, ra1.8a,       r0
  sub r2, rb_fir_off_h, r2      ; mul24      r3, ra1.8d,       r1
  sub r2, r2, r3                ; mul24      r3, ra1.8b << 2,  r0 << 2  @ "mul_used", 0
  nop                           ; mul24.ifn  r3, ra1.8b << 12, r1 << 12 @ "mul_used", 0
  add r2, r3, r2                ; mul24      r3, ra1.8c << 4,  r0 << 4  @ "mul_used", 0
  add.setf -, r5, r5            ; mul24.ifn  r3, ra1.8c << 14, r1 << 14 @ "mul_used", 0

  brr.anyn -, r:1b
  add r2, r2, r3                ; mul24 r0, rb9,  ra3.8a
  mov rb9, rb10                 ; mul24 r1, rb10, ra3.8b
  shr ra11, r2, v_bit_depth - 8 ; mov rb10, ra11
# >>> .anyn 1b

  sub r2, r1, r0                ; mul24 r1, rb5,  ra2.8b        # L1 ; L0
  sub.setf -, r5, rb_lcount     ; mov r0, ra4
  sub r1, r1, r0                ; mul24 r0, rb6,  ra2.8c
  add r1, r1, r0                ; mul24 r0, ra7,  rb7

  sub r1, r1, r0                ; mul24 r0, rb10, ra3.8c        # L1
  add r2, r2, r0                ; mul24 r0, ra11, rb11          # L1
  sub r2, r2, r0

  shr r1, r1, 6
  shr r2, r2, 6                 ; mul24 r0, r1, ra_wt_mul_l0
  add r2, r2, r1                ; mul24 r1, r2, ra_wt_mul_l1
  add r1, r1, r0                ; mul24 r2, r2, ra_kmul_add
  sub r1, r1, r2                ; mov r3, ra_blk_height         # ; NxtLoop
  add r1, r1, rb_wt_off         ; v8subs r0, ra_height, r3      # ; NxtLoop

  brr.anyn -, r:1b
  asr r1, r1, ra_wt_den_p7
  min r1, r1, ra_pmax           ; mov -, vw_wait
  max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch        # ; NxtLoop
# >>> .anyn 1b

# r0 = remaining height (min 0)
# r2 = r3 * rb_pitch
# r3 = block_height

# If looping again then we consumed 16 height last loop
# rb_dma1 (stride) remains constant
# rb_i_tmu remains const (based on total height)
# recalc ra_dma0, rb_lcount based on new segment height

  mov.setf ra_height, r0        ; mov vw_setup, ra_dma0         # ; VDW setup 0

# DMA out
  bra.anyz -, ra_link
  min r0, r0, r3                ; mov vw_setup, rb_dma1         # ; Stride
  sub r1, r0, r3                ; mov vw_addr, ra_dest          # ; start the VDW
  shl r1, r1, i_shift23
# >>> .anyz ra_link

# Here r1 = cur_blk_height - 16 so it will be 0 or -ve
# We add to dma0 to reduce the number of output lines in the final block
  brr -, r:1b
  add rb_lcount, rb_lcount, r0
  add ra_dma0, ra_dma0, r1
  add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init     # ; Reset our VDM write pointer
# >>> 1b
.endm

::mc_filter_c_b
  m_filter_c_b 8

################################################################################
# Exit code used by both Luma & Chroma so place between them to avoid I-cache
# conflicts

.macro m_exit_drain
.if PREREAD == 2
# Special case 2 as loop is wasteful
  nop                   ; nop           ; ldtmu0
  nop                   ; nop           ; ldtmu1
  nop                   ; nop           ; ldtmu0
  mov -, vw_wait        ; nop           ; ldtmu1
.else
  mov.setf r3, PREREAD - 1
:1
  brr.anynz -, r:1b
  nop                   ; nop           ; ldtmu0
  nop                   ; nop           ; ldtmu1
  sub.setf r3, r3, 1
 # >>>
  mov  -, vw_wait
.endif
.endm

# This sync layout groups QPUs 0-3, 4-7, 8-11 (i.e. 1 group per TMU pair)
# All qpus start at the beginning and after that (group - 1) must have finished
# before (group) can start
#
# Requires setup code for QPU 0 to srel sem 12 (m_setup_q0) to start the chain
# Exit code will sacq sem 12 so everything is @ 0 on exit (this is important -
# lockup otherwise)
#
# There is some, currently ill defined, potential lockup if we have the VDM active
# whilst doing sem stuff so we wait first. ?? QPU stall from sem stalls VDM pipe too ??
#
# The code stalled when I had many waiters on a single sem so we have a
# "ripple" of srels to restart.  Unsure why, may have been bug, but this works
# and we currently have both the memory & sems to support it.
.macro m_sync_q, n_qpu, n_quads
# Do not generate code for qpu >= quads * 4 -  fns should never be called
.if n_qpu < n_quads * 4
  mov ra_link, unif     # Can only branch to an a reg (not r0)
  mov -, vw_wait        # [ra_link delay]

.set n_sem_sync, n_qpu - (n_qpu % 4)
.set n_sem_in, n_qpu
.set n_sem_out, n_qpu + 1

.if n_qpu % 4 == 0

.set n_sem_quad_in,  12 + n_qpu / 4
.set n_sem_quad_out, 12 + (((n_qpu / 4) + 1) % n_quads)

  sacq -, n_sem_sync
  sacq -, n_sem_sync
  sacq -, n_sem_sync
  bra -, ra_link
  sacq -, n_sem_quad_in
  srel -, n_sem_out
  srel -, n_sem_quad_out

.else
  bra -, ra_link
  srel -, n_sem_sync
  sacq -, n_sem_in
.if n_sem_out % 4 != 0
  srel -, n_sem_out
.else
  nop
.endif
.endif
.endif
.endm

.set v_quads8, N_QPU_8 / 4

::mc_sync_q0
  m_sync_q 0, v_quads8
::mc_sync_q1
  m_sync_q 1, v_quads8
::mc_sync_q2
  m_sync_q 2, v_quads8
::mc_sync_q3
  m_sync_q 3, v_quads8
::mc_sync_q4
  m_sync_q 4, v_quads8
::mc_sync_q5
  m_sync_q 5, v_quads8
::mc_sync_q6
  m_sync_q 6, v_quads8
::mc_sync_q7
  m_sync_q 7, v_quads8
::mc_sync_q8
  m_sync_q 8, v_quads8
::mc_sync_q9
  m_sync_q 9, v_quads8
::mc_sync_q10
  m_sync_q 10, v_quads8
::mc_sync_q11
  m_sync_q 11, v_quads8

# mc_exit()
# Chroma & Luma the same now

.macro m_exit_qn
  m_exit_drain
  nop                   ; nop           ; thrend
  nop
  nop
# >>> thrend <<<
.endm

::mc_exit_c_qn
::mc_exit_y_qn
  m_exit_qn



# mc_interrupt_exit12()

.macro m_exit_q0
  m_exit_drain
  sacq -, 12
  nop                   ; nop           ; thrend
  mov interrupt, 1
  nop
# >>> thrend <<<
.endm

::mc_exit_c_q0
::mc_exit_y_q0
  m_exit_q0

# LUMA CODE

# The idea is to form B predictions by doing 8 pixels from ref0 in parallel with 8 pixels from ref1.
# For P frames we make the second x,y coordinates offset by +8


################################################################################
# mc_setup
#
# typedef struct qpu_mc_pred_y_s_s {
#    qpu_mc_src_t next_src1;
#    qpu_mc_src_t next_src2;
#    uint16_t pic_h;
#    uint16_t pic_w;
#    uint32_t stride2;
#    uint32_t stride1;
#    uint32_t wdenom;
#    uint32_t next_fn;
# } qpu_mc_pred_y_s_t;

.macro m_setup_y, v_bit_depth

# Cannot use mul24 on x as x might be -ve, so must use shift
.if v_bit_depth <= 8
.set v_x_shift,         0
.set v_pmask,           0xff
.set v_blk_height,      Y_BLK_HEIGHT_8
.else
.set v_x_shift,         1
.set v_pmask,           0xffff
.set v_blk_height,      Y_BLK_HEIGHT_16
.endif


  # Need to save these because we need to know the frame dimensions before computing texture coordinates
  mov tmurs, 1                  ; mov ra0, unif                 # No TMU swap ; x_y
  mov ra9, unif                                                 # ref_y_base
  mov ra1, unif                                                 # x2_y2


# load constants
  mov r0, [0,2,0,2,0,2,0,2,1,3,1,3,1,3,1,3]
  shl rb_ef, r0, i_shift30      ; mov ra11, unif                # ; ref_y2_base

  mov ra_kff800100, 0xff800100
  mov rb_pmask, v_pmask
  mov ra_blk_height_pmax, ((1 << v_bit_depth) - 1) | (v_blk_height << 16)
  mov rb_fir_off_h, (FIR_OFFSET << (v_bit_depth - 8))
  mov ra_fir_off_val_wt_den_p7, (FIR_OFFSET << 16) | (DENOM + 15 - v_bit_depth)
  mov rb_y_coeffs_2, 0x050b0a00
  mov rb_y_coeffs_3, 0x11283a40
  mov rb_y_coeffs_5, 0x0a0b0500

# Compute part of VPM to use

# Read image dimensions
  mov ra3, unif                                                 # width_height
  mov ra_ef, rb_ef              ; mov rb_xpitch, unif           # [ra3 delay] ; stride2
.if v_x_shift == 0
  sub rb_max_x, ra3.16b, 1
.else
  sub r0, ra3.16b, 1
  shl rb_max_x, r0, v_x_shift
.endif
  sub rb_max_y, ra3.16a, 1
  mov r3, elem_num              ; mov rb_pitch, unif            # stride1

# get destination pitch
  mov r1, vdw_setup_1(0)                                        # [rb_pitch delay]
  or  rb_dma1_base, r1, rb_pitch

# Compute base address for first and second access
  add r0, ra0.16b, r3                                           # Load x + elem_num
.if v_x_shift != 0
  shl r0, r0, v_x_shift
.endif
  max r0, r0, 0
  min r0, r0, rb_max_x
  shl ra_xshift_next, r0, 3                                     # Compute shifts

# X is byte offset - we can only load words - mask

  and r0, r0, -4                ; v8subs r2, r2, r2
  sub r2, r2, rb_pitch
  and r1, r0, r2
  xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1                                                # Add stripe offsets
  add ra_base, ra9, r0

  # r3 still contains elem_num
  add r0, ra1.16b, r3                                           # Load x
.if v_x_shift != 0
  shl r0, r0, v_x_shift
.endif
  max r0, r0, 0
  min r0, r0, rb_max_x
  shl rb_xshift2_next, r0, 3                                    # Compute shifts

  # r2 still contains mask
  and r0, r0, -4
  and r1, r0, r2
  xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1                                                # Add stripe offsets
  add rb_base2, ra11, r0

# Do preloads
  nop                           ; mov r0, ra0.16a               # ; r0 = y
  mov r3, PREREAD               ; mov r2, ra1.16a               # ; r2 = y2

:1
  sub.setf r3, r3, 1
  max r1, r0, 0
  min r1, r1, rb_max_y
  add r0, r0, ra_k1             ; mul24 r1, r1, rb_pitch
  add t0s, ra_base, r1          ; mov ra_y, r0

  max r1, r2, 0
  brr.anynz -, r:1b
  min r1, r1, rb_max_y
  add r2, r2, ra_k1             ; mul24 r1, r1, rb_pitch
  add t1s, rb_base2, r1         ; mov ra_y2, r2
# >>> .anynz 1b

  m_calc_dma_regs v_bit_depth, v_blk_height, rb_vpm_init, rb_dma0_base

  mov ra_link, unif                                             # Next fn

# touch vertical context to keep simulator happy
  mov ra8,  0                   ; mov rb8,  0                   # [ra_link delay]
  bra -, ra_link
  mov ra9,  0                   ; mov rb9,  0
  mov ra10, 0                   ; mov rb10, 0
  mov ra11, 0                   ; mov rb11, 0
# >>> ra_link
.endm

::mc_setup_y_q0
  m_setup_q0
::mc_setup_y_qn
  m_setup_y 8

################################################################################
#
# Start of per-block setup code
# P and B blocks share the same setup code to save on Icache space

# get base addresses and per-channel shifts for *next* invocation
# per-channel shifts were calculated on the *previous* invocation

# 1st 3 instructions of per_block-setup in branch delay
#
# typedef struct qpu_mc_pred_y_p_s {
#    qpu_mc_src_t next_src1;
#    qpu_mc_src_t next_src2;
#    uint16_t h;
#    uint16_t w;
#    uint32_t mymx21;
#    uint32_t wo1;
#    uint32_t wo2;
#    uint32_t dst_addr;
#    uint32_t next_fn;
# } qpu_mc_pred_y_p_t;
#

.macro m_luma_setup, v_bit_depth
# Hack - QASM may well have have label pasting but I have no idea how...
.if v_bit_depth == 8
  brr ra_link, r:per_block_setup_8
.elif v_bit_depth == 10
  brr ra_link, r:per_block_setup_10
.endif
  mov ra0, unif                 ; mov r3, elem_num              # y_x ; elem_num has implicit unpack??
  add.setf -, rb_ef, rb_ef      ; v8subs r5rep, r2, r2          # [ra0 delay] ; r5 = 0
  add r0, ra0.16b, r3           ; mov rb_xshift2, rb_xshift2_next
.endm

.macro m_per_block_setup, v_bit_depth

.if v_bit_depth <= 8
.set v_x_shift,         0
.set v_x_mul,           1
# Shifts to get width & height in the right place in ra_dma0
.set v_dma_h_shift,     7
.set v_dma_wh_shift,    i_shift16
.else
.set v_x_shift,         1
.set v_x_mul,           2
# Shifts to get width & height in the right place in ra_dma0
.set v_dma_h_shift,     8
.set v_dma_wh_shift,    15
.endif

.if v_x_shift != 0
  shl r0, r0, v_x_shift
.endif
  max r0, r0, r5                ; mov ra_xshift, ra_xshift_next
  min r0, r0, rb_max_x

  shl ra_xshift_next, r0, 3                                     # Compute shifts
  and r0, r0, -4
  sub r2, r5, rb_pitch          ; mov ra_base_next, unif        # ; src1.base
  and r1, r0, r2                ; mov ra_y_next, ra0.16a
  xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1                ; mov ra1, unif                 # Add stripe offsets ; src2.x_y
  add ra_base_next, ra_base_next, r0                            # [ra1 delay]

  add r0, ra1.16b, r3                                           # Load x2
.if v_x_shift != 0
  shl r0, r0, v_x_shift
.endif
  max r0, r0, r5                ; mov ra_y2_next, ra1.16a
  min r0, r0, rb_max_x          ; mov rb_base2_next, unif       # ; src2.base
  shl rb_xshift2_next, r0, 3                                    # Compute shifts
  and r0, r0, -4                ; mov ra_width_height, unif     # ; width_height
  and r1, r0, r2                ; mov vw_setup, rb_vpm_init     # ; set up VPM write
  xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1                ; mul24 r1, ra_width, v_x_mul   # Add stripe offsets ; r1 = x in bytes
  add rb_base2_next, rb_base2_next, r0

# get width,height of block (unif load above), r1 = width * pel_size
  sub rb_dma1, rb_dma1_base, r1 ; mov r0, ra_height             # Compute vdw_setup1(dst_pitch-width)
  add rb_i_tmu, r0, (7-8) - PREREAD ; v8min r0, r0, ra_blk_height
  add rb_lcount, r0, (7-8)
  shl r0, r0, v_dma_h_shift     ; mov r3, ra_kmul_add           # ; r3 return val
  add r0, r0, r1                                                # Combine width and height of destination area
  shl r0, r0, v_dma_wh_shift    ; mov r2, ra_fir_off_val        # Shift into bits 16 upwards of the vdw_setup0 register ; r2 return val
  add ra_dma0, r0, rb_dma0_base ; mov r0, unif                  # ; Packed filter offsets

# get filter coefficients and discard unused B frame values
  shl.ifnn r0, r0, i_shift16    ; mov ra_wt_off_mul_l0, unif    #  Pick half to use ; L0 offset/weight
  shl ra8, r0, 3                ; mov rb5, ra_k255

# Coeffs are all abs values here as that means mul24 works (no sign extend from .8)

# 2nd half coeffs same as first if we can swap 8<->24 in the rotate val
# but I can't see a way of doing that that is cheap enough to be worth it

# Picked out in a slightly random order to space out uniform loads

  # 1
  mov r1, 0x01040400            # [ra8 delay]
  ror ra2.8b, r1, ra8.8d
  ror ra0.8b, r1, ra8.8c
  # 2
  ror ra2.8c, rb_y_coeffs_2, ra8.8d
  ror ra0.8c, rb_y_coeffs_2, ra8.8c
  # 0
  mov r1,0x00010100             # -ve  [ra8 delay]
  ror r0, r1, ra8.8d            ; mov ra_wt_off_mul_l1, unif    # ; L1 Wt/Offset
  ror ra0.8a, r1, ra8.8c        ; v8min rb4, r0, rb5
  # 7
  shl r1, r1, 8                 ; mov.ifn ra_wt_off_mul_l0, ra_wt_off_mul_l1 # r1 = 0x01010000
  ror r0, r1, ra8.8d            ; mov ra_dest, unif             # ; Destination address
  ror ra1.8d, r1, ra8.8c        ; v8min rb11, r0, rb5
  # 3
  ror ra2.8d, rb_y_coeffs_3, ra8.8d
  ror ra0.8d, rb_y_coeffs_3, ra8.8c
  # 5
  ror ra3.8b, rb_y_coeffs_5, ra8.8d
  ror ra1.8b, rb_y_coeffs_5, ra8.8c
  # 6
  mov r1,0x04040100
  ror ra3.8c, r1, ra8.8d
  ror ra1.8c, r1, ra8.8c        ; mov r5rep, -8                 # ; r5 return val

  bra -, ra_link
  # 4
  mov r1,0x3a281100
  ror r0, r1, ra8.8d            ; mov ra_link, unif             # ; link - load after we've used its previous val
  ror ra1.8a, r1, ra8.8c        ; v8min rb8, r0, rb5
# >>> branch ra_link

# r5 = -8
# r2 = fir_off_val
# r3 = 128
.endm

:per_block_setup_8
  m_per_block_setup 8



################################################################################
#
# mc_filter_y_pxx
#
# Setup (& therefore uniform struct) shared with _bxx
# Struct in m_luma_setup
#
# We can have 2 separate P reqs here as long as they mate to generate a
# rectangular output block (i.e. h0 = h1, w0 = 8)
#
# At this point we have already issued PREREAD pairs of texture requests for the current block

.macro m_filter_y_pxx, v_bit_depth

# denom shift values
.set i_wt_den_p5,                  (DENOM + 13 - v_bit_depth)
.set i_wt_den_p6,                  (DENOM + 14 - v_bit_depth)

  m_luma_setup v_bit_depth

  shl r1, ra_wt_off_l0, i_wt_den_p5
  add ra_wt_mul_l0, ra_wt_mul_l0, r3 ; mul24 r0, r2, ra_wt_mul_l0 # r2 = 0x4000 so mul24 safe even with -ve wt_mul
  sub rb_wt_off, r1, r0         ; mov ra_ef.8a, rb4

# retrieve texture results and pick out bytes
# then submit two more texture requests

# This loop is identical to the B loop from here --->
:1
  add.setf -, ra_ef, ra_ef      ; mul24 ra4, rb5, ra_ef

  max r2, ra_y, 0               ; mov r1, 0
  min r2, r2, rb_max_y          ; mov r3, ra_k1
  add ra_y, ra_y, r3            ; mul24 r2, r2, rb_pitch        ; ldtmu0
  add t0s, ra_base, r2          ; mov rb5,  rb6
  shr r0, r4, ra_xshift         ; mov rb6,  rb7

  max r2, ra_y2, r1             ; v8min r0, r0, rb_pmask        ; ldtmu1 # ; masks out all but wanted bytes
  shr r1, r4, rb_xshift2        ; mov rb7, ra8
  min r2, r2, rb_max_y          ; v8min r1, r1, ra_pmax
  add ra_y2, ra_y2, r3          ; mul24 r2, r2, rb_pitch
  add t1s, rb_base2, r2         ; mov ra8,  ra9

# apply horizontal filter
  add r5rep, r5, r3     ; mul24      r2, ra0.8a << 8,  r1 << 8  @ "mul_used", 0
  mov r3, rb_fir_off_h  ; mul24.ifnn r2, ra0.8a,       r0
  sub r2, r3, r2        ; mul24      r3, ra0.8b << 1,  r0 << 1  @ "mul_used", 0
  nop                   ; mul24.ifn  r3, ra0.8b << 9,  r1 << 9  @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra0.8c << 2,  r0 << 2  @ "mul_used", 0
  nop                   ; mul24.ifn  r3, ra0.8c << 10, r1 << 10 @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra0.8d << 3,  r0 << 3  @ "mul_used", 0
  nop                   ; mul24.ifn  r3, ra0.8d << 11, r1 << 11 @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra1.8a << 4,  r0 << 4  @ "mul_used", 0
  nop                   ; mul24.ifn  r3, ra1.8a << 12, r1 << 12 @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra1.8b << 5,  r0 << 5  @ "mul_used", 0
  nop                   ; mul24.ifn  r3, ra1.8b << 13, r1 << 13 @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra1.8c << 6,  r0 << 6  @ "mul_used", 0
  nop                   ; mul24.ifn  r3, ra1.8c << 14, r1 << 14 @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra1.8d << 7,  r0 << 7  @ "mul_used", 0
  add.setf -, r5, r5    ; mul24.ifn  r3, ra1.8d << 15, r1 << 15 @ "mul_used", 0

  brr.anyn -, r:1b
  sub r2, r2, r3                ; mul24 r1, rb5,  ra2.8b
  mov ra9,  rb10                ; mul24 r0, rb10, ra3.8b
  asr ra11, r2, v_bit_depth - 8 ; mov rb10, ra11
  # >>> .anyn 1b (r5 + r5)

  # apply vertical filter and write to VPM
  # - r4* + r5 - r6 + r7 + r8 - r9 + r10 - r11

  sub r1, r1, r0                ; mul24 r0, rb6,  ra2.8c
  sub r1, r1, r0                ; mul24 r0, rb7,  ra2.8d
  add r1, r1, r0                ; mul24 r0, ra8,  rb8
  add r1, r1, r0                ; mul24 r0, rb10, ra3.8c
  add r1, r1, r0                ; mul24 r0, ra11, rb11
# <--- to here
  sub.setf -, r5, rb_i_tmu      ; mov r3, ra_blk_height                 # ; NxtLoop: r3 = block height
  sub r1, r1, ra4               ; mov.ifz rb_base2, rb_base2_next
  sub r1, r1, r0                ; mov.ifz ra_base, ra_base_next

  asr r1, r1, 6                 ; mov.ifz ra_y_y2, ra_y_y2_next
  sub.setf -, r5, rb_lcount     ; mul24 r0, r1, ra_wt_mul_l0
  add r0, r0, rb_wt_off         ; mul24 r1, r1, ra_kmul_add
  sub r1, r0, r1                ; v8subs r0, ra_height, r3              # ; NxtLoop: r0 = remaining height (0 saturate)

  brr.anyn -, r:1b
  asr r1, r1, i_wt_den_p6
  min r1, r1, ra_pmax           ; mov -, vw_wait
  max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch                # ; NxtLoop
# >>> branch.anyn 1b (r5 - rb_lcount)

# r0 = remaining height (min 0)
# r2 = r3 * rb_pitch
# r3 = block_height

# If looping again then we consumed 16 height last loop
# rb_dma1 (stride) remains constant
# rb_i_tmu remains const (based on total height)
# recalc ra_dma0, rb_lcount based on new segment height

  mov.setf ra_height, r0        ; mov vw_setup, ra_dma0 # VDW setup 0

# DMA out
  bra.anyz -, ra_link
  min r0, r0, r3                ; mov vw_setup, rb_dma1 # Stride
  sub r1, r0, r3                ; mov vw_addr, ra_dest  # start the VDW
  shl r1, r1, i_shift23
# >>> .anyz ra_link

# Here r1 = cur_blk_height - 16 so it will be 0 or -ve
# We add to dma0 to reduce the number of output lines in the final block
  brr -, r:1b
  add rb_lcount, rb_lcount, r0
  add ra_dma0, ra_dma0, r1
  add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init     # ; Reset our VDM write pointer
# >>> 1b
.endm

::mc_filter_y_pxx
  m_filter_y_pxx 8


################################################################################

# mc_filter_b(y_x, base, y2_x2, base2, width_height, my2_mx2_my_mx, offsetweight0, this_dst, next_kernel)
#
# Setup (& therefore uniform struct) shared with _pxx
# Struct in m_luma_setup
#
# l0 calc in els 0-7, L1 in 8-15
# Only els 0-7 write data that is stored back to ram (els 8-15 may write tosh)
#
# At this point we have already issued PREREAD pairs of texture requests for the current block

.macro m_filter_y_bxx, v_bit_depth

# denom shift values
.set i_wt_den_p5,                  (DENOM + 13 - v_bit_depth)
.set i_wt_den_p6,                  (DENOM + 14 - v_bit_depth)

  m_luma_setup v_bit_depth

  shl r1, ra_wt_off_l0, i_wt_den_p6
  add ra_wt_mul_l0, ra_wt_mul_l0, r3 ; mul24 r0, r2, ra_wt_mul_l0
  sub r1, r1, r0                ; mul24 r0, r2, ra_wt_mul_l1
  sub rb_wt_off, r1, r0         ; mov ra_ef.8a, rb4

# This loop is identical to the P loop from here --->
:1
  add.setf -, ra_ef, ra_ef      ; mul24 ra4, rb5, ra_ef

  max r2, ra_y, 0               ; mov r1, 0
  min r2, r2, rb_max_y          ; mov r3, ra_k1
  add ra_y, ra_y, r3            ; mul24 r2, r2, rb_pitch        ; ldtmu0
  add t0s, ra_base, r2          ; mov rb5,  rb6
  shr r0, r4, ra_xshift         ; mov rb6,  rb7

  max r2, ra_y2, r1             ; v8min r0, r0, rb_pmask        ; ldtmu1 # ; masks out all but wanted bytes
  shr r1, r4, rb_xshift2        ; mov rb7, ra8
  min r2, r2, rb_max_y          ; v8min r1, r1, ra_pmax
  add ra_y2, ra_y2, r3          ; mul24 r2, r2, rb_pitch
  add t1s, rb_base2, r2         ; mov ra8,  ra9

# apply horizontal filter
  add r5rep, r5, r3     ; mul24      r2, ra0.8a << 8,  r1 << 8  @ "mul_used", 0
  mov r3, rb_fir_off_h  ; mul24.ifnn r2, ra0.8a,       r0
  sub r2, r3, r2        ; mul24      r3, ra0.8b << 1,  r0 << 1  @ "mul_used", 0
  nop                   ; mul24.ifn  r3, ra0.8b << 9,  r1 << 9  @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra0.8c << 2,  r0 << 2  @ "mul_used", 0
  nop                   ; mul24.ifn  r3, ra0.8c << 10, r1 << 10 @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra0.8d << 3,  r0 << 3  @ "mul_used", 0
  nop                   ; mul24.ifn  r3, ra0.8d << 11, r1 << 11 @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra1.8a << 4,  r0 << 4  @ "mul_used", 0
  nop                   ; mul24.ifn  r3, ra1.8a << 12, r1 << 12 @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra1.8b << 5,  r0 << 5  @ "mul_used", 0
  nop                   ; mul24.ifn  r3, ra1.8b << 13, r1 << 13 @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra1.8c << 6,  r0 << 6  @ "mul_used", 0
  nop                   ; mul24.ifn  r3, ra1.8c << 14, r1 << 14 @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra1.8d << 7,  r0 << 7  @ "mul_used", 0
  add.setf -, r5, r5    ; mul24.ifn  r3, ra1.8d << 15, r1 << 15 @ "mul_used", 0

  brr.anyn -, r:1b
  sub r2, r2, r3                ; mul24 r1, rb5,  ra2.8b
  mov ra9,  rb10                ; mul24 r0, rb10, ra3.8b
  asr ra11, r2, v_bit_depth - 8 ; mov rb10, ra11
  # >>> .anyn 1b (r5 + r5)

  # apply vertical filter and write to VPM
  # - r4* + r5 - r6 + r7 + r8 - r9 + r10 - r11

  sub r1, r1, r0                ; mul24 r0, rb6,  ra2.8c
  sub r1, r1, r0                ; mul24 r0, rb7,  ra2.8d
  add r1, r1, r0                ; mul24 r0, ra8,  rb8
  add r1, r1, r0                ; mul24 r0, rb10, ra3.8c
  add r1, r1, r0                ; mul24 r0, ra11, rb11
# <--- to here
  sub r1, r1, ra4
  sub r1, r1, r0                ; mov r2, rb_wt_off

  asr r1, r1, 6
  sub.setf -, r5, rb_i_tmu      ; mul24 r0, r1, ra_wt_mul_l0
  mov.ifz rb_base2, rb_base2_next ; mul24 r1, r1, ra_kmul_add
  sub r1, r0, r1                ; mov.ifz ra_y_y2, ra_y_y2_next
  sub.setf -, r5, rb_lcount     ; mov.ifz ra_base, ra_base_next
  add r1, r1, r2                ; mov r0, r1 << 8
  add r1, r1, r0                ; mov r3, ra_blk_height         # ; NxtLoop: r3 = block height

  brr.anyn -, r:1b
  asr r1, r1, ra_wt_den_p7      ; mul24 r2, r3, rb_pitch        # ; NxtLoop
  min r1, r1, ra_pmax           ; mov -, vw_wait
  max vpm, r1, 0                ; v8subs r0, ra_height, r3      # ; NxtLoop: r0 = remaining height (0 saturate)
# >>> branch.anyn 1b (r5 - rb_lcount)

# r0 = remaining height (min 0)
# r2 = r3 * rb_pitch
# r3 = block_height

# If looping again then we consumed block_height last loop
# rb_dma1 (stride) remains constant
# rb_i_tmu remains const (based on total height)
# recalc ra_dma0, rb_lcount based on new segment height

  mov.setf ra_height, r0        ; mov vw_setup, ra_dma0         # VDW setup 0

# DMA out
  bra.anyz -, ra_link
  min r0, r0, r3                ; mov vw_setup, rb_dma1         # Stride
  sub r1, r0, r3                ; mov vw_addr, ra_dest          # start the VDW
  shl r1, r1, i_shift23
# >>> .anyz ra_link (ra_height - remaining height)

# Here r1 = cur_blk_height - blk_height so it will be 0 or -ve
# We add to dma0 to reduce the number of output lines in the final block
  brr -, r:1b
  add rb_lcount, rb_lcount, r0
  add ra_dma0, ra_dma0, r1
  add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init     # ; Reset our VDM write pointer
# >>> 1b
.endm

::mc_filter_y_bxx
  m_filter_y_bxx 8

################################################################################
#
# typedef struct qpu_mc_pred_y_p00_s {
#    qpu_mc_src_t next_src1;
#    uint16_t h;
#    uint16_t w;
#    uint32_t wo1;
#    uint32_t dst_addr;
#    uint32_t next_fn;
# } qpu_mc_pred_y_p00_t;

.macro m_filter_y_p00, v_bit_depth

.if v_bit_depth <= 8
.set v_x_shift,         0
.set v_x_mul,           1
# Shifts to get width & height in the right place in ra_dma0
.set v_dma_h_shift,     7
.set v_dma_wh_shift,    i_shift16
.else
.set v_x_shift,         1
.set v_x_mul,           2
# Shifts to get width & height in the right place in ra_dma0
.set v_dma_h_shift,     8
.set v_dma_wh_shift,    15
.endif

  mov ra0, unif                 ; mov r0, elem_num              # y_x
  mov ra_xshift, ra_xshift_next ; v8subs r5rep, r5, r5          # [ra0 delay] ; r5 = 0
  add r0, ra0.16b, r0           ; mov ra_base_next, unif        # ; src1.base
.if v_x_shift != 0
  shl r0, r0, v_x_shift
.endif

  max r0, r0, r5                ; mov ra_y_next, ra0.16a        # ; width_height
  min r0, r0, rb_max_x          ; mov ra_width_height, unif

  shl ra_xshift_next, r0, 3                                     # Compute shifts
  and r0, r0, -4
  sub r2, r5, rb_pitch          ; mov ra_wt_off_mul_l0, unif    # ; weight_offset
  and r1, r0, r2
  xor r0, r0, r1                ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1                ; mov ra_dest, unif             # Add stripe offsets ; dest addr
  add ra_base_next, ra_base_next, r0 ; mov vw_setup, rb_vpm_init  # [ra_width delay] ; set up VPM write

# get width,height of block (unif load above)
# Compute vdw_setup1(dst_pitch-width)
  shl r1, ra_width, v_x_shift
  sub rb_dma1, rb_dma1_base, r1 ; mov r0, ra_height
  sub rb_i_tmu, r0, PREREAD     ; v8min r0, r0, ra_blk_height
  shl r0, r0, v_dma_h_shift     ; mov rb_lcount, r0
  add r0, r0, r1                                                # Combine width and height of destination area
  shl rb_wt_off, ra_wt_off_l0, DENOM + 7
  shl r0, r0, v_dma_wh_shift    ; mov ra_link, unif             # Shift into bits 16 upwards of the vdw_setup0 register ; link
  add ra_dma0, r0, rb_dma0_base

:1
  sub.setf -, r5, rb_i_tmu      ; v8adds r5rep, r5, ra_k1
  nop                           ; mov.ifz ra_y, ra_y_next       ; ldtmu0
  shr r0, r4, ra_xshift         ; mov r3, rb_pitch

  max r2, ra_y, 0  # y
  min r2, r2, rb_max_y          ; mov.ifz ra_base, ra_base_next
  add ra_y, ra_y, 1             ; mul24 r2, r2, r3
  add t0s, ra_base, r2          ; v8min r0, r0, rb_pmask

  sub.setf -, r5, rb_lcount     ; mul24 r1, r0, ra_wt_mul_l0
  shl r1, r1, 8                 ; mov r3, ra_blk_height
  add r1, r1, rb_wt_off         ; v8subs r0, ra_height, r3

  brr.anyn -, r:1b
  asr r1, r1, DENOM + 8
  min r1, r1, ra_pmax           ; mov -, vw_wait
  max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch
# >>> branch.anyn 1b

# r0 = remaining height (min 0)
# r2 = r3 * rb_pitch
# r3 = block_height

# If looping again then we consumed 16 height last loop
# rb_dma1 (stride) remains constant
# rb_i_tmu remains const (based on total height)
# recalc ra_dma0, rb_lcount based on new segment height

  mov.setf ra_height, r0 ; mov vw_setup, ra_dma0 # VDW setup 0

# DMA out
  bra.anyz -, ra_link
  min r0, r0, r3        ; mov vw_setup, rb_dma1 # Stride
  sub r1, r0, r3        ; mov vw_addr, ra_dest  # start the VDW
  shl r1, r1, i_shift23
# >>> .anyz ra_link

# Here r1 = cur_blk_height - 16 so it will be 0 or -ve
# We add to dma0 to reduce the number of output lines in the final block
  brr -, r:1b
  add rb_lcount, rb_lcount, r0
  add ra_dma0, ra_dma0, r1
  add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init     # ; Reset our VDM write pointer
# >>> 1b
.endm

::mc_filter_y_p00
  m_filter_y_p00 8

################################################################################

.macro m_filter_y_b00, v_bit_depth
# luma setup does a fair bit more than we need calculating filter coeffs
# that we will never use but it saves I-cache to use it (also simple!)
  m_luma_setup v_bit_depth

# Fix up vals that were expecting a filter (somewhat icky)
  mov r2, 1
  add rb_i_tmu, rb_i_tmu, r2    ; mov r1, ra_wt_off_mul_l0      # Need in rX rather than raX for <<8 to do what we want
  shl rb_wt_off, ra_wt_off_l0, DENOM + 8 ; v8subs r5quad, r5, r5 # [r1 << delay] ; r5quad OK for zero
  nop                           ; mov.ifnz ra_wt_off_mul_l0, r1 << 8

:1
  sub.setf -, r5, rb_i_tmu      ; nop                           ; ldtmu1
  shr r1, r4, rb_xshift2        ; mov.ifz ra_y_y2, ra_y_y2_next ; ldtmu0
  shr r0, r4, ra_xshift         ; mov r3, rb_pitch

  max r2, ra_y, 0  # y
  min r2, r2, rb_max_y          ; mov.ifz ra_base, ra_base_next
  add ra_y, ra_y, 1             ; mul24 r2, r2, r3
  add t0s, ra_base, r2          ; mov.ifz rb_base2, rb_base2_next

  max r2, ra_y2, 0
  min r2, r2, rb_max_y
  add ra_y2, ra_y2, 1           ; mul24 r2, r2, r3
  add t1s, rb_base2, r2         ; v8min r0, r0, ra_pmax         # v8subs masks out all but bottom byte
  and r1, r1, rb_pmask          ; mul24 r0, r0, ra_wt_mul_l0

  sub.setf -, r5, rb_lcount     ; mul24 r1, r1, ra_wt_mul_l1
  add r1, r0, r1                ; v8adds r5rep, r5, ra_k1

  shl r1, r1, 8                 ; mov r3, ra_blk_height
  add r1, r1, rb_wt_off         ; v8subs r0, ra_height, r3

  brr.anyn -, r:1b
  asr r1, r1, (DENOM + 9) - 32                                  # -32 to get valid shift immediate
  min r1, r1, ra_pmax           ; mov -, vw_wait
  max vpm, r1, ra_k0            ; mul24 r2, r3, rb_pitch
# >>> branch.anyn 1b

# r0 = remaining height (min 0)
# r2 = r3 * rb_pitch
# r3 = block_height

# If looping again then we consumed 16 height last loop
# rb_dma1 (stride) remains constant
# rb_i_tmu remains const (based on total height)
# recalc ra_dma0, rb_lcount based on new segment height

  mov.setf ra_height, r0        ; mov vw_setup, ra_dma0         # ; VDW setup 0

# DMA out
  bra.anyz -, ra_link
  min r0, r0, r3                ; mov vw_setup, rb_dma1         # ; Stride
  sub r1, r0, r3                ; mov vw_addr, ra_dest          # ; start the VDW
  shl r1, r1, i_shift23
# >>> .anyz ra_link

# Here r1 = cur_blk_height - 16 so it will be 0 or -ve
# We add to dma0 to reduce the number of output lines in the final block
  brr -, r:1b
  add rb_lcount, rb_lcount, r0
  add ra_dma0, ra_dma0, r1
  add ra_dest, ra_dest, r2      ; mov vw_setup, rb_vpm_init     # ; Reset our VDM write pointer
# >>> 1b
.endm

::mc_filter_y_b00
  m_filter_y_b00 8

################################################################################
################################################################################
# 10 BIT

::mc_setup_c10_q0
  m_setup_q0
::mc_setup_c10_qn
  m_setup_c 10

::mc_filter_c10_p
  m_filter_c_p 0, 10

::mc_filter_c10_p_l1
  m_filter_c_p 1, 10


::mc_filter_c10_b
  m_filter_c_b 10

# Even if these fns are the same as for other bit depths we want our own copy
# to keep the code we are using in a single lump to avoid (direct map) cache
# thrashing
.set v_quads10, N_QPU_16 / 4

::mc_sync10_q0
  m_sync_q 0, v_quads10
::mc_sync10_q1
  m_sync_q 1, v_quads10
::mc_sync10_q2
  m_sync_q 2, v_quads10
::mc_sync10_q3
  m_sync_q 3, v_quads10
::mc_sync10_q4
  m_sync_q 4, v_quads10
::mc_sync10_q5
  m_sync_q 5, v_quads10
::mc_sync10_q6
  m_sync_q 6, v_quads10
::mc_sync10_q7
  m_sync_q 7, v_quads10
::mc_sync10_q8
  m_sync_q 8, v_quads10
::mc_sync10_q9
  m_sync_q 9, v_quads10
::mc_sync10_q10
  m_sync_q 10, v_quads10
::mc_sync10_q11
  m_sync_q 11, v_quads10

::mc_exit_y10_q0
::mc_exit_c10_q0
  m_exit_q0

::mc_exit_y10_qn
::mc_exit_c10_qn
  m_exit_qn

::mc_setup_y10_q0
  m_setup_q0
::mc_setup_y10_qn
  m_setup_y 10

:per_block_setup_10
  m_per_block_setup 10

::mc_filter_y10_pxx
  m_filter_y_pxx 10

::mc_filter_y10_p00
  m_filter_y_p00 10

::mc_filter_y10_bxx
  m_filter_y_bxx 10

::mc_filter_y10_b00
  m_filter_y_b00 10



::mc_end
# Do not add code here because mc_end must appear after all other code.
