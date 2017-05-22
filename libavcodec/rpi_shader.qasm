
# The @ "mul_used", 0 annotations that occur by various mul blocks suppress
# the warning that we are using rotation & ra/rb registers. r0..3 can be
# rotated through all 16 elems ra regs can only be rotated through their
# local 4.  As it happens this is what is wanted here as we do not want the
# constants from the other half of the calc.

# register allocation
#

# ra0-3
# Used as temp and may be loop filter coeffs (split into .8s)
# or temp in loop. Check usage on an individual basis.

# ra4-7
# C-B: (with rb4-7) stash between L0 & L1 processing
# otherwise -- free --

# ra8-11
# temp in some places - check usage
# Y:   (with rb8-11) horiz out FIFO

# ra12-15
# C:   horiz out FIFO
# Y:  -- free --

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

# -- free --                       ra22
# -- free --                       ra23

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

# chroma-B height+3; free otherwise
.set ra_cb_lheight_p3,             ra31

.set rb_xshift2,                   rb0
.set rb_xshift2_next,              rb1

# C:  (elem & 1) == 0 ? elem * 2 : (elem + 4) * 2
.set rb_elem_x,                    rb2

# -- free --                       rb3

# rb4-7
# C-B: (with ra4-7) stash between L0 & L1 processing
# Y:   (with ra2.8x) Y vertical filter coeffs

# rb8-11
# C:   Vertical filter coeffs
# Y:   (with ra8-11) horiz out FIFO

# Loop var: offset to add before shift (round + weighting offsets)
# Exact value varies by loop
.set rb_wt_off,                    rb12

# Setup: denom + 6 + 9
.set rb_wt_den_p15,                rb13

# Loop var: L0 weight (U on left, V on right)
.set rb_wt_mul_l0,                 rb14

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

# Setup: 255
.set rb_k255,                      rb22

# Loop: destination address
.set rb_dest,                      rb23

# vdw_setup_1(dst_pitch)
.set rb_dma1_base,                 rb24

# Setup: pic width - 1
# In the case of chroma it is in bytes so 2 * (pic_width_c - 1)
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

# Loop counter - counts up from 0, sometimes in r3
.set rb_i,                         rb31




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

# For chroma use packed H = (qpu_num & 1), Y = (qpu_num >> 1) * 16
.macro m_calc_dma_regs_c, r_vpm, r_dma
  mov r2, qpu_num
  asr r1, r2, 1
  shl r1, r1, 5
  and r0, r2, 1
  or  r0, r0, r1

  mov r1, vpm_setup(0, 2, h16p(0, 0))   # 2 is stride - stride acts on ADDR which is Y[5:0],B[1:0] for 8 bit
  add r_vpm, r0, r1  # VPM 16bit storage

  # X = H * 8 so the YH from VPMVCD_WR_SETUP[ADDR] drops into
  # XY VPMVCD_WR_SETUP[VPMBASE] if shifted left 3 (+ 3 for pos of field in reg)
  mov r1, vdw_setup_0(0, 0, dma_h16p(0,0,0)) # height,width added later
  shl r0, r0, 6
  add r_dma, r0, r1  # DMA out
.endm


################################################################################
# mc_setup_uv(next_kernel, x, y, ref_c_base, frame_width, frame_height, pitch, dst_pitch, offset, denom, vpm_id)
::mc_setup_c
  mov tmurs, 1                                  # No swap TMUs

# Load first request location
  mov ra0, unif         # next_x_y

  mov ra_base, unif                             # Store frame c base

# Read image dimensions
  sub r0, unif, 1                               # pic c width
  add rb_max_x, r0, r0
  sub rb_max_y, unif, 1     # pic c height

# load constants
  mov ra_kff100100, 0xff100100
  mov rb_k255, 255

# touch registers to keep simulator happy

  # ra/b4..7: B0 -> B stash registers
  mov ra4, 0 ; mov rb4, 0
  mov ra5, 0 ; mov rb5, 0
  mov ra6, 0 ; mov rb6, 0
  mov ra7, 0 ; mov rb7, 0

  mov r1, vdw_setup_1(0)  # Merged with dst_stride shortly, delay slot for ra_base

# ; ra12..15: vertical scroll registers
# get source pitch
  mov rb_xpitch, unif   ; mov ra12, 0           # stride2
  mov rb_pitch, unif    ; mov ra13, 0           # stride1
  nop                   ; mov ra14, 0
# get destination vdw setup
  add rb_dma1_base, r1, rb_pitch ; mov ra15, ra_k0 # vdw_setup_1

  and r0, 1, elem_num
  nop                   ; mul24 r0, r0, 7
  add rb_elem_x, r0, elem_num

# Compute base address for first and second access
# ra_base ends up with t0s base
# ra_base2 ends up with t1s base

  add r0, ra0.16b, ra0.16b                      # [rb_elem_x delay]
  add r0, r0, rb_elem_x                         # Add elem no to x to get X for this slice
  max r0, r0, 0         ; mov ra_y, ra0.16a     # ; stash Y
  min r0, r0, rb_max_x

# Get shift
  shl ra_xshift_next, r0, 3

# In a single 32 bit word we get 2 UV pairs so mask bottom bit of xs

  and r0, r0, -4
  nop                   ; v8subs r1, r1, r1
  sub r1, r1, rb_pitch
  and r1, r0, r1
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        ; mov r1, ra_y
  add ra_base, ra_base, r0

  max r0, r1, 0
  min r0, r0, rb_max_y

