
# The @ "mul_used", 0 annotations that occur by various mul blocks suppress
# the warning that we are using rotation & ra/rb registers. r0..3 can be
# rotated through all 16 elems ra regs can only be rotated through their
# local 4.  As it happens this is what is wanted here as we do not want the
# constants from the other half of the calc.

# PREREAD is the number of requests that we have sitting in the TMU request
# queue.
#
# There are 8 slots availible in the TMU request Q for tm0s requests, but
# only 4 output FIFO entries and overflow is bad (corruption or crash)
# (If threaded then only 2 out FIFO entries, but we aren't.)
# In s/w we are effectively limited to the min vertical read which is >= 4
# so output FIFO is the limit.
#
# However in the current world there seems to be no benefit (and a small
# overhead) in setting this bigger than 2.

.set PREREAD, 2


# register allocation
#

# ra0-3
# Used as temp and may be loop filter coeffs (split into .8s)
# or temp in loop. Check usage on an individual basis.

# ra4-7
# C:   L0 H filter out FIFO
# otherwise -- free --

# ra8-11
# temp in some places - check usage
# Y:   (with rb8-11) horiz out FIFO

# ra12-15
# -- free --

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
.set ra_kff100100,                 ra20
.set ra_k256,                      ra20.16a
.set ra_k0,                        ra20.8a
.set ra_k1,                        ra20.8b
.set ra_k16,                       ra20.8c
.set ra_k255,                      ra20.8d

# Loop: xshifts
.set ra_xshift,                    ra21.16a
.set ra_xshift_next,               ra21.16b

# Loop var: L0 weight (U on left, V on right)
# _off_ is not used in loop as we want to modify it before use
.set ra_wt_off_mul_l0,             ra22
.set ra_wt_mul_l0,                 ra22.16a
.set ra_wt_off_l0,                 ra22.16b

# Max pel value (for 8 bit we can get awaay with sat ops but not 9+)
.set ra_pmax,                      ra23.16a
# -- free --                       ra23.16b

# Loop:  src frame base (L0)
.set ra_base,                      ra24

# Loop: src frame base (L1)
.set ra_base2,                     ra25

# Loop: next src frame base (L0)
.set ra_base_next,                 ra26

# -- free --                       ra27
# -- free --                       ra28
# -- free --                       ra29

# Use an even numbered register as a link register to avoid corrupting flags
.set ra_link,                      ra30

# -- free --                       ra31

.set rb_xshift2,                   rb0
.set rb_xshift2_next,              rb1

# C:  (elem & 1) == 0 ? elem * 2 : (elem + 4) * 2
.set rb_elem_x,                    rb2

# rb3
# C: Temp (U/V flag)
# Y: free

# rb4-7
# C-B: L1 H filter out FIFO
# Y:   (with ra2.8x) Y vertical filter coeffs

# rb8-11
# C:   Vertical filter coeffs
# Y:   (with ra8-11) horiz out FIFO

# Loop var: offset to add before shift (round + weighting offsets)
# Exact value varies by loop
.set rb_wt_off,                    rb12

# Setup: denom + 6 + 9
.set rb_wt_den_p15,                rb13

# -- free --                       rb14
# -- free --                       rb15

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

# -- free --                       rb21

# Setup: 0xff (8-bit) / 0xffff (9+ bit)
.set rb_pmask,                     rb22

# Loop: destination address
.set rb_dest,                      rb23

# vdw_setup_1(dst_pitch)
.set rb_dma1_base,                 rb24

# Setup: pic width - 1
# In bytes so 8 bit luma is (width - 1)*1, 16 bit chroma is (width -1)*4 etc.
.set rb_max_x,                     rb25

# Loop: height<<23 + width<<16 + vdw_setup_0
.set rb_dma0,                      rb26

# vdw_setup_0 (depends on QPU number)
.set rb_dma0_base,                 rb27

# Setup: vw_setup value to reset VPM write pointer
.set rb_vpm_init,                  rb28

# Loop: vdw_setup_1(dst_pitch-width) = stride
.set rb_dma1,                      rb29

# Setup: pic_height - 1
.set rb_max_y,                     rb30

# -- free --                       rb31




# With shifts only the bottom 5 bits are considered so -16=16, -15=17 etc.
.set i_shift16,                    -16
.set i_shift21,                    -11
.set i_shift23,                     -9
.set i_shift30,                     -2

# Much of the setup code is common between Y & C
# Macros that express this - obviously these can't be overlapped
# so are probably unsuitable for loop code

.macro m_calc_dma_regs, r_vpm, r_dma
  mov r2, qpu_num
  asr r1, r2, 2
  shl r1, r1, 6
  and r0, r2, 3
  or  r0, r0, r1

  mov r1, vpm_setup(0, 4, h8p(0, 0))   # 4 is stride - stride acts on ADDR which is Y[5:0],B[1:0] for 8 bit
  add r_vpm, r0, r1  # VPM 8bit storage

  mov r1, vdw_setup_0(0, 0, dma_h8p(0,0,0)) # height,width added later
  shl r0, r0, 5
  add r_dma, r0, r1  # DMA out
.endm

# 16 bits per pel, 16 line height, 8 QPU max
.macro m_calc_dma_regs_16, r_vpm, r_dma
  mov r2, qpu_num
  asr r1, r2, 1
  shl r1, r1, 5
  and r0, r2, 1
  or  r0, r0, r1

  mov r1, vpm_setup(0, 2, h16p(0, 0))   # 2 is stride - stride acts on ADDR
  add r_vpm, r0, r1  # VPM 8bit storage

  # X = H * 8 so the YH from VPMVCD_WR_SETUP[ADDR] drops into
  # XY VPMVCD_WR_SETUP[VPMBASE] if shifted left 3 (+ 3 for pos of field in reg)
  mov r1, vdw_setup_0(0, 0, dma_h16p(0,0,0))    # height,width added later
  shl r0, r0, 6
  add r_dma, r0, r1                             # DMA out
.endm


.macro m_setup_q0
  srel -, 12
.endm

# Code start label
::mc_start

################################################################################
# mc_setup_uv(next_kernel, x, y, ref_c_base, frame_width, frame_height, pitch, dst_pitch, offset, denom, vpm_id)

.macro m_setup_c, v_bit_depth

# Cannot use mul24 on x as x might be -ve, so must use shift
.if v_bit_depth <= 8
.set v_x_to_el_shift,   3
.set v_x_shift,         1
.set v_pmask,           0xff
.else
# .set v_x_to_el_shift,   3                     # Will alaways produce a shift of zero
.set v_x_shift,         2
.set v_pmask,           0xffff
.endif

  mov tmurs, 1                                  # No swap TMUs

# Load first request location
  mov ra0, unif                                 # next_x_y

  mov ra_base, unif                             # Store frame c base