# submit texture requests for first line
  add r1, r1, ra_k1     ; mul24 r0, r0, rb_pitch
  add t0s, ra_base, r0

# submit texture requests for 2nd line

  max r0, r1, 0
  min r0, r0, rb_max_y

  add ra_y, r1, ra_k1   ; mul24 r0, r0, rb_pitch
  add t0s, ra_base, r0

  add rb_wt_den_p15, 9, unif     # denominator
  mov -, unif           # Unused

# Compute part of VPM to use for DMA output
  m_calc_dma_regs rb_vpm_init, rb_dma0_base

  mov -, unif                                   # Next fn (ignored)

# -----------------
# And again for L1, but only worrying about frame2 stuff


# Load first request location
  mov ra0, unif            # next_x_y

  mov ra_base2, unif # Store frame c base

# Compute base address for first and second access
# ra_base ends up with t0s base
# ra_base2 ends up with t1s base

  mov ra_y2, ra0.16a                            # Store y
  add r0, ra0.16b, ra0.16b                      # Load x
  add r0, r0, rb_elem_x                         # Add QPU slice offset
  max r0, r0, 0         ; mov -, unif           # Unused 0
  min r0, r0, rb_max_x  ; mov -, unif           # Unused 1

# Get shift
  nop                   ; mov -, unif           # Unused 2
  shl rb_xshift2_next, r0, 3

# In a single 32 bit word we get 2 UV pairs so mask bottom bit of xs

  and r0, r0, -4        ; mov -, unif           # Unused 3
  nop                   ; v8subs r1, r1, r1
  sub r1, r1, rb_pitch  ; mov -, unif           # Unused 4
  and r1, r0, r1
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        ; mov r1, ra_y2
  add ra_base2, ra_base2, r0 ; mov - , unif     # Unused 5

  max r0, r1, 0
  min r0, r0, rb_max_y

# submit texture requests for first line
  add r1, r1, ra_k1     ; mul24 r0, r0, rb_pitch
  add t1s, ra_base2, r0 ; mov ra_link, unif     # Link

# submit texture requests for 2nd line

  max r0, r1, 0                                 # [ra_link delay]

  bra -, ra_link

  min r0, r0, rb_max_y
  add ra_y2, r1, ra_k1  ; mul24 r0, r0, rb_pitch
  add t1s, ra_base2, r0

# >>> ra_link


.macro setf_nz_if_v
  mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
.endm


################################################################################

# mc_filter_uv(next_kernel, x, y, frame_c_base, width_height, hcoeffs, vcoeffs, offset_weight_u, offset_weight_v, this_u_dst, this_v_dst)

# At this point we have already issued two pairs of texture requests for the current block
# ra_x, ra_x16_base point to the current coordinates for this block
::mc_filter_uv
  mov vw_setup, rb_vpm_init

# per-channel shifts were calculated on the *previous* invocation

# get base addresses and per-channel shifts for *next* invocation
  mov ra2, unif                                 # x_y

  setf_nz_if_v                                  # Also acts as delay slot for ra2

  add r0, ra2.16b, ra2.16b ; v8subs r1, r1, r1     # x ; r1=0
  add r0, r0, rb_elem_x
  sub r1, r1, rb_pitch  ; mov r3, unif          # r1=pitch2 mask ; r3=base
  max r0, r0, 0         ; mov rb_xshift2, ra_xshift_next # ; xshift2 used because B
  min r0, r0, rb_max_x  ; mov ra1, unif         # ; width_height

  shl ra_xshift_next, r0, 3

  and r0, r0, -4        ; mov ra0, unif         # H filter coeffs
  nop                   ; mov ra_y_next, ra2.16a
  and r1, r0, r1        ; mul24 r2, ra1.16b, 2  # r2=w*2 (we are working in pel pairs)  ** x*2 already calced!
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        ; mov r1, ra1.16a       # Add stripe offsets ; r1=height
  add ra_base_next, r3, r0
  shl r0, r1, 7

# set up VPM write

  sub rb_dma1, rb_dma1_base, r2    ; mov ra3, unif         # Compute vdw_setup1(dst_pitch-width) ; V filter coeffs
  add rb_i_tmu, r1, 1   ; mov ra1, unif         # ; U offset/weight
  add rb_lcount, r1, 3  ; mov.ifnz ra1, unif    # ; V offset/weight

# ; unpack filter coefficients

  add r0, r0, r2      ; mov rb8,  ra3.8a      # Combine width and height of destination area (r0=h<<8, r2=w*2)
  shl r0, r0, i_shift16 ; mov rb9,  ra3.8b      # Shift into bits 16 upwards of the vdw_setup0 register
  add rb_dma0, r0, rb_dma0_base ; mov r1, ra1.16b       # ; r1=weight

  mov rb_dest, unif                             # dst_addr

  shl r1, r1, rb_wt_den_p15 ; mov rb10, ra3.8c
  mov r3, 0             ; mov rb11, ra3.8d      # Loop count

  asr rb_wt_off, r1, 1  ; mov ra_link, unif     # Link
  shl rb_wt_mul_l0, ra1.16a, 1                  # b14 = weight*2

# rb_wt_mul_l0 - weight L0 * 2
# rb_wt_den_p15 = weight denom + 6 + 9
# rb_wt_off = (((is P) ? offset L0 * 2 : offset L1 + offset L0) + 1) << (rb_wt_den_p15 - 1)