# Read image dimensions
  sub r0, unif, 1                               # pic c width
  shl rb_max_x, r0, v_x_shift                   # rb_max_x in bytes
  sub rb_max_y, unif, 1                         # pic c height

# load constants
  mov ra_kff100100, 0xff100100
  mov rb_pmask, v_pmask
  mov ra_pmax, (1 << v_bit_depth) - 1

# get source pitch
  mov rb_xpitch, unif                           # stride2
  mov rb_pitch, unif                            # stride1
  mov r1, vdw_setup_1(0)                        # [rb_pitch delay] Merged with dst_stride shortly
  add rb_dma1_base, r1, rb_pitch                # vdw_setup_1

  and r0, 1, elem_num
  nop                   ; mul24 r0, r0, 5
.if v_bit_depth <= 8
  add rb_elem_x, r0, elem_num
.else
  add r0, r0, elem_num
  add rb_elem_x, r0, r0
.endif

# Compute base address for first and second access
# ra_base ends up with t0s base
# ra_base2 ends up with t1s base

  shl r0, ra0.16b, v_x_shift                    # [rb_elem_x delay]
  add r0, r0, rb_elem_x                         # Add elem no to x to get X for this slice
  max r0, r0, 0         ; mov ra_y, ra0.16a     # ; stash Y
  min r0, r0, rb_max_x

# Get shift
# No shift wanted for 9+ bit here so set zero & never change
# Ideally we can optimize the shift out of the code in these cases but for now
# it is tidier to leave it in
.if v_bit_depth <= 8
  shl ra_xshift_next, r0, v_x_to_el_shift
.else
  mov ra_xshift_next, 0 ; mov rb_xshift2_next, 0
.endif

# In a single 32 bit word we get 1 or 2 UV pairs so mask bottom bits of xs if we need to

.if v_bit_depth <= 8
  and r0, r0, -4
.endif
  sub r1, ra_k0, rb_pitch
  and r1, r0, r1
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1
  add ra_base, ra_base, r0

  add rb_wt_den_p15, 17 - v_bit_depth, unif                    # denominator

# Compute part of VPM to use for DMA output
# * We only get 8 QPUs if 16 bit - maybe reduce height and auto-loop?
.if v_bit_depth <= 8
  m_calc_dma_regs rb_vpm_init, rb_dma0_base
.else
  m_calc_dma_regs_16 rb_vpm_init, rb_dma0_base
.endif

# And again for L1, but only worrying about frame2 stuff

# Load first request location
  mov ra0, unif                                 # next_x_y

  mov ra_base2, unif                            # [ra0 delay] Store frame c base

# Compute base address for first and second access
# ra_base ends up with t0s base
# ra_base2 ends up with t1s base

  shl r0, ra0.16b, v_x_shift
  add r0, r0, rb_elem_x ; mov ra_y2, ra0.16a    # Add QPU slice offset
  max r0, r0, 0
  min r0, r0, rb_max_x

# Get shift (already zero if 9+ bit so ignore)
.if v_bit_depth <= 8
  shl rb_xshift2_next, r0, v_x_to_el_shift
.endif

# In a single 32 bit word we get 2 UV pairs so mask bottom bit of xs

.if v_bit_depth <= 8
  and r0, r0, -4
.endif
  sub r1, ra_k0, rb_pitch
  and r1, r0, r1
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        ; mov r2, ra_y2
  add ra_base2, ra_base2, r0

# Do preloads
# r0 = ra_y, r2 = ra_y2
  mov r3, PREREAD       ; mov r0, ra_y

:1
  sub.setf r3, r3, 1
  max r1, r0, 0
  min r1, r1, rb_max_y
  add r0, r0, ra_k1     ; mul24 r1, r1, rb_pitch
  add t0s, ra_base, r1  ; mov ra_y, r0

  max r1, r2, 0
  brr.anynz -, r:1b
  min r1, r1, rb_max_y
  add r2, r2, ra_k1     ; mul24 r1, r1, rb_pitch
  add t1s, ra_base2, r1 ; mov ra_y2, r2
# >>> .anynz 1b

  mov ra_link, unif                             # link
# touch registers to keep simulator happy
  # ra/b4..7: B0 -> B stash registers
  mov ra4, 0 ; mov rb4, 0
  bra -, ra_link
  mov ra5, 0 ; mov rb5, 0
  mov ra6, 0 ; mov rb6, 0
  mov ra7, 0 ; mov rb7, 0
# >>> ra_link
.endm

::mc_setup_c_q0
  m_setup_q0
::mc_setup_c_qn
  m_setup_c 8

################################################################################

# mc_filter_uv(next_kernel, x, y, frame_c_base, width_height, hcoeffs, vcoeffs, offset_weight_u, offset_weight_v, this_u_dst, this_v_dst)

# At this point we have already issued two pairs of texture requests for the current block
# ra_x, ra_x16_base point to the current coordinates for this block

.macro m_filter_c_p, v_bit_depth

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

# per-channel shifts were calculated on the *previous* invoca
#
# tion

# get base addresses and per-channel shifts for *next* invocation
  mov vw_setup, rb_vpm_init ; mov ra2, unif     # ; x_y

  and.setf -, elem_num, 1                       # [ra2 delay]

  shl r0, ra2.16b, v_x_shift
  add r0, r0, rb_elem_x ; v8subs r1, r1, r1     # ; r1=0
  sub r1, r1, rb_pitch  ; mov r3, unif          # r1=pitch2 mask ; r3=base
  max r0, r0, 0         ; mov rb_xshift2, ra_xshift_next
  min r0, r0, rb_max_x  ; mov ra1, unif         # ; width_height

.if v_bit_depth <= 8
  shl ra_xshift_next, r0, 3
.endif

.if v_bit_depth <= 8
  and r0, r0, -4        ; mov ra0, unif         # H filter coeffs
.else
  nop                   ; mov ra0, unif         # H filter coeffs
.endif
  nop                   ; mov ra_y_next, ra2.16a # [ra0 delay]
  and r1, r0, r1        ; mul24 r2, ra1.16b, v_x_mul  # r2=w*2 (we are working in pel pairs)  ** x*2 already calced!
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        ; mov r1, ra1.16a       # Add stripe offsets ; r1=height
  add ra_base_next, r3, r0
  shl r0, r1, v_dma_h_shift

# set up VPM write

  sub rb_dma1, rb_dma1_base, r2 ; mov ra3, unif         # Compute vdw_setup1(dst_pitch-width) ; V filter coeffs
  add rb_i_tmu, r1, 3 - PREREAD ; mov ra_wt_off_mul_l0, unif # ; U offset/weight
  add rb_lcount, r1, 3  ; mov.ifnz ra_wt_off_mul_l0, unif    # ; V offset/weight

# ; unpack filter coefficients

  add r0, r0, r2        ; mov rb8, ra3.8a       # Combine width and height of destination area (r0=h<<8, r2=w*2)
  shl r0, r0, v_dma_wh_shift ; mov rb9, ra3.8b       # Shift into bits 16 upwards of the vdw_setup0 register
  add rb_dma0, r0, rb_dma0_base ; mov r1, ra_wt_off_l0       # ; r1=weight

  mov rb_dest, unif     ; mov ra9, rb_max_y     # dst_addr ; alias rb_max_y

  shl r1, r1, rb_wt_den_p15 ; mov rb10, ra3.8c
  mov r5quad, 0         ; mov rb11, ra3.8d      # Loop count (r5rep is B, r5quad is A)

  asr rb_wt_off, r1, 1  ; mov ra_link, unif     # Link
  shl ra_wt_mul_l0, ra_wt_mul_l0, 1             # weight*2

# ra9 alias for rb_max_y
# ra_wt_mul_l0 - weight L0 * 2
# rb_wt_den_p15 = weight denom + 6 + 9
# rb_wt_off = (((is P) ? offset L0 * 2 : offset L1 + offset L0) + 1) << (rb_wt_den_p15 - 1)

# retrieve texture results and pick out bytes
# then submit two more texture requests

# We want (r0r1)
# U0U3 : V0V3 : U1U4 : V1V4 : U2U5 : V2U5 : ...
# We fetch (after shift)
#  C0  :  C3  :  C1  :  C4  :  C2  :  C5  : ...

  mov rb3, [0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1]

# r5 = 0 (loop counter)
:1
# retrieve texture results and pick out bytes
# then submit two more texture requests

  sub.setf -, r5, rb_i_tmu ; v8adds r5rep, r5, ra_k1 ; ldtmu0     # loop counter increment
  shr r2, r4, rb_xshift2 ; mov.ifz r3, ra_y_next
  shr r1, r2, v_v_shift ; mov.ifnz r3, ra_y
  add r0, r3, 1         ; mov.ifz ra_base, ra_base_next

  and.setf -, 1, elem_num ; mov ra_y, r0
  max r3, r3, ra_k0     ; mov      r0, r1 << 15
  min r3, r3, ra9       ; mov.ifz  r1, r2 << 1

  mov.ifz r0, r2        ; mul24 r2, r3, rb_pitch
  add t0s, ra_base, r2  ; v8min r0, r0, rb_pmask  # v8subs masks out all but bottom byte

# ra4 not really needed; this could be a mul24 rather than a mov but current
# register usage means this wouldn't help
  mov.setf -, rb3       ; mov ra4, ra5

# apply horizontal filter
# The filter coeffs for the two halves of this are the same (unlike in the
# Y case) so it doesn't matter which ra0 we get them from
# Also as the two halves are locked together we don't need to separate the 1st
# r0 mul or the last r1 mul as they are vaild for all QPUs

  and r1, r1, rb_pmask  ; mul24      r3, ra0.8a,       r0
  nop                   ; mul24      r2, ra0.8b << 2,  r0 << 2  @ "mul_used", 0
  nop                   ; mul24.ifnz r2, ra0.8b << 12, r1 << 12 @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra0.8c << 4,  r0 << 4  @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8c << 14, r1 << 14 @ "mul_used", 0
  sub.setf -, r5, 4     ; mul24      r0, ra0.8d      , r1
# V filter =- ra4 * rb8-+ ra5 * rb9 + ra6 * rb10 - ra7 * rb11 (post FIFO shift)
# Have to dup block as we need to move the brr - code is more common than it
# looks at first glance
.if v_bit_depth <= 8
  brr.anyn -, r:1b
  add r2, r2, r3        ; mov ra5, ra6
  mov ra6, ra7          ; mul24 r1, ra7, rb10
  sub ra7, r2, r0       ; mul24 r0, ra4, rb8
.else
  add r2, r2, r3        ; mov ra5, ra6
  brr.anyn -, r:1b
  mov ra6, ra7          ; mul24 r1, ra7, rb10
  sub r2, r2, r0        ; mul24 r0, ra4, rb8
  asr ra7, r2, v_bit_depth - 8
.endif
# >>> .anyn 1b

  sub r1, r1, r0        ; mul24 r0, ra5, rb9    # [ra7 delay]
  add r1, r1, r0        ; mul24 r0, ra7, rb11
  sub r1, r1, r0
  sub.setf -, r5, rb_lcount ; mul24 r1, r1, ra_k256
  asr r1, r1, 14
  nop                   ; mul24 r1, r1, ra_wt_mul_l0
  shl r1, r1, 8

  add r1, r1, rb_wt_off
  brr.anyn -, r:1b
  asr r1, r1, rb_wt_den_p15
  min r1, r1, ra_pmax   ; mov -, vw_wait
  max vpm, r1, 0
# >>> .anyn 1b

# DMA out for U & stash for V
  bra -, ra_link
  mov vw_setup, rb_dma0
  mov vw_setup, rb_dma1
  mov vw_addr, rb_dest
# >>> ra_link
.endm

::mc_filter_uv
  m_filter_c_p 8

################################################################################

# mc_filter_uv_b0(next_kernel, x, y, frame_c_base, height, hcoeffs[0], hcoeffs[1], vcoeffs[0], vcoeffs[1], this_u_dst, this_v_dst)

# At this point we have already issued two pairs of texture requests for the current block
# ra_x, ra_x16_base point to the current coordinates for this block

.macro m_filter_c_b, v_bit_depth

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

# per-channel shifts were calculated on the *previous* invocation

# get base addresses and per-channel shifts for *next* invocation
  mov vw_setup, rb_vpm_init ; mov ra2, unif     # ; x_y

  and.setf -, elem_num, 1                       # [ra2 delay]

  add r0, ra2.16b, ra2.16b ; v8subs r1, r1, r1  # x ; r1=0
  add r0, r0, rb_elem_x ; mov ra_y_next, ra2.16a
  sub r1, r1, rb_pitch  ; mov r3, unif          # r1=pitch2 mask ; r3=base
  max r0, r0, 0         ; mov ra_xshift, ra_xshift_next
  min r0, r0, rb_max_x  ; mov ra1, unif         # ; width_height

.if v_bit_depth <= 8
  shl ra_xshift_next, r0, 3
.endif

  and r0, r0, -4        ; mov ra0, unif         # L0 H filter coeffs
  and r1, r0, r1        ; mul24 r2, ra1.16b, 2  # r2=x*2 (we are working in pel pairs)
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        ; mov r1, ra1.16a       # Add stripe offsets ; r1=height
  add ra_base_next, r3, r0
  shl r0, r1, v_dma_h_shift ; mov ra2, unif         # ; L0 V filter coeffs

# set up VPM write

  sub rb_dma1, rb_dma1_base, r2                 # Compute vdw_setup1(dst_pitch-width)
  add rb_i_tmu, r1, 3 - PREREAD
  add rb_lcount, r1, 3

  add r0, r0, r2        ; mov ra_wt_mul_l0, unif # ; U weight
  shl r0, r0, v_dma_wh_shift ; mov.ifnz ra_wt_mul_l0, unif  # Shift into bits 16 upwards of the vdw_setup0 register ; V weight
  add rb_dma0, r0, rb_dma0_base ; mov ra3, unif  # ; x2_y2