# retrieve texture results and pick out bytes
# then submit two more texture requests


# we want (r4a r4b)
# C0C1 : C4Ca : C0C1 : C4Ca : C2C3 : CaCb : C2C3...
# goes to (r0r1)
# U0U4 : V0V4 : U1U5 : V1V5 : U2U6 : V2U6 : ...


# r3 = 0
:uvloop
# retrieve texture results and pick out bytes
# then submit two more texture requests

  sub.setf -, r3, rb_i_tmu ; v8adds rb_i, r3, ra_k1 ; ldtmu0     # loop counter increment
  shr r2, r4, rb_xshift2 ; mov.ifz r3, ra_y_next
  shr r1, r2, 8         ; mov.ifnz r3, ra_y
  mov.ifz ra_base, ra_base_next

  and.setf -, 1, elem_num ; mov r0, r2
  nop                   ; mov.ifnz r0, r1 << 15
  nop                   ; mov.ifz  r1, r2 << 1

  max r2, r3, 0
  min r2, r2, rb_max_y
  add ra_y, r3, ra_k1   ; mul24 r2, r2, rb_pitch
  add t0s, ra_base, r2  ; v8min r0, r0, rb_k255  # v8subs masks out all but bottom byte

# generate seven shifted versions
# interleave with scroll of vertical context

  setf_nz_if_v

# apply horizontal filter
# The filter coeffs for the two halves of this are the same (unlike in the
# Y case) so it doesn't matter which ra0 we get them from

  and r1, r1, rb_k255   ; mul24      r3, ra0.8a,       r0
  nop                   ; mul24      r2, ra0.8b << 2,  r0 << 2  @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8a << 8,  r1 << 8  @ "mul_used", 0
  nop                   ; mul24.ifnz r2, ra0.8b << 10, r1 << 10 @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra0.8c << 4,  r0 << 4  @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8c << 12, r1 << 12 @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra0.8d << 6,  r0 << 6  @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8d << 14, r1 << 14 @ "mul_used", 0
  sub r0, r2, r3        ; mov r3, rb_i
  sub.setf -, r3, 4     ; mov ra12, ra13
  brr.anyn -, r:uvloop
  mov ra13, ra14        ; mul24 r1, ra14, rb9
  mov ra14, ra15
  mov ra15, r0          ; mul24 r0, ra12, rb8
# >>> .anyn uvloop

# apply vertical filter and write to VPM

  sub r1, r1, r0        ; mul24 r0, ra14, rb10
  add r1, r1, r0        ; mul24 r0, ra15, rb11
  sub r1, r1, r0
  sub.setf -, r3, rb_lcount ; mul24 r1, r1, ra_k256
  asr r1, r1, 14
  nop                   ; mul24 r1, r1, rb_wt_mul_l0
  shl r1, r1, 8

  add r1, r1, rb_wt_off
  brr.anyn -, r:uvloop
  asr ra1.8as, r1, rb_wt_den_p15
  mov -, vw_wait
  mov vpm, ra1.8a

# >>>

# DMA out for U & stash for V
  bra -, ra_link
  mov vw_setup, rb_dma0
  mov vw_setup, rb_dma1
  mov vw_addr, rb_dest     # u_dst_addr
# >>>

# we want
# C0C1 : C4Ca : C0C1 : C4Ca : C2C3 : CaCb : C2C3...
# U0U4 : V0V4 : U1U5 : V1V5 : U2U6 : V2U6 : ...

# mov.setf -, [0,1,0,...]
# shr r2, r4, ra_xshift  # if elem & 2 == 1 then r0 = r0 >> 16
# shr r1, r2, 8
# mov r0, r2
# mov.ifnz r0, r1 << 1
# mov.ifz  r1, r2 << 15
#
#
#


################################################################################

# mc_filter_uv_b0(next_kernel, x, y, frame_c_base, height, hcoeffs[0], hcoeffs[1], vcoeffs[0], vcoeffs[1], this_u_dst, this_v_dst)

# At this point we have already issued two pairs of texture requests for the current block
# ra_x, ra_x16_base point to the current coordinates for this block
::mc_filter_uv_b0
  mov vw_setup, rb_vpm_init

# per-channel shifts were calculated on the *previous* invocation

# get base addresses and per-channel shifts for *next* invocation
  mov ra2, unif                                 # x_y

  setf_nz_if_v                                  # Also acts as delay slot for ra2

  add r0, ra2.16b, ra2.16b ; v8subs r1, r1, r1     # x ; r1=0
  add r0, r0, rb_elem_x
  sub r1, r1, rb_pitch  ; mov r3, unif          # r1=pitch2 mask ; r3=base
  max r0, r0, 0         ; mov rb_xshift2, ra_xshift_next # ; xshift2 used because B
  min r0, r0, rb_max_x  ; mov ra1, unif         # ; width_height

  shl ra_xshift_next, r0, 3

  and r0, r0, -4        ; mov ra0, unif         # H filter coeffs
  nop                   ; mov ra_y_next, ra2.16a
  and r1, r0, r1        ; mul24 r2, ra1.16b, 2  # r2=x*2 (we are working in pel pairs)
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        ; mov r1, ra1.16a       # Add stripe offsets ; r1=height
  add ra_base_next, r3, r0
  shl r0, r1, 7

# set up VPM write

  sub rb_dma1, rb_dma1_base, r2 ; mov ra3, unif         # Compute vdw_setup1(dst_pitch-width) ; V filter coeffs
  add rb_i_tmu, r1, 1
  add ra_cb_lheight_p3, r1, 3   ; mov rb8,  ra3.8a      # Combine width and height of destination area

# ; unpack filter coefficients

  add r0,   r0, r2      ; mov rb9,  ra3.8b
  shl r0,   r0, i_shift16 ; mov rb10, ra3.8c      # Shift into bits 16 upwards of the vdw_setup0 register
  add rb_dma0, r0, rb_dma0_base

  mov r3, 0             ; mov rb11, ra3.8d      # Loop count

  mov rb_wt_mul_l0, unif                                # U weight
  mov.ifnz rb_wt_mul_l0, unif                           # V weight

# rb_wt_mul_l0 unused in b0 but will hang around till the second pass

# retrieve texture results and pick out bytes
# then submit two more texture requests

# r3 = 0
:uvloop_b0
# retrieve texture results and pick out bytes
# then submit two more texture requests

  sub.setf -, r3, rb_i_tmu ; v8adds rb_i, r3, ra_k1 ; ldtmu0     # loop counter increment
  shr r2, r4, rb_xshift2 ; mov.ifz r3, ra_y_next
  shr r1, r2, 8         ; mov.ifnz r3, ra_y
  mov.ifz ra_base, ra_base_next

  and.setf -, 1, elem_num ; mov r0, r2
  nop                   ; mov.ifnz r0, r1 << 15
  nop                   ; mov.ifz  r1, r2 << 1

  max r2, r3, 0
  min r2, r2, rb_max_y
  add ra_y, r3, ra_k1   ; mul24 r2, r2, rb_pitch
  add t0s, ra_base, r2  ; v8min r0, r0, rb_k255  # v8subs masks out all but bottom byte

# generate seven shifted versions
# interleave with scroll of vertical context

  mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

  and r1, r1, rb_k255   ; mul24      r3, ra0.8a,       r0
  nop                   ; mul24      r2, ra0.8b << 2,  r0 << 2  @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8a << 8,  r1 << 8  @ "mul_used", 0
  nop                   ; mul24.ifnz r2, ra0.8b << 10, r1 << 10 @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra0.8c << 4,  r0 << 4  @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8c << 12, r1 << 12 @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra0.8d << 6,  r0 << 6  @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8d << 14, r1 << 14 @ "mul_used", 0
  sub r0, r2, r3        ; mov r3, rb_i
  sub.setf -, r3, 4     ; mov ra12, ra13
  brr.anyn -, r:uvloop_b0
  mov ra13, ra14        ; mul24 r1, ra14, rb9   # ra14 is about to be ra13
  mov ra14, ra15        ; mul24 r2, ra15, rb10  # ra15 is about to be ra14
  mov ra15, r0          ; mul24 r0, ra12, rb8
# >>> .anyn uvloop_b0

# apply vertical filter and write to B-FIFO

  sub r1, r1, r0        ; mov ra8.16b, ra7      # start of B FIFO writes
  add r1, r1, r2        ; mul24 r0, ra15, rb11  # N.B. ra15 write gap
  sub r1, r1, r0        ; mov ra7, rb6

# FIFO goes:
# b7a, a6a, b5a, a4a, b4a, a5a, b6a, a7a : b7b, a6b, b5b, a4b, b4b, a5b, b6b, a7b
# This arrangement optimizes the inner loop FIFOs at the expense of making the
# bulk shift between loops quite a bit nastier
# a8 used as temp

  sub.setf -, r3, ra_cb_lheight_p3
  asr ra8.16a, r1, 6    ; mov rb6, ra5          # This discards the high bits that might be bad
  brr.anyn -, r:uvloop_b0
  mov ra5, rb4          ; mov rb4, ra4
  mov ra4, rb5          ; mov rb5, ra6
  mov ra6, rb7          ; mov rb7, ra8
# >>>

# 1st half done all results now in the a/b4..7 fifo

# Need to bulk rotate FIFO for heights other than 16
# plausible heights are 16, 12, 8, 6, 4, 2 and that is all we deal with
# we are allowed 3/4 cb_size w/h :-(

# Destination uniforms discarded
# At the end drop through to _b - we will always do b after b0

  sub.setf -, 15, r3    # 12 + 3 of preroll
  brr.anyn -, r:uv_b0_post_fin                  # h > 12 (n) => 16 (do nothing)
  sub r3, 11, r3                ; mov -, unif   # r3 = shifts wanted ; Discard u_dst_addr
  mov r0, i_shift16             ; mov -, unif   # Link from b0 - ignored
  mov r1, 0x10000
# >>>
  brr.anyz -, r:uv_b0_post12                    # h == 12 deal with specially
# If h != 16 && h != 12 then h <= 8 so
# shift 8 with discard (.16b = .16a on all regs)
  shl.ifnz ra7, ra7, r0 ; mul24.ifnz rb7, rb7, r1
  shl.ifnz ra6, ra6, r0 ; mul24.ifnz rb6, rb6, r1
  shl.ifnz ra5, ra5, r0 ; mul24.ifnz rb5, rb5, r1
# >>>
  shl ra4, ra4, r0      ; mul24 rb4, rb4, r1

  shl.setf -, r3, i_shift30  # b2 -> C, b1 -> N