# L1 - uniform layout could possibly be optimized

  mov ra9, rb_max_y                             # [ra3 delay]

  add r0, ra3.16b, ra3.16b ; v8subs r1, r1, r1  # r0=x*2 ; r1=0
  add r0, r0, rb_elem_x ; mov ra_y2_next, ra3.16a
  sub r1, r1, rb_pitch  ; mov r3, unif          # r1=pitch2 mask ; r3=base
  max r0, r0, ra_k0     ; mov rb_xshift2, rb_xshift2_next # ; xshift2 used because B
  min r0, r0, rb_max_x  ; mov ra1, unif         # H filter coeffs

.if v_bit_depth <= 8
  shl rb_xshift2_next, r0, 3
.endif

  and r0, r0, -4
  and r1, r0, r1        ; mov ra3, unif         # ; V filter coeffs
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        ; mov rb8,  ra3.8a      # Add stripe offsets ; start unpacking filter coeffs
  add rb_base2_next, r3, r0

  mov ra_wt_off_mul_l1, unif        ; mov rb9,  ra3.8b      # U offset/weight
  mov.ifnz ra_wt_off_mul_l1, unif   ; mov rb10, ra3.8c      # V offset/weight

  mov rb_dest, unif                             # dst_addr
  mov r5quad,0          ; mov rb11, ra3.8d
  shl r1, ra_wt_off_l1, rb_wt_den_p15
  asr rb_wt_off, r1, 9  ; mov ra_link, unif     # link

# r5        loop counter
# ra0       H coeffs L0
# ra1       H coeffs L1
# ra2       V coeffs L0
# ra3       temp
# ra4-7     L0 H FIFO
# rb4-7     L1 H FIFO
# rb8-rb11  V coeffs L1
# ra9       rb_max_y alias

  mov rb3, [0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1]

:1
# retrieve texture results and pick out bytes
# then submit two more texture requests
  sub.setf -, r5, rb_i_tmu ; v8adds r5rep, r5, ra_k1 ; ldtmu0     # loop counter increment
  shr r2, r4, ra_xshift ; mov.ifz ra_base2, rb_base2_next
  shr r1, r2, 8         ; mov.ifz ra_y_y2, ra_y_y2_next
  mov rb4, rb5          ; mov.ifz ra_base, ra_base_next
  add ra_y, 1, ra_y     ; mov r3, ra_y

  and.setf -, 1, elem_num
  max r3, r3, ra_k0     ; mov      r0, r1 << 15
  min r3, r3, ra9       ; mov.ifz  r1, r2 << 1

  mov.ifz r0, r2        ; mul24 r3, r3, rb_pitch
  add t0s, ra_base, r3  ; v8min r0, r0, rb_pmask  # v8subs masks out all but bottom byte

# L0 H-filter
# H FIFO scrolls are spread all over this loop
  mov.setf -, rb3       ; mov ra4, ra5

  and r1, r1, rb_pmask   ; mul24      r3, ra0.8a,       r0
  nop                   ; mul24      r2, ra0.8b << 2,  r0 << 2  @ "mul_used", 0
  nop                   ; mul24.ifnz r2, ra0.8b << 12, r1 << 12 @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra0.8c << 4,  r0 << 4  @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8c << 14, r1 << 14 @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra0.8d,       r1
  sub ra3, r2, r3       ; mov rb5, rb6          ; ldtmu1

  shr r2, r4, rb_xshift2 ; mov ra5, ra6
  shr r1, r2, 8         ; mov r3, ra_y2
  add ra_y2, r3, ra_k1  ; mov rb6, rb7

  and.setf -, 1, elem_num
  max r3, r3, ra_k0     ; mov      r0, r1 << 15
  min r3, r3, ra9       ; mov.ifz  r1, r2 << 1

  mov.ifz r0, r2        ; mul24 r3, r3, rb_pitch
  add t1s, ra_base2, r3 ; v8min r0, r0, rb_pmask  # v8subs masks out all but bottom byte

# L1 H-filter
  mov.setf -, rb3       ; mov rb7, ra3

  and r1, r1, rb_pmask   ; mul24      r3, ra1.8a,       r0
  nop                   ; mul24      r2, ra1.8b << 2,  r0 << 2  @ "mul_used", 0
  nop                   ; mul24.ifnz r2, ra1.8b << 12, r1 << 12 @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra1.8c << 4,  r0 << 4  @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra1.8c << 14, r1 << 14 @ "mul_used", 0
  sub.setf -, r5, 4     ; mul24      r0, ra1.8d,       r1
  brr.anyn -, r:1b
# V filters - start in branch delay slots of H
  add r2, r2, r3        ; mul24 r1, rb5, ra2.8b
  mov ra6, ra7          ; mul24 r3, ra7, rb10
  sub ra7, r2, r0       ; mul24 r0, rb4, ra2.8a
# >>> .anyn 1b

  sub r1, r1, r0        ; mul24 r0, rb6, ra2.8c
  add r1, r1, r0        ; mul24 r0, rb7, ra2.8d
  sub r2, r1, r0        ; mul24 r0, ra4, rb8
  sub r1, r3, r0        ; mul24 r0, ra5, rb9
  add r1, r1, r0        ; mul24 r0, ra7, rb11
  sub r1, r1, r0        ; mul24 r2, r2, ra_k256

  asr r2, r2, 14        ; mul24 r1, r1, ra_k256
  asr r1, r1, 14        ; mul24 r2, r2, ra_wt_mul_l0

  add r2, r2, rb_wt_off ; mul24 r1, r1, ra_wt_mul_l1    # rb_wt_off = (offsetL0 + offsetL1 + 1) << (rb_wt_den_p15 - 9)
  add r1, r1, r2

  sub.setf -, r5, rb_lcount ; mul24 r1, r1, ra_k256     # Lose bad top 8 bits & sign extend

  brr.anyn -, r:1b
  asr ra3.8as, r1, rb_wt_den_p15
  mov -, vw_wait
  mov vpm, ra3.8a
# >>> .anyn 1b

# DMA out
  bra -, ra_link
  mov vw_setup, rb_dma0
  mov vw_setup, rb_dma1
  mov vw_addr, rb_dest
# >>> ra_link
.endm

::mc_filter_uv_b0
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
.macro m_sync_q, n_qpu
  mov ra_link, unif
  mov -, vw_wait

.set n_sem_sync, n_qpu - (n_qpu % 4)
.set n_sem_in, n_qpu
.set n_sem_out, n_qpu + 1

.if n_qpu % 4 == 0