# Shift 4
  mov.ifc ra7, ra4      ; mov.ifc rb6, rb5
  mov.ifc ra5, ra6      ; mov.ifc rb4, rb7
  # If we shifted by 4 here then the max length remaining is 4
  # so that is it

  brr -, r:uv_b0_post_fin
# Shift 2
  mov.ifn ra7, ra5      ; mov.ifn rb6, rb4
  mov.ifn ra5, ra4      ; mov.ifn rb4, rb5
  mov.ifn ra4, ra6      ; mov.ifn rb5, rb7
  # 6 / 2 so need 6 outputs
# >>>

:uv_b0_post12
# this one is annoying as we need to swap halves of things that don't
# really want to be swapped

# b7a, a6a, b5a, a4a
# b4a, a5a, b6a, a7a
# b7b, a6b, b5b, a4b
# b4b, a5b, b6b, a7b

  mov r2, ra6           ; mov r3, rb7
  shl ra6, ra5, r0      ; mul24 rb7, rb4, r1
  mov ra5, r2           ; mov rb4, r3

  mov r2,  ra4          ; mov r3,  rb5
  shl ra4, ra7, r0      ; mul24 rb5, rb6, r1
  mov ra7, r2           ; mov rb6, r3

:uv_b0_post_fin

##### L1 B processing

# per-channel shifts were calculated on the *previous* invocation

# get base addresses and per-channel shifts for *next* invocation
  mov ra2, unif                                 # x_y

  setf_nz_if_v                                  # Also acts as delay slot for ra2

  add r0, ra2.16b, ra2.16b ; v8subs r1, r1, r1  # r0=x*2 ; r1=0
  add r0, r0, rb_elem_x
  sub r1, r1, rb_pitch  ; mov r3, unif          # r1=pitch2 mask ; r3=base
  max r0, r0, ra_k0     ; mov rb_xshift2, rb_xshift2_next # ; xshift2 used because B
  min r0, r0, rb_max_x  ; mov -, unif           # ; width_height

  shl rb_xshift2_next, r0, 3

  and r0, r0, -4        ; mov ra0, unif         # H filter coeffs
  nop                   ; mov ra_y2_next, ra2.16a
  and r1, r0, r1        ; mov ra3, unif         # ; V filter coeffs
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        ; mov rb8,  ra3.8a      # Add stripe offsets ; start unpacking filter coeffs
  add rb_base2_next, r3, r0

  mov ra1, unif         ; mov rb9,  ra3.8b      # U offset/weight
  mov.ifnz ra1, unif    ; mov rb10, ra3.8c      # V offset/weight

  mov rb_dest, unif                             # dst_addr
  nop                   ; mov rb11, ra3.8d
  shl r1, ra1.16b, rb_wt_den_p15 ; v8subs r3, r3, r3     # ; r3 (loop counter)  = 0
  asr rb_wt_off, r1, 1  ; mov ra_link, unif     # link


# r3 = 0

# ra0.8x  filter components
# ra1.16a (L1 mul weight) used directly in the loop
# ra3.8a  used for saturation (temp)

:uvloop_b
# retrieve texture results and pick out bytes
# then submit two more texture requests

  sub.setf -, r3, rb_i_tmu      ; v8adds rb_i, r3, ra_k1 ; ldtmu1     # loop counter increment
  shr r2, r4, rb_xshift2        ; mov.ifz r3, ra_y2_next
  shr r1, r2, 8                 ; mov.ifnz r3, ra_y2
  mov.ifz ra_base2, rb_base2_next

  and.setf -, 1, elem_num ; mov r0, r2
  nop                   ; mov.ifnz r0, r1 << 15
  nop                   ; mov.ifz  r1, r2 << 1

  max r2, r3, ra_k0
  min r2, r2, rb_max_y
  add ra_y2, r3, ra_k1          ; mul24 r2, r2, rb_pitch
  add t1s, ra_base2, r2         ; v8min r0, r0, rb_k255  # v8subs masks out all but bottom byte

# generate seven shifted versions
# interleave with scroll of vertical context

  mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

  and r1, r1, rb_k255   ; mul24      r3, ra0.8a,       r0
  nop                   ; mul24      r2, ra0.8b << 2,  r0 << 2  @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8a << 8,  r1 << 8  @ "mul_used", 0
  nop                   ; mul24.ifnz r2, ra0.8b << 10, r1 << 10 @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra0.8c << 4,  r0 << 4  @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8c << 12, r1 << 12 @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra0.8d << 6,  r0 << 6  @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8d << 14, r1 << 14 @ "mul_used", 0
  sub r0, r2, r3        ; mov r3, rb_i
  sub.setf -, r3, 4     ; mov ra12, ra13
  brr.anyn -, r:uvloop_b
  mov ra13, ra14        ; mul24 r1, ra14, rb9
  mov ra14, ra15        ; mul24 r2, ra15, rb10
  mov ra15, r0          ; mul24 r0, ra12, rb8
# >>> .anyn uvloop_b

# apply vertical filter and write to VPM

  sub r1, r1, r0        ; mov ra8.16b, ra7      # FIFO rotate (all ra/b4..7)
  add r1, r1, r2        ; mul24 r0, ra15, rb11
  sub r1, r1, r0        ; mul24 r0, ra7.16b, rb_wt_mul_l0
  mov ra7, rb6          ; mul24 r1, r1, ra_k256
  asr r1, r1, 14        ; mov rb6, ra5          # shift2=6

  mov ra5, rb4          ; mul24 r1, r1, ra1.16a
  add r1, r1, r0        ; mov rb4, ra4

  mov ra4, rb5          ; mul24 r1, r1, ra_k256 # Lose bad top 8 bits & sign extend
  add r1, r1, rb_wt_off ; mov rb5, ra6          # rb_wt_off = (offsetL0 + offsetL1 + 1) << (rb_wt_den_p15 - 1)

  sub.setf -, r3, ra_cb_lheight_p3 ; mov ra6, rb7
  brr.anyn -, r:uvloop_b
  asr ra3.8as, r1, rb_wt_den_p15
  mov -, vw_wait        ; mov rb7, ra8          #  vw_wait is B-reg (annoyingly) ; Final FIFO mov
  mov vpm, ra3.8a
# >>>

# DMA out

  bra -, ra_link
  mov vw_setup, rb_dma0
  mov vw_setup, rb_dma1
  mov vw_addr, rb_dest                          # c_dst_addr


################################################################################
# Exit code used by both Luma & Chroma so place between them to avoid I-cache
# conflicts

# mc_exit()

::mc_interrupt_exit8c
  nop                   ; nop           ; ldtmu0
  nop                   ; nop           ; ldtmu1
  nop                   ; nop           ; ldtmu0
  mov  -, vw_wait       ; nop           ; ldtmu1 # wait on the VDW

  mov -,sacq(0) # 1
  mov -,sacq(0) # 2
  mov -,sacq(0) # 3
  mov -,sacq(0) # 4
  mov -,sacq(0) # 5
  mov -,sacq(0) # 6
  mov -,sacq(0) # 7
#  mov -,sacq(0) # 8
#  mov -,sacq(0) # 9
#  mov -,sacq(0) # 10
#  mov -,sacq(0) # 11

  nop                   ; nop           ; thrend
  mov interrupt, 1
  nop
# >>> thrend <<<

# Chroma & Luma the same now
::mc_exit_c
::mc_exit
  nop                   ; nop           ; ldtmu0
  nop                   ; nop           ; ldtmu1
  nop                   ; nop           ; ldtmu0
  mov  -, vw_wait       ; nop           ; ldtmu1 # wait on the VDW

  mov -,srel(0)

  nop                   ; nop           ; thrend
  nop
  nop
# >>> thrend <<<

# mc_interrupt_exit12()
::mc_interrupt_exit12
  nop                   ; nop           ; ldtmu0
  nop                   ; nop           ; ldtmu1
  nop                   ; nop           ; ldtmu0
  mov  -, vw_wait       ; nop           ; ldtmu1  # wait on the VDW

  mov -,sacq(0) # 1
  mov -,sacq(0) # 2
  mov -,sacq(0) # 3
  mov -,sacq(0) # 4
  mov -,sacq(0) # 5
  mov -,sacq(0) # 6
  mov -,sacq(0) # 7
  mov -,sacq(0) # 8
  mov -,sacq(0) # 9
  mov -,sacq(0) # 10
  mov -,sacq(0) # 11

  nop                   ; nop           ; thrend
  mov interrupt, 1
  nop
# >>> thrend <<<

# LUMA CODE

# The idea is to form B predictions by doing 8 pixels from ref0 in parallel with 8 pixels from ref1.
# For P frames we make the second x,y coordinates offset by +8

################################################################################
# mc_setup(y_x, ref_y_base, y2_x2, ref_y2_base, frame_width_height, pitch, dst_pitch, offset_shift, tbd, next_kernel)
::mc_setup
  # Need to save these because we need to know the frame dimensions before computing texture coordinates
  mov tmurs, 1          ; mov ra8, unif         # No TMU swap ; y_x
  mov ra9, unif         # ref_y_base
  mov ra10, unif        # y2_x2
  mov ra11, unif        # ref_y2_base

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
  add r0, ra8.16a, r3   # Load x + elem_num
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

  mov r1, ra8.16b       # Load y
  add ra_y, r1, 1       # Set for next
  max r1, r1, 0
  min r1, r1, rb_max_y

# submit texture requests for first line
  nop                   ; mul24 r1, r1, rb_pitch
  add t0s, ra_base, r1


  # r3 still contains elem_num
  add r0, ra10.16a, r3  # Load x
  max r0, r0, 0
  min r0, r0, rb_max_x
  shl rb_xshift2_next, r0, 3 # Compute shifts

  # r2 still contains mask
  and r0, r0, -4
  and r1, r0, r2
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        # Add stripe offsets
  add ra_base2, ra11, r0

  mov r1, ra10.16b       # Load y
  add ra_y2, r1, 1       # Set for next
  max r1, r1, 0
  min r1, r1, rb_max_y

# submit texture requests for first line
  nop                   ; mul24 r1, r1, rb_pitch
  add t1s, ra_base2, r1

# load constants

  mov ra_kff100100, 0xff100100
  mov rb_k255, 255

# touch vertical context to keep simulator happy

  mov ra8,  0           ; mov rb8,  0
  mov ra9,  0           ; mov rb9,  0
  mov ra10, 0           ; mov rb10, 0
  mov ra11, 0           ; mov rb11, 0

# Compute part of VPM to use
  m_calc_dma_regs rb_vpm_init, rb_dma0_base