.set n_sem_quad_in,  12 + n_qpu / 4
.set n_sem_quad_out, 12 + (((n_qpu / 4) + 1) % 3)

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
.endm

::mc_sync_q0
  m_sync_q 0
::mc_sync_q1
  m_sync_q 1
::mc_sync_q2
  m_sync_q 2
::mc_sync_q3
  m_sync_q 3
::mc_sync_q4
  m_sync_q 4
::mc_sync_q5
  m_sync_q 5
::mc_sync_q6
  m_sync_q 6
::mc_sync_q7
  m_sync_q 7
::mc_sync_q8
  m_sync_q 8
::mc_sync_q9
  m_sync_q 9
::mc_sync_q10
  m_sync_q 10
::mc_sync_q11
  m_sync_q 11

# mc_exit()
# Chroma & Luma the same now
::mc_exit_c
::mc_exit
  m_exit_drain
  nop                   ; nop           ; thrend
  nop
  nop

# mc_interrupt_exit12()
::mc_interrupt_exit12c
::mc_interrupt_exit12
  m_exit_drain
  sacq -, 12
  nop                   ; nop           ; thrend
  mov interrupt, 1
  nop
# >>> thrend <<<

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

::mc_setup_y_q0
  m_setup_q0
::mc_setup_y_qn
  # Need to save these because we need to know the frame dimensions before computing texture coordinates
  mov tmurs, 1          ; mov ra0, unif         # No TMU swap ; x_y
  mov ra9, unif                                 # ref_y_base
  mov ra1, unif                                 # x2_y2
  mov ra11, unif                                # ref_y2_base

# load constants

  mov ra_kff100100, 0xff100100
  mov rb_pmask, 255

# Compute part of VPM to use

# Read image dimensions
  mov ra3, unif         # width_height
  mov rb_xpitch, unif   # stride2
  sub rb_max_x, ra3.16b, 1
  sub rb_max_y, ra3.16a, 1
  mov rb_pitch, unif    # stride1

# get destination pitch
  mov r1, vdw_setup_1(0)
  or  rb_dma1_base, r1, rb_pitch

# Compute base address for first and second access
  mov r3, elem_num
  add r0, ra0.16b, r3   # Load x + elem_num
  max r0, r0, 0
  min r0, r0, rb_max_x
  shl ra_xshift_next, r0, 3 # Compute shifts

# In a single 32 bit word we get 4 Y Pels so mask 2 bottom bits of xs

  and r0, r0, -4        ; v8subs r2, r2, r2
  sub r2, r2, rb_pitch
  and r1, r0, r2
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        # Add stripe offsets
  add ra_base, ra9, r0

  # r3 still contains elem_num
  add r0, ra1.16b, r3  # Load x
  max r0, r0, 0
  min r0, r0, rb_max_x
  shl rb_xshift2_next, r0, 3 # Compute shifts

  # r2 still contains mask
  and r0, r0, -4
  and r1, r0, r2
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        # Add stripe offsets
  add ra_base2, ra11, r0

# Do preloads
  nop                   ; mov r0, ra0.16a       # ; r0 = y
  mov r3, PREREAD       ; mov r2, ra1.16a       # ; r2 = y2

:y_preload
  sub.setf r3, r3, 1
  max r1, r0, 0
  min r1, r1, rb_max_y
  add r0, r0, ra_k1     ; mul24 r1, r1, rb_pitch
  add t0s, ra_base, r1  ; mov ra_y, r0

  max r1, r2, 0
  brr.anynz -, r:y_preload
  min r1, r1, rb_max_y
  add r2, r2, ra_k1     ; mul24 r1, r1, rb_pitch
  add t1s, ra_base2, r1 ; mov ra_y2, r2
# >>> .anynz y_preload

  add rb_wt_den_p15, unif, 9                    # weight denom + 6

  m_calc_dma_regs rb_vpm_init, rb_dma0_base

  mov ra_link, unif                             # Next fn

# touch vertical context to keep simulator happy
  mov ra8,  0           ; mov rb8,  0
  bra -, ra_link
  mov ra9,  0           ; mov rb9,  0
  mov ra10, 0           ; mov rb10, 0
  mov ra11, 0           ; mov rb11, 0
# >>> ra_link

################################################################################
#
# Start of per-block setup code
# P and B blocks share the same setup code to save on Icache space

# luma_setup_delay3 done in delay slots of branch that got us here

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

.macro luma_setup
  brr ra_link, r:per_block_setup
  mov ra0, unif         ; mov r3, elem_num  # y_x ; elem_num has implicit unpack??
  mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1] # [ra0 delay]
  add r0, ra0.16b, r3   ; mov rb_xshift2, rb_xshift2_next
.endm

:per_block_setup
  max r0, r0, 0         ; mov ra_xshift, ra_xshift_next
  min r0, r0, rb_max_x

  shl ra_xshift_next, r0, 3         # Compute shifts
  and r0, r0, -4        ; v8subs r2, r2, r2
  sub r2, r2, rb_pitch  ; mov ra_base_next, unif # src1.base
  and r1, r0, r2        ; mov ra_y_next, ra0.16a
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        ; mov ra1, unif         # Add stripe offsets ; src2.x_y
  add ra_base_next, ra_base_next, r0            # [ra1 delay]

  add r0, ra1.16b, r3                           # Load x2
  max r0, r0, 0         ; mov ra_y2_next, ra1.16a
  min r0, r0, rb_max_x  ; mov rb_base2_next, unif # ; src2.base
  shl rb_xshift2_next, r0, 3                    # Compute shifts
  and r0, r0, -4        ; mov ra_width_height, unif # ; width_height
  and r1, r0, r2
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        ; mov vw_setup, rb_vpm_init # Add stripe offsets ; set up VPM write
  add rb_base2_next, rb_base2_next, r0

# get width,height of block (unif load above)
  sub rb_dma1, rb_dma1_base, ra_width # Compute vdw_setup1(dst_pitch-width)
  add rb_i_tmu, ra_height, 7 - PREREAD ; mov r0, ra_height
  min r0, r0, ra_k16
  add rb_lcount, r0, 7
  shl r0,   r0, 7
  add r0,   r0, ra_width                        # Combine width and height of destination area
  shl r0,   r0, i_shift16                       # Shift into bits 16 upwards of the vdw_setup0 register
  add rb_dma0, r0, rb_dma0_base ; mov r0, unif  # ; Packed filter offsets

# get filter coefficients and discard unused B frame values
  shl.ifz r0, r0, i_shift16 ; mov ra_wt_off_mul_l0, unif     #  Pick half to use ; L0 offset/weight
  shl ra8, r0, 3        ; mov r3, ra_k255