# Weighted prediction denom
  add rb_wt_den_p15, unif, 9     # unif = weight denom + 6

# submit texture requests for second line
  max r1, ra_y, 0
  min r1, r1, rb_max_y
  add ra_y, ra_y, 1
  mov -, unif           ; mul24 r1, r1, rb_pitch  # unused ;
  add t0s, r1, ra_base  ; mov ra_link, unif     # ; Next fn

  max r1, ra_y2, 0
  min r1, r1, rb_max_y
  bra -, ra_link
  add ra_y2, ra_y2, 1
  nop                   ; mul24 r1, r1, rb_pitch
  add t1s, r1, ra_base2
# >>>

# 1st 3 instructions of per_block-setup in branch delay
.macro luma_setup
  brr ra_link, r:per_block_setup
  mov ra1, unif         ; mov r3, elem_num  # y_x ; elem_num has implicit unpack??
  mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1] # [ra1 delay]
  add r0, ra1.16a, r3   ; mov rb_xshift2, rb_xshift2_next
.endm

################################################################################
#
# Start of per-block setup code
# P and B blocks share the same setup code to save on Icache space
:per_block_setup

# luma_setup_delay3 done in delay slots of branch that got us here

# get base addresses and per-channel shifts for *next* invocation
# per-channel shifts were calculated on the *previous* invocation

  max r0, r0, 0         ; mov ra_xshift, ra_xshift_next
  min r0, r0, rb_max_x

  shl ra_xshift_next, r0, 3         # Compute shifts
  and r0, r0, -4        ; v8subs r2, r2, r2
  sub r2, r2, rb_pitch
  and r1, r0, r2
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        # Add stripe offsets
  add ra_base_next, unif, r0              # Base1
  mov ra_y_next, ra1.16b                      # Load y
  mov ra1, unif         # x2_y2
  nop                   # ra1 delay

  add r0, ra1.16a, r3   # Load x2
  max r0, r0, 0
  min r0, r0, rb_max_x

  shl rb_xshift2_next, r0, 3         # Compute shifts
  and r0, r0, -4
  and r1, r0, r2
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        # Add stripe offsets
  add rb_base2_next, unif, r0              # Base1
  mov ra_y2_next, ra1.16b                      # Load y
  mov ra_width_height, unif         # width_height

# set up VPM write
  mov vw_setup, rb_vpm_init    # [ra1 delay]

# get width,height of block (unif load above)
  sub rb_dma1, rb_dma1_base, ra_width # Compute vdw_setup1(dst_pitch-width)
  add rb_i_tmu, ra_height, 5  ; mov r0, ra_height
  mov r1, 16
  min r0, r0, r1
  add rb_lcount, r0, 7
  shl r0,   r0, 7
  add r0,   r0, ra_width                        # Combine width and height of destination area
  shl r0,   r0, i_shift16                       # Shift into bits 16 upwards of the vdw_setup0 register
  add rb_dma0, r0, rb_dma0_base                 ; mov r0, unif   # Packed filter offsets

# get filter coefficients and discard unused B frame values
  shl.ifz r0, r0, i_shift16          ; mov ra5, unif    #  Pick half to use ; L0 offset/weight
  mov r2, 0x01040400                 # [ra5 delay]
  shl ra8, r0, 3                     ; mov rb_wt_mul_l0, ra5.16a

# Pack the 1st 4 filter coefs for H & V tightly

  mov r1,0x00010100  # -ve
  ror ra2.8a, r1, ra8.8d
  ror ra0.8a, r1, ra8.8c

  ror ra2.8b, r2, ra8.8d
  ror ra0.8b, r2, ra8.8c

  mov r1,0x050b0a00  # -ve
  ror ra2.8c, r1, ra8.8d
  ror ra0.8c, r1, ra8.8c

  mov r1,0x11283a40
  ror ra2.8d, r1, ra8.8d
  ror ra0.8d, r1, ra8.8c

# In the 2nd vertical half we use b registers due to
# using a-side fifo regs. The easiest way to achieve this to pack it
# and then unpack!

  mov r1,0x3a281100
  ror ra3.8a, r1, ra8.8d
  ror ra1.8a, r1, ra8.8c

  mov r1,0x0a0b0500  # -ve
  ror ra3.8b, r1, ra8.8d
  ror ra1.8b, r1, ra8.8c

  mov r1,0x04040100
  ror ra3.8c, r1, ra8.8d
  ror ra1.8c, r1, ra8.8c

  mov r1,0x01010000  # -ve
  ror ra3.8d, r1, ra8.8d
  ror ra1.8d, r1, ra8.8c

# Extract weighted prediction information in parallel
# We are annoyingly A src limited here

  mov rb4, ra3.8a               ; mov ra_wt_off_mul_l1, unif
  mov rb5, ra3.8b
  mov rb6, ra3.8c
  mov.ifnz ra5, ra_wt_off_mul_l1

  mov rb_dest, unif             # Destination address

  bra -, ra_link

  shl r0, ra5.16b, rb_wt_den_p15      # Offset calc
  asr rb_wt_off, r0, 9          ; mov ra_link, unif # For B l1 & L0 offsets should be identical so it doesn't matter which we use
  mov r3, 0                     ; mov rb7, ra3.8d
# >>> branch ra_link
#
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

# ra5.16a = weight << 16; We want weight * 2 in rb_wt_mul_l0

  shl rb_wt_mul_l0, ra5.16a, 1