# Pack the 1st 4 filter coefs for H & V tightly
# Coeffs are all abs values here as that means mul24 works (no sign extend from .8)

  mov r1,0x00010100  # -ve                      [ra8 delay]
  ror ra2.8a, r1, ra8.8d
  ror ra0.8a, r1, ra8.8c

  mov r1, 0x01040400
  ror ra2.8b, r1, ra8.8d
  ror ra0.8b, r1, ra8.8c

  mov r1,0x050b0a00  # -ve
  ror ra2.8c, r1, ra8.8d
  ror ra0.8c, r1, ra8.8c

  mov r1,0x11283a40
  ror ra2.8d, r1, ra8.8d
  ror ra0.8d, r1, ra8.8c

# In the 2nd vertical half we use b registers due to using a-side fifo regs

  mov r1,0x3a281100
  ror r0, r1, ra8.8d    ; mov ra_wt_off_mul_l1, unif
  ror ra1.8a, r1, ra8.8c ; v8min rb4, r0, r3

  mov r1,0x0a0b0500  # -ve
  ror r0, r1, ra8.8d
  ror ra1.8b, r1, ra8.8c ; v8min rb5, r0, r3

  mov r1,0x04040100
  ror r0, r1, ra8.8d
  ror ra1.8c, r1, ra8.8c ; v8min rb6, r0, r3

  mov.ifnz ra_wt_off_mul_l0, ra_wt_off_mul_l1 ; mov rb_dest, unif # ; Destination address

  mov r1,0x01010000  # -ve
  ror r0, r1, ra8.8d
  bra -, ra_link
  ror ra1.8d, r1, ra8.8c ; v8min rb7, r0, r3

  shl r0, ra_wt_off_l0, rb_wt_den_p15 ; v8subs r5rep, r3, r3     # Offset calc ; r5 = 0
  # For B l1 & L0 offsets should be identical so it doesn't matter which we use
  asr rb_wt_off, r0, 9  ; mov ra_link, unif    # ; link - load after we've used its previous val
# >>> branch ra_link

# r3 = 0
# ra_wt_mul_l1  = weight L1
# ra5.16a       = weight L0/L1 depending on side (wanted for 2x mono-pred)
# rb_wt_off     = (((is P) ? offset L0/L1 * 2 : offset L1 + offset L0) + 1) << (rb_wt_den_p15 - 1)
# rb_wt_den_p15 = weight denom + 6 + 9
# rb_wt_mul_l0  = weight L0


################################################################################
# mc_filter(y_x, base, y2_x2, base2, width_height, my2_mx2_my_mx, offsetweight0, this_dst, next_kernel)
# In a P block, y2_x2 should be y_x+8
# At this point we have already issued two pairs of texture requests for the current block

::mc_filter
  luma_setup

  shl ra_wt_mul_l0, ra_wt_mul_l0, 1

# r5 = 0 (loop count)

:yloop
# retrieve texture results and pick out bytes
# then submit two more texture requests

# N.B. Whilst y == y2 as far as this loop is concerned we will start
# the grab for the next block before we finish with this block and that
# might be B where y != y2 so we must do full processing on both y and y2

  sub.setf -, r5, rb_i_tmu      ; v8adds r5rep, r5, ra_k1            ; ldtmu1
  shr r1, r4, rb_xshift2        ; mov.ifz ra_y_y2, ra_y_y2_next      ; ldtmu0
  shr r0, r4, ra_xshift         ; mov r3, rb_pitch

  max r2, ra_y, 0  # y
  min r2, r2, rb_max_y          ; mov.ifz ra_base, ra_base_next
  add ra_y, ra_y, 1             ; mul24 r2, r2, r3
  add t0s, ra_base, r2          ; mov.ifz ra_base2, rb_base2_next

  max r2, ra_y2, 0
  min r2, r2, rb_max_y
  add ra_y2, ra_y2, 1           ; mul24 r2, r2, r3
  add t1s, ra_base2, r2         ; v8min r0, r0, rb_pmask # v8subs masks out all but bottom byte

# generate seven shifted versions
# interleave with scroll of vertical context

  mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

# apply horizontal filter
  and r1, r1, rb_pmask   ; mul24      r3, ra0.8a,      r0
  nop                   ; mul24      r2, ra0.8b << 1, r0 << 1    @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8a << 8, r1 << 8    @ "mul_used", 0
  nop                   ; mul24.ifnz r2, ra0.8b << 9, r1 << 9    @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra0.8c << 2, r0 << 2    @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8c << 10, r1 << 10  @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra0.8d << 3, r0 << 3    @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8d << 11, r1 << 11  @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra1.8a << 4, r0 << 4    @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra1.8a << 12, r1 << 12  @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra1.8b << 5, r0 << 5    @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra1.8b << 13, r1 << 13  @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra1.8c << 6, r0 << 6    @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra1.8c << 14, r1 << 14  @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra1.8d << 7, r0 << 7    @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra1.8d << 15, r1 << 15  @ "mul_used", 0

  sub.setf -, r5, 8     ; mov r1,   ra8
  mov ra8,  ra9         ; mov rb8,  rb9
  brr.anyn -, r:yloop
  mov ra9,  ra10        ; mov rb9,  rb10
  mov ra10, ra11        ; mov rb10, rb11
  sub ra11, r2, r3      ; mov rb11, r1
  # >>> .anyn yloop

  # apply vertical filter and write to VPM

  nop                   ; mul24 r0, rb8,  ra2.8a
  nop                   ; mul24 r1, rb9,  ra2.8b
  sub r1, r1, r0        ; mul24 r0, rb10, ra2.8c
  sub r1, r1, r0        ; mul24 r0, rb11, ra2.8d
  add r1, r1, r0        ; mul24 r0, ra8,  rb4
  add r1, r1, r0        ; mul24 r0, ra9,  rb5
  sub r1, r1, r0        ; mul24 r0, ra10, rb6
  add r1, r1, r0        ; mul24 r0, ra11, rb7
  sub r1, r1, r0
# At this point r1 is a 22-bit signed quantity: 8 (original sample),
#  +6, +6 (each pass), +1 (the passes can overflow slightly), +1 (sign)
# The top 8 bits have rubbish in them as mul24 is unsigned
# The low 6 bits need discard before weighting
  sub.setf -, r5, rb_lcount ; mul24 r1, r1, ra_k256  # x256 - sign extend & discard rubbish
  asr r1, r1, 14
  nop                   ; mul24 r1, r1, ra_wt_mul_l0
  add r1, r1, rb_wt_off

  shl r1, r1, 8         ; mov r0, ra_height
  brr.anyn -, r:yloop
  asr ra3.8as, r1, rb_wt_den_p15
  mov r1, ra_k16        ; mov -, vw_wait
  sub r0, r0, r1        ; mov vpm, ra3.8a
# >>> branch.anyn yloop

# If looping again the we consumed 16 height last loop
  # rb_dma1 (stride) remains constant
  # rb_i_tmu remains const (based on total height)
  # recalc rb_dma0, rb_lcount based on new segment height
  # N.B. r3 is loop counter still

  max.setf -, r0, 0     ; mov ra_height, r0     # Done if Z now

# DMA out
  bra.anyz -, ra_link
  min r0, r0, r1        ; mov vw_setup, rb_dma0 # VDW setup 0
  sub r2, r0, r1        ; mov vw_setup, rb_dma1 # Stride
  nop                   ; mov vw_addr, rb_dest  # start the VDW
# >>> .anyz ra_link

  add rb_lcount, rb_lcount, r0
  shl r0, r2, i_shift23
  add rb_dma0, rb_dma0, r0
  brr -, r:yloop
  nop                   ; mul24 r0, r1, rb_pitch # r0 = pitch*16
  add rb_dest, rb_dest, r0
  mov vw_setup, rb_vpm_init                     # Reset our VDM write pointer
# >>> yloop


################################################################################

# mc_filter_b(y_x, base, y2_x2, base2, width_height, my2_mx2_my_mx, offsetweight0, this_dst, next_kernel)
# In a P block, only the first half of coefficients contain used information.
# At this point we have already issued two pairs of texture requests for the current block
# May be better to just send 16.16 motion vector and figure out the coefficients inside this block (only 4 cases so can compute hcoeffs in around 24 cycles?)
# Can fill in the coefficients so only
# Can also assume default weighted prediction for B frames.
# Perhaps can unpack coefficients in a more efficient manner by doing H/V for a and b at the same time?
# Or possibly by taking advantage of symmetry?
# From 19->7 32bits per command.

::mc_filter_b
  luma_setup

:yloopb
# retrieve texture results and pick out bytes
# then submit two more texture requests

# If we knew there was no clipping then this code would get simpler.
# Perhaps we could add on the pitch and clip using larger values?

  sub.setf -, r5, rb_i_tmu      ; v8adds r5rep, r5, ra_k1             ; ldtmu1
  shr r1, r4, rb_xshift2        ; mov.ifz ra_y_y2, ra_y_y2_next      ; ldtmu0
  shr r0, r4, ra_xshift         ; mov r3, rb_pitch

  max r2, ra_y, 0  # y
  min r2, r2, rb_max_y          ; mov.ifz ra_base, ra_base_next
  add ra_y, ra_y, 1             ; mul24 r2, r2, r3
  add t0s, ra_base, r2          ; mov.ifz ra_base2, rb_base2_next

  max r2, ra_y2, 0
  min r2, r2, rb_max_y
  add ra_y2, ra_y2, 1           ; mul24 r2, r2, r3
  add t1s, ra_base2, r2         ; v8min r0, r0, rb_pmask # v8subs masks out all but bottom byte

# generate seven shifted versions
# interleave with scroll of vertical context

  mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

# apply horizontal filter
  and r1, r1, rb_pmask   ; mul24      r3, ra0.8a,      r0
  nop                   ; mul24      r2, ra0.8b << 1, r0 << 1    @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8a << 8, r1 << 8    @ "mul_used", 0
  nop                   ; mul24.ifnz r2, ra0.8b << 9, r1 << 9    @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra0.8c << 2, r0 << 2    @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8c << 10, r1 << 10  @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra0.8d << 3, r0 << 3    @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8d << 11, r1 << 11  @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra1.8a << 4, r0 << 4    @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra1.8a << 12, r1 << 12  @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra1.8b << 5, r0 << 5    @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra1.8b << 13, r1 << 13  @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra1.8c << 6, r0 << 6    @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra1.8c << 14, r1 << 14  @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra1.8d << 7, r0 << 7    @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra1.8d << 15, r1 << 15  @ "mul_used", 0

  sub.setf -, r5, 8     ; mov r1,   ra8
  mov ra8,  ra9         ; mov rb8,  rb9
  brr.anyn -, r:yloopb
  mov ra9,  ra10        ; mov rb9,  rb10
  mov ra10, ra11        ; mov rb10, rb11
  sub ra11, r2, r3      ; mov rb11, r1
  # >>> .anyn yloopb

  # apply vertical filter and write to VPM
  nop                   ; mul24 r0, rb8,  ra2.8a
  nop                   ; mul24 r1, rb9,  ra2.8b
  sub r1, r1, r0        ; mul24 r0, rb10, ra2.8c
  sub r1, r1, r0        ; mul24 r0, rb11, ra2.8d
  add r1, r1, r0        ; mul24 r0, ra8,  rb4
  add r1, r1, r0        ; mul24 r0, ra9,  rb5
  sub r1, r1, r0        ; mul24 r0, ra10, rb6
  add r1, r1, r0        ; mul24 r0, ra11, rb7
  sub r1, r1, r0        ; mov r2, rb_wt_off
# As with P-pred r1 is a 22-bit signed quantity in 32-bits
# Top 8 bits are bad - low 6 bits should be discarded
  sub.setf -, r5, rb_lcount ; mul24 r1, r1, ra_k256

  asr r1, r1, 14
  nop                   ; mul24 r0, r1, ra_wt_mul_l0
  add r0, r0, r2        ; mul24 r1, r1 << 8, ra_wt_mul_l1 << 8    @ "mul_used", 0

  add r1, r1, r0
  shl r1, r1, 8         ; mov r0, ra_height
  brr.anyn -, r:yloopb
  asr ra3.8as, r1, rb_wt_den_p15
  mov r1, ra_k16        ; mov -, vw_wait
  sub r0, r0, r1        ; mov vpm, ra3.8a
# >>> branch.anyn yloop

# If looping again the we consumed 16 height last loop
  # rb_dma1 (stride) remains constant
  # rb_i_tmu remains const (based on total height)
  # recalc rb_dma0, rb_lcount based on new segment height
  # N.B. r5 is loop counter still

  max.setf -, r0, 0     ; mov ra_height, r0     # Done if Z now

# DMA out
  bra.anyz -, ra_link
  min r0, r0, r1        ; mov vw_setup, rb_dma0 # VDW setup 0
  sub r2, r0, r1        ; mov vw_setup, rb_dma1 # Stride
  nop                   ; mov vw_addr, rb_dest  # start the VDW
# >>> .anyz ra_link

  add rb_lcount, rb_lcount, r0
  shl r0, r2, i_shift23
  add rb_dma0, rb_dma0, r0
  brr -, r:yloopb
  nop                   ; mul24 r0, r1, rb_pitch # r0 = pitch*16
  add rb_dest, rb_dest, r0
  mov vw_setup, rb_vpm_init                     # Reset our VDM write pointer
# >>> yloopb

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

::mc_filter_y_p00
  mov ra0, unif         ; mov r3, elem_num      # y_x ; elem_num has implicit unpack??
  mov ra_xshift, ra_xshift_next                 # [ra0 delay]
  add r0, ra0.16b, r3

  max r0, r0, 0
  min r0, r0, rb_max_x

  shl ra_xshift_next, r0, 3                     # Compute shifts
  and r0, r0, -4        ; v8subs r2, r2, r2
  sub r2, r2, rb_pitch  ; mov ra_base_next, unif # src1.base
  and r1, r0, r2        ; mov ra_y_next, ra0.16a
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        ; mov ra_width_height, unif # Add stripe offsets ; width_height
  add ra_base_next, ra_base_next, r0 ; mov vw_setup, rb_vpm_init  # ; set up VPM write