# r3 = 0

:yloop
# retrieve texture results and pick out bytes
# then submit two more texture requests

# If we knew there was no clipping then this code would get simpler.
# Perhaps we could add on the pitch and clip using larger values?

# N.B. Whilst y == y2 as far as this loop is concerned we will start
# the grab for the next block before we finish with this block and that
# might be B where y != y2 so we must do full processing on both y and y2

  sub.setf -, r3, rb_i_tmu      ; v8adds rb_i, r3, ra_k1             ; ldtmu1
  shr r1, r4, rb_xshift2        ; mov.ifz ra_y_y2, ra_y_y2_next      ; ldtmu0
  shr r0, r4, ra_xshift         ; mov r3, rb_pitch

  max r2, ra_y, 0  # y
  min r2, r2, rb_max_y          ; mov.ifz ra_base, ra_base_next
  add ra_y, ra_y, 1             ; mul24 r2, r2, r3
  add t0s, ra_base, r2          ; mov.ifz ra_base2, rb_base2_next

  max r2, ra_y2, 0
  min r2, r2, rb_max_y
  add ra_y2, ra_y2, 1           ; mul24 r2, r2, r3
  add t1s, ra_base2, r2         ; v8min r0, r0, rb_k255 # v8subs masks out all but bottom byte

# generate seven shifted versions
# interleave with scroll of vertical context

  mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

# apply horizontal filter
  and r1, r1, rb_k255   ; mul24      r3, ra0.8a,      r0
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
  sub r0, r2, r3        ; mov r3, rb_i

  sub.setf -, r3, 8     ; mov r1,   ra8
  mov ra8,  ra9         ; mov rb8,  rb9
  brr.anyn -, r:yloop
  mov ra9,  ra10        ; mov rb9,  rb10
  mov ra10, ra11        ; mov rb10, rb11
  mov ra11, r0          ; mov rb11, r1
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
  sub r1, r1, r0        ; mov -, vw_wait
# At this point r1 is a 22-bit signed quantity: 8 (original sample),
#  +6, +6 (each pass), +1 (the passes can overflow slightly), +1 (sign)
# The top 8 bits have rubbish in them as mul24 is unsigned
# The low 6 bits need discard before weighting
  sub.setf -, r3, rb_lcount    ; mul24 r1, r1, ra_k256  # x256 - sign extend & discard rubbish
  asr r1, r1, 14
  nop                   ; mul24 r1, r1, rb_wt_mul_l0
  add r1, r1, rb_wt_off

  shl r1, r1, 8         ; mov r0, ra_height
  brr.anyn -, r:yloop
  asr ra3.8as, r1, rb_wt_den_p15
  mov r1, 16
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

  # r0 = weightL0 << 16, we want it in rb_wt_mul_l0
#  asr rb_wt_mul_l0, r0, i_shift16

:yloopb
# retrieve texture results and pick out bytes
# then submit two more texture requests

# If we knew there was no clipping then this code would get simpler.
# Perhaps we could add on the pitch and clip using larger values?

  sub.setf -, r3, rb_i_tmu      ; v8adds rb_i, r3, ra_k1             ; ldtmu1
  shr r1, r4, rb_xshift2        ; mov.ifz ra_y_y2, ra_y_y2_next      ; ldtmu0
  shr r0, r4, ra_xshift         ; mov r3, rb_pitch

  max r2, ra_y, 0  # y
  min r2, r2, rb_max_y          ; mov.ifz ra_base, ra_base_next
  add ra_y, ra_y, 1             ; mul24 r2, r2, r3
  add t0s, ra_base, r2          ; mov.ifz ra_base2, rb_base2_next

  max r2, ra_y2, 0
  min r2, r2, rb_max_y
  add ra_y2, ra_y2, 1           ; mul24 r2, r2, r3
  add t1s, ra_base2, r2         ; v8min r0, r0, rb_k255 # v8subs masks out all but bottom byte

# generate seven shifted versions
# interleave with scroll of vertical context

  mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

# apply horizontal filter
  and r1, r1, rb_k255   ; mul24      r3, ra0.8a,      r0
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
  sub r0, r2, r3        ; mov r3, rb_i

  sub.setf -, r3, 8     ; mov r1,   ra8
  mov ra8,  ra9         ; mov rb8,  rb9
  brr.anyn -, r:yloopb
  mov ra9,  ra10        ; mov rb9,  rb10
  mov ra10, ra11        ; mov rb10, rb11
  mov ra11, r0          ; mov rb11, r1
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
  sub.setf -, r3, rb_lcount ; mul24 r1, r1, ra_k256

  asr r1, r1, 14
  nop                   ; mul24 r0, r1, rb_wt_mul_l0
  add r0, r0, r2        ; mul24 r1, r1 << 8, ra_wt_mul_l1 << 8    @ "mul_used", 0

  add r1, r1, r0        ; mov -, vw_wait
  shl r1, r1, 8         ; mov r0, ra_height
  brr.anyn -, r:yloopb
  asr ra3.8as, r1, rb_wt_den_p15
  mov r1, 16
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
  brr -, r:yloopb
  nop                   ; mul24 r0, r1, rb_pitch # r0 = pitch*16
  add rb_dest, rb_dest, r0
  mov vw_setup, rb_vpm_init                     # Reset our VDM write pointer
# >>> yloopb


################################################################################

::mc_end
# Do not add code here because mc_end must appear after all other code.