# get width,height of block (unif load above)
  sub rb_dma1, rb_dma1_base, ra_width # Compute vdw_setup1(dst_pitch-width)
  sub rb_i_tmu, ra_height, PREREAD ; mov r0, ra_height
  min r0, r0, ra_k16
  add rb_lcount, r0, 0  ; mov ra_wt_off_mul_l0, unif
  shl r0,   r0, 7       ; mov rb_dest, unif     # Destination address
  add r0,   r0, ra_width                        # Combine width and height of destination area
  shl r0,   r0, i_shift16                       # Shift into bits 16 upwards of the vdw_setup0 register
  add rb_dma0, r0, rb_dma0_base

  shl r0, ra_wt_off_l0, rb_wt_den_p15 ; v8subs r5rep, r3, r3     # Offset calc ; r5 = 0
  # For B l1 & L0 offsets should be identical so it doesn't matter which we use
  asr rb_wt_off, r0, 1  ; mov ra_link, unif    # ; link

:yloop_p00
  sub.setf -, r5, rb_i_tmu  ; v8adds r5rep, r5, ra_k1
  nop                   ; mov.ifz ra_y, ra_y_next      ; ldtmu0
  shr r0, r4, ra_xshift ; mov r3, rb_pitch

  max r2, ra_y, 0  # y
  min r2, r2, rb_max_y  ; mov.ifz ra_base, ra_base_next
  add ra_y, ra_y, 1     ; mul24 r2, r2, r3
  add t0s, ra_base, r2  ; v8min r0, r0, rb_pmask

  sub.setf -, r5, rb_lcount ; mul24 r1, r0, ra_wt_mul_l0
  shl r1, r1, 15        ; mov r0, ra_height
  add r1, r1, rb_wt_off

  brr.anyn -, r:yloop_p00
  asr ra3.8as, r1, rb_wt_den_p15
  mov r1, ra_k16        ; mov -, vw_wait
  sub r0, r0, r1        ; mov vpm, ra3.8a
# >>> branch.anyn yloop_p00

# If looping again the we consumed 16 height last loop
  # rb_dma1 (stride) remains constant
  # rb_i_tmu remains const (based on total height)
  # recalc rb_dma0, rb_lcount based on new segment height
  # N.B. r5 is loop counter still

  max.setf -, r0, 0     ; mov ra_height, r0     # Done if Z now

# DMA out
  bra.anyz -, ra_link
  min r0, r0, r1        ; mov vw_setup, rb_dma0 # VDW setup 0
  sub r2, r0, r1        ; mov vw_setup, rb_dma1 # Stride
  nop                   ; mov vw_addr, rb_dest  # start the VDW
# >>> .anyz ra_link

  add rb_lcount, rb_lcount, r0
  shl r0, r2, i_shift23
  add rb_dma0, rb_dma0, r0
  brr -, r:yloop_p00
  nop                   ; mul24 r0, r1, rb_pitch # r0 = pitch*16
  add rb_dest, rb_dest, r0
  mov vw_setup, rb_vpm_init                     # Reset our VDM write pointer
# >>> yloop_p00

################################################################################

::mc_filter_y_b00
# luma setup does a fair bit more than we need calculating filter coeffs
# that we will never use but it saves I-cache to use it (also simple!)
  luma_setup

# Fix up vals that were expecting a filter (somewhat icky)
  mov r0, 7
  sub rb_i_tmu, rb_i_tmu, r0
  sub rb_lcount, rb_lcount, r0
  mov r0, 8             ; mov r1, ra_wt_off_mul_l0
  shl rb_wt_off, rb_wt_off, r0
  nop                   ; mov.ifnz ra_wt_off_mul_l0, r1 << 8

:yloop_b00
  sub.setf -, r5, rb_i_tmu ; v8adds r5rep, r5, ra_k1            ; ldtmu1
  shr r1, r4, rb_xshift2 ; mov.ifz ra_y_y2, ra_y_y2_next        ; ldtmu0
  shr r0, r4, ra_xshift ; mov r3, rb_pitch

  max r2, ra_y, 0  # y
  min r2, r2, rb_max_y  ; mov.ifz ra_base, ra_base_next
  add ra_y, ra_y, 1     ; mul24 r2, r2, r3
  add t0s, ra_base, r2  ; mov.ifz ra_base2, rb_base2_next

  max r2, ra_y2, 0
  min r2, r2, rb_max_y
  add ra_y2, ra_y2, 1   ; mul24 r2, r2, r3
  add t1s, ra_base2, r2 ; v8min r0, r0, rb_pmask # v8subs masks out all but bottom byte
  and r1, r1, rb_pmask   ; mul24 r0, r0, ra_wt_mul_l0

  sub.setf -, r5, rb_lcount ; mul24 r1, r1, ra_wt_mul_l1
  add r1, r0, r1
  shl r1, r1, 14
  add r1, r1, rb_wt_off ; mov r0, ra_height

  brr.anyn -, r:yloop_b00
  asr ra3.8as, r1, rb_wt_den_p15
  mov r1, ra_k16        ; mov -, vw_wait
  sub r0, r0, r1        ; mov vpm, ra3.8a
# >>> branch.anyn yloop

# If looping again the we consumed 16 height last loop
  # rb_dma1 (stride) remains constant
  # rb_i_tmu remains const (based on total height)
  # recalc rb_dma0, rb_lcount based on new segment height
  # N.B. r5 is loop counter still

  max.setf -, r0, 0     ; mov ra_height, r0     # Done if Z now

# DMA out
  bra.anyz -, ra_link
  min r0, r0, r1        ; mov vw_setup, rb_dma0 # VDW setup 0
  sub r2, r0, r1        ; mov vw_setup, rb_dma1 # Stride
  nop                   ; mov vw_addr, rb_dest  # start the VDW
# >>> .anyz ra_link

  add rb_lcount, rb_lcount, r0
  shl r0, r2, i_shift23
  add rb_dma0, rb_dma0, r0
  brr -, r:yloop_b00
  nop                   ; mul24 r0, r1, rb_pitch # r0 = pitch*16
  add rb_dest, rb_dest, r0
  mov vw_setup, rb_vpm_init                     # Reset our VDM write pointer
# >>> yloopb00

################################################################################
# 10 BIT

::mc_setup_c10_q0
::mc_setup_c10_qn
  m_setup_c 10

::mc_filter_c10_p
  m_filter_c_p 10

::mc_filter_c10_b
  m_filter_c_b 10

::mc_end
# Do not add code here because mc_end must appear after all other code.
