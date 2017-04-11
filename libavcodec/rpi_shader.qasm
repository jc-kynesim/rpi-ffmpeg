
# The @ "mul_used", 0 annotations that occur by various mul blocks suppress
# the warning that we are using rotation & ra/rb registers. r0..3 can be
# rotated through all 16 elems ra regs can only be routated through their
# local 4.  As it happens this is what is wanted here as we do not want the
# constants from the other half of the calc.

# register allocation
#
# ra0...ra7                                     eight horizontal filter coefficients
#
# rb0 rx_shift2
# rb1 rb_y2_next
#
# rb4...rb7
#
# rb8..rb11, ra8...ra11                         Y: eight filtered rows of context (ra11 == most recent)
#
#                                               (ra15 isn't clamped to zero - this happens during the
#                                                copy to ra14, and during its use in the vertical filter)
#
# rb8...rb11                                    eight vertical filter coefficients

# ra4                                           y: Fiter, UV: part -of b0 -> b stash

# rb12                                          offset to add before shift (round + weighting offsets)
# rb13                                          shift: denom + 6 + 9
# rb14                                          L0 weight (U on left, V on right)
# rb15                                          -- free --
#
# ra16                                          clipped(row start address+elem_num)&~3
# ra17                                          per-channel shifts
# ra18                                          L1 weight (Y)
# ra19                                          next ra17
#
# rb16                                          pitch
# rb17                                          height + 1
# rb18                                          height + 3
# rb19                                          next ra16
#
# ra20                                          1
# ra21                                          ra_21
# ra22 ra_k256                                  256
# ra23 ra_y2_next                               ra_y2_next
#
# rb20                                          -- free -- (xpitch)
# rb21                                          -- free --
# rb22 rb_k255                                  255
# rb23                                          -- free --
#
# rb24                                          vdw_setup_1(dst_pitch)
# rb25                                          frame width-1
# rb26                                          height<<23 + width<<16 + vdw_setup_0
# rb27                                          vdw_setup_0 (depends on QPU number)
# rb28                                          vpm_setup (depends on QPU number) for writing 8bit results into VPM
# rb29                                          vdw_setup_1(dst_pitch-width)
# rb30                                          frame height-1
# rb31                                          used as temp to count loop iterations
#
# ra24                                          clipped(row start address+8+elem_num)&~3
# ra25                                          per-channel shifts 2
# ra26                                          next ra24
# ra27                                          next ra25
# ra28                                          next y
# ra29                                          y for next texture access
#
# Use an even numbered register as a link register to avoid corrupting flags
# ra30                                          next kernel address
# ra31                                          chroma-B height+3; free otherwise

.set SRC_RASTER, 0
.set SRC_STRIPE_WIDTH, 128
.set SRC_STRIPE_SHIFT, 7


.set rb_frame_width_minus_1,       rb25
.set rb_frame_height_minus_1,      rb30
.set rb_pitch,                     rb16
.set ra_x,                         ra16
.set ra_y2,                        ra21.16a
.set ra_y2_next,                   ra21.16b

.set rb_x_next,                    rb19
.set rb_frame_base2_next,          rb19

.set ra_frame_base,                ra24
.set ra_frame_base_next,           ra26
.set ra_xshift,                    ra17

.set ra_u2v_ref_offset,            ra25
.set ra_frame_base2,               ra25

.set ra_xshift_next,               ra19
.set rx_xshift2,                   rb0
.set rx_xshift2_next,              rb1

.set ra_u2v_dst_offset,            ra27

.set ra_y_next,                    ra28
.set ra_y,                         ra29

.set ra_k1,                        ra20
.set rb_xpitch,                    rb20
.set rb_k255,                      rb22
.set ra_k256,                      ra22

.set ra_link,                      ra30

# With shifts only the bottom 5 bits are considered so -16=16, -15=17 etc.
.set i_shift16,                    -16
.set i_shift21,                    -11
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


################################################################################
# mc_setup_uv(next_kernel, x, y, ref_u_base, ref_v_base, frame_width, frame_height, pitch, dst_pitch, offset, denom, vpm_id)
::mc_setup_uv
  mov tmurs, 1          ; mov ra_link, unif        # No swap TMUs ; Next fn

# Load first request location
mov ra0, unif
mov r0, elem_num

add ra_x, ra0.16b, r0   # Store x
mov ra_y, ra0.16a       # Store y
mov ra_frame_base, unif # Store frame u base
mov r1, vdw_setup_1(0)  # Merged with dst_stride shortly, delay slot for ra_frame_base
sub ra_u2v_ref_offset, unif, ra_frame_base # Store offset to add to move from u to v in reference frame

# Read image dimensions
sub rb25,unif,1
sub rb30,unif,1

# get source pitch
.if SRC_RASTER
mov rb_pitch, unif
.else
mov rb_pitch, SRC_STRIPE_WIDTH
mov rb_xpitch, unif
.endif

# get destination vdw setup
add rb24, r1, unif      # dst_stride

# load constants
  mov ra_k1, 1
  mov ra_k256, 256
  mov rb_k255, 255

# touch registers to keep simulator happy

  # ra/b4..7: B0 -> B stash registers
  mov ra4, 0 ; mov rb4, 0
  mov ra5, 0 ; mov rb5, 0
  mov ra6, 0 ; mov rb6, 0
  mov ra7, 0 ; mov rb7, 0

  # ra12..15: vertical scroll registers
  mov ra12, 0
  mov ra13, 0
  mov ra14, 0
  mov ra15, 0

  # ra9 - delayed setup - must be 0 initially
  mov ra9, 0

# Compute base address for first and second access
.if SRC_RASTER
mov r0, ra_x           # Load x
max r0, r0, 0                      ; mov r1, ra_y # Load y
min r0, r0, rb_frame_width_minus_1 ; mov r3, ra_frame_base  # Load the frame base
shl ra_xshift_next, r0, 3          ; mov r2, ra_u2v_ref_offset
add ra_y, r1, 1
add r0, r0, r3
and r0, r0, ~3
max r1, r1, 0                      ; mov ra_x, r0 # y
min r1, r1, rb_frame_height_minus_1
# submit texture requests for first line
add r2, r2, r0 ; mul24 r1, r1, rb_pitch
add t0s, r0, r1 ; mov ra_frame_base, r2
add t1s, r2, r1

add rb13, 9, unif   # denominator
mov -, unif         # Unused

mov -, unif   # ??? same as (register) qpu_num

# Compute part of VPM to use for DMA output
m_calc_dma_regs rb28, rb27

# submit texture requests for second line
max r1, ra_y, 0
min r1, r1, rb_frame_height_minus_1
add ra_y, ra_y, 1
bra -, ra_link
nop ; mul24 r1, r1, rb_pitch
add t0s, r1, ra_x
add t1s, r1, ra_frame_base

.else

# ra_x ends up with t0s base
# ra_frame_base ends up with t1s base

  mov r0, ra_x           # Load x
  max r0, r0, 0
  min r0, r0, rb_frame_width_minus_1

# Get shift
  and r1, r0, 1
  shl ra_xshift_next, r1, 4

# In a single 32 bit word we get 2 UV pairs so mask bottom bit of xs

  and r0, r0, -2
  add r0, r0, r0        ; v8subs r1, r1, r1
  sub r1, r1, rb_pitch
  and r1, r0, r1
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add ra_x, ra_frame_base, r0

  mov r1, ra_y          # Load y
  add ra_y, r1, 1       # Set for next
  max r1, r1, 0
  min r1, r1, rb_frame_height_minus_1

# submit texture requests for first line
  nop                   ; mul24 r1, r1, rb_pitch
  add t0s, ra_x, r1

# submit texture requests for 2nd line

  mov r1, ra_y          # Load y
  add ra_y, r1, 1       # Set for next
  max r1, r1, 0
  min r1, r1, rb_frame_height_minus_1

  nop                   ; mul24 r1, r1, rb_pitch
  add t0s, ra_x, r1

  add rb13, 9, unif     # denominator
  mov -, unif           # Unused
  mov -, unif           # ??? same as (register) qpu_num

# Compute part of VPM to use for DMA output
  m_calc_dma_regs rb28, rb27

  bra -, ra_link
  nop
  nop
  nop

.endif




################################################################################

# mc_filter_uv(next_kernel, x, y, frame_u_base, frame_v_base, width_height, hcoeffs, vcoeffs, offset_weight_u, offset_weight_v, this_u_dst, this_v_dst)

# At this point we have already issued two pairs of texture requests for the current block
# ra_x, ra_x16_base point to the current coordinates for this block
::mc_filter_uv
mov ra_link, unif

# per-channel shifts were calculated on the *previous* invocation

# get base addresses and per-channel shifts for *next* invocation
mov ra2, unif         # x_y
mov r0, elem_num      ; mov r3, unif          # frame_base

add r0, ra2.16b, r0   # x
max r0, r0, 0
min r0, r0, rb_frame_width_minus_1

.if SRC_RASTER
sub r2, unif, r3      ; mov ra_xshift, ra_xshift_next
shl ra_xshift_next, r0, 3
add r0, r0, r3        ; mov ra1, unif  # ; width_height
and rb_x_next, r0, ~3 ; mov ra0, unif  # H filter coeffs
mov ra_y_next, ra2.16a ; mov vw_setup, rb28

add ra_frame_base_next, rb_x_next, r2
.else

  mov -, unif           # V base
  mov ra_xshift, ra_xshift_next
  shl ra_xshift_next, r0, 4

  and r0, r0, -2
  add r0, r0, r0        ; v8subs r1, r1, r1
  sub r1, r1, rb_pitch
  and r1, r0, r1
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        # Add stripe offsets
  add rb_x_next, r3, r0

  mov ra1, unif         # ; width_height
  mov ra0, unif         # H filter coeffs
  mov ra_y_next, ra2.16a
  mov vw_setup, rb28

.endif

# set up VPM write
# get width,height of block

sub rb29, rb24, ra1.16b  # Compute vdw_setup1(dst_pitch-width)
add rb17, ra1.16a, 1
add rb18, ra1.16a, 3
shl r0,   ra1.16a, 7

  mov.setf -, ra9     ; mov -, vw_wait
  brr.anyz -, r:filter_uv_1

add r0,   r0, ra1.16b    # Combine width and height of destination area
shl r0,   r0, i_shift16  # Shift into bits 16 upwards of the vdw_setup0 register
add rb26, r0, rb27    ; mov ra3, unif  # ; V filter coeffs
# >>> (skip V DMA if never requested)

  sub vw_setup, ra9, -16
  mov vw_setup, ra10
  mov vw_addr, ra11
:filter_uv_1

mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

# unpack filter coefficients

mov ra1, unif         ; mov rb8,  ra3.8a   # U offset/weight
mov.ifnz ra1, unif    ; mov rb9,  ra3.8b   # V offset/weight
nop                   ; mov rb10, ra3.8c
mov r3, 0             ; mov rb11, ra3.8d   # Loop count

shl r1, ra1.16b, rb13
asr rb12, r1, 1
shl rb14, ra1.16a, 1  # b14 = weight*2

# rb14 - weight L0 * 2
# rb13 = weight denom + 6 + 9
# rb12 = (((is P) ? offset L0 * 2 : offset L1 + offset L0) + 1) << (rb13 - 1)

# r2 is elem_num
# retrieve texture results and pick out bytes
# then submit two more texture requests

# r3 = 0
:uvloop
# retrieve texture results and pick out bytes
# then submit two more texture requests

.if SRC_RASTER
sub.setf -, r3, rb17      ; v8adds r3, r3, ra_k1          ; ldtmu0     # loop counter increment
shr r0, r4, ra_xshift     ; mov.ifz ra_x, rb_x_next       ; ldtmu1
mov.ifz ra_frame_base, ra_frame_base_next ; mov rb31, r3
mov.ifz ra_y, ra_y_next   ; mov r3, rb_pitch
shr r1, r4, ra_xshift    ; v8min r0, r0, rb_k255  # v8subs masks out all but bottom byte

max r2, ra_y, 0  # y
min r2, r2, rb_frame_height_minus_1
add ra_y, ra_y, 1         ; mul24 r2, r2, r3
add t0s, ra_x, r2    ; v8min r1, r1, rb_k255
add t1s, ra_frame_base, r2
.else


  sub.setf -, r3, rb17   ; v8adds r3, r3, ra_k1          ; ldtmu0     # loop counter increment
  shr r0, r4, ra_xshift  ; mov.ifz ra_x, rb_x_next
  nop                   ; mov rb31, r3
  mov.ifz ra_y, ra_y_next ; mov r3, rb_pitch
  mov r1, r0            ; v8min r0, r0, rb_k255  # v8subs masks out all but bottom byte
  shr r1, r1, 8

  max r2, ra_y, 0  # y
  min r2, r2, rb_frame_height_minus_1
  add ra_y, ra_y, 1     ; mul24 r2, r2, r3
  add t0s, ra_x, r2     ; v8min r1, r1, rb_k255

.endif

# generate seven shifted versions
# interleave with scroll of vertical context

mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

# apply horizontal filter
nop                  ; mul24      r3, ra0.8a,       r0
nop                  ; mul24.ifnz r3, ra0.8a << 8,  r1 << 8     @ "mul_used", 0
nop                  ; mul24      r2, ra0.8b << 1,  r0 << 1     @ "mul_used", 0
nop                  ; mul24.ifnz r2, ra0.8b << 9,  r1 << 9     @ "mul_used", 0
sub r2, r2, r3       ; mul24      r3, ra0.8c << 2,  r0 << 2     @ "mul_used", 0
nop                  ; mul24.ifnz r3, ra0.8c << 10, r1 << 10    @ "mul_used", 0
add r2, r2, r3       ; mul24      r3, ra0.8d << 3,  r0 << 3     @ "mul_used", 0
nop                  ; mul24.ifnz r3, ra0.8d << 11, r1 << 11    @ "mul_used", 0
sub r0, r2, r3       ; mov r3, rb31
sub.setf -, r3, 4    ; mov ra12, ra13
brr.anyn -, r:uvloop
mov ra13, ra14          ; mul24 r1, ra14, rb9
mov ra14, ra15
mov ra15, r0            ; mul24 r0, ra12, rb8
# >>> .anyn uvloop

# apply vertical filter and write to VPM

sub r1, r1, r0          ; mul24 r0, ra14, rb10
add r1, r1, r0          ; mul24 r0, ra15, rb11
sub r1, r1, r0          ; mov -, vw_wait
sub.setf -, r3, rb18    ; mul24 r1, r1, ra_k256
asr r1, r1, 14
nop                     ; mul24 r1, r1, rb14
shl r1, r1, 8

add r1, r1, rb12
brr.anyn -, r:uvloop
asr r1, r1, rb13
min r1, r1, rb_k255       # Delay 2
max vpm, r1, 0         # Delay 3
# >>>

# DMA out for U & stash for V
  mov vw_setup, rb26    ; mov ra9, rb26 # VDW setup 0
  bra -, ra_link
  mov vw_setup, rb29    ; mov ra10, rb29 # Stride
  mov vw_addr, unif     # u_dst_addr
  mov ra11, unif        # v_dst_addr
# >>>

################################################################################

# mc_filter_uv_b0(next_kernel, x, y, frame_u_base, frame_v_base, height, hcoeffs[0], hcoeffs[1], vcoeffs[0], vcoeffs[1], this_u_dst, this_v_dst)

# At this point we have already issued two pairs of texture requests for the current block
# ra_x, ra_x16_base point to the current coordinates for this block
::mc_filter_uv_b0
mov -, unif                  # Ignore chain address - always "b"

# per-channel shifts were calculated on the *previous* invocation

# get base addresses and per-channel shifts for *next* invocation
mov ra2, unif         # x_y
mov r0, elem_num      ; mov r3, unif          # frame_base

add r0, ra2.16b, r0   # x
max r0, r0, 0
min r0, r0, rb_frame_width_minus_1

.if SRC_RASTER
# compute offset from frame base u to frame base v
sub r2, unif, r3      ; mov ra_xshift, ra_xshift_next
shl ra_xshift_next, r0, 3
add r0, r0, r3        ; mov ra1, unif  # ; width_height
and rb_x_next, r0, ~3 ; mov ra0, unif  # H filter coeffs
mov ra_y_next, ra2.16a

add ra_frame_base_next, rb_x_next, r2
.else

  mov -, unif           # V base
  mov ra_xshift, ra_xshift_next
  shl ra_xshift_next, r0, 4

  and r0, r0, -2
  add r0, r0, r0        ; v8subs  r1, r1, r1
  sub r1, r1, rb_pitch
  and r1, r0, r1
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        # Add stripe offsets
  add rb_x_next, r3, r0

  mov ra1, unif         # ; width_height
  mov ra0, unif         # H filter coeffs
  mov ra_y_next, ra2.16a

.endif

# Need to have unsigned coeffs to so we can just unpack in the filter
# chroma filter always goes -ve, +ve, +ve, -ve. This is fixed in the
# filter code. Unpack into b regs for V

sub rb29, rb24, ra1.16b         # Compute vdw_setup1(dst_pitch-width)
add rb17, ra1.16a, 1
add ra31, ra1.16a, 3
shl r0,   ra1.16a, 7
add r0,   r0, ra1.16b        ; mov ra3, unif   # Combine width and height of destination area ; V filter coeffs
shl r0,   r0, i_shift16      ; mov rb14, unif  # U weight L0
add rb26, r0, rb27

mov rb8, ra3.8a
mov rb9, ra3.8b
mov rb10, ra3.8c
mov rb11, ra3.8d

# r2 is elem_num
# r3 is loop counter

mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

mov.ifnz rb14, unif    ; mov r3, 0  # V weight L0 ; Loop counter

# rb14 unused in b0 but will hang around till the second pass

# retrieve texture results and pick out bytes
# then submit two more texture requests

# r3 = 0
:uvloop_b0
# retrieve texture results and pick out bytes
# then submit two more texture requests

.if SRC_RASTER
  sub.setf -, r3, rb17  ; v8adds r3, r3, ra_k1          ; ldtmu0     # loop counter increment
  shr r0, r4, ra_xshift ; mov.ifz ra_x, rb_x_next       ; ldtmu1
  mov.ifz ra_frame_base, ra_frame_base_next ; mov rb31, r3
  mov.ifz ra_y, ra_y_next ; mov r3, rb_pitch
  shr r1, r4, ra_xshift ; v8min r0, r0, rb_k255          # v8subs masks out all but bottom byte

  max r2, ra_y, 0       # y
  min r2, r2, rb_frame_height_minus_1
  add ra_y, ra_y, 1     ; mul24 r2, r2, r3
  add t0s, ra_x, r2     ; v8min r1, r1, rb_k255
  add t1s, ra_frame_base, r2
.else

  sub.setf -, r3, rb17  ; v8adds r3, r3, ra_k1          ; ldtmu0     # loop counter increment
  shr r0, r4, ra_xshift ; mov.ifz ra_x, rb_x_next
  nop                   ; mov rb31, r3
  mov.ifz ra_y, ra_y_next ; mov r3, rb_pitch
  mov r1, r0            ; v8min r0, r0, rb_k255          # v8subs masks out all but bottom byte
  shr r1, r1, 8

  max r2, ra_y, 0       # y
  min r2, r2, rb_frame_height_minus_1
  add ra_y, ra_y, 1     ; mul24 r2, r2, r3
  add t0s, ra_x, r2     ; v8min r1, r1, rb_k255

.endif

# generate seven shifted versions
# interleave with scroll of vertical context

  mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

  nop                   ; mul24      r3, ra0.8a,       r0
  nop                   ; mul24.ifnz r3, ra0.8a << 8,  r1 << 8  @ "mul_used", 0
  nop                   ; mul24      r2, ra0.8b << 1,  r0 << 1  @ "mul_used", 0
  nop                   ; mul24.ifnz r2, ra0.8b << 9,  r1 << 9  @ "mul_used", 0
  sub r2, r2, r3        ; mul24      r3, ra0.8c << 2,  r0 << 2  @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8c << 10, r1 << 10 @ "mul_used", 0
  add r2, r2, r3        ; mul24      r3, ra0.8d << 3,  r0 << 3  @ "mul_used", 0
  nop                   ; mul24.ifnz r3, ra0.8d << 11, r1 << 11 @ "mul_used", 0
  sub r0, r2, r3        ; mov r3, rb31
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

  sub.setf -, r3, ra31
  asr ra8.16a, r1, 6    ; mov rb6, ra5          # This discards the high bits that might be bad
  brr.anyn -, r:uvloop_b0
  mov ra5, rb4          ; mov rb4, ra4
  mov ra4, rb5          ; mov rb5, ra6
  mov ra6, rb7          ; mov rb7, ra8
# >>>

# 1st half done all results now in the a/b4..7 fifo

# Need to bulk rotate FIFO for heights other than 16
# plausible heights are 16, 12, 8, 6, 4, 3, 2 and that is all we deal with
# we are allowed 3/4 cb_size w/h :-(

# Destination uniforms discarded
# At the end drop through to _b - we will always do b after b0

  sub.setf -, 15, r3    # 12 + 3 of preroll
  brr.anyn -, r:uv_b0_post_fin                  # h > 12 (n) => 16 (do nothing)
  sub r3, 11, r3        ; mov -, unif           # r3 = shifts wanted ; Discard u_dst_addr
  mov r0, i_shift16     ; mov -, unif           # ; Discard v_dst_addr
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
  # drop through

################################################################################

::mc_filter_uv_b

  mov ra_link, unif
  mov.setf -, ra9       ; mov -, vw_wait  # Delayed V DMA
  brr.anyz -, r:uv_filter_b_1

  mov ra0, unif         ; mov r0, elem_num

# per-channel shifts were calculated on the *previous* invocation

# set up VPM write
mov ra_xshift, ra_xshift_next      ; mov vw_setup, rb28

# get base addresses and per-channel shifts for *next* invocation
add r0, ra0.16b, r0    # x
# >>>
  sub vw_setup, ra9, -16
  mov vw_setup, ra10
  mov vw_addr, ra11
:uv_filter_b_1

.if SRC_RASTER
max r0, r0, 0                      ; mov ra_y_next, ra0.16a # y
min r0, r0, rb_frame_width_minus_1 ; mov r3, unif        # V frame_base
# compute offset from frame base u to frame base v
sub r2, unif, r3                   ; mul24 ra_xshift_next, r0, 8 # U frame_base
add r0, r0, r3                     ; mov -, unif         # discard width_height
and rb_x_next, r0, ~3              ; mov ra0, unif       # H filter coeffs
.else

  max r0, r0, 0                      ; mov ra_y_next, ra0.16a # y
  min r0, r0, rb_frame_width_minus_1 ; mov r3, unif        # U frame_base

  mov ra_xshift, ra_xshift_next      ; mov -, unif         # V base
  shl ra_xshift_next, r0, 4

  and r0, r0, -2
  add r0, r0, r0        ; v8subs r1, r1, r1
  sub r1, r1, rb_pitch
  and r1, r0, r1
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        # Add stripe offsets
  add rb_x_next, r3, r0

  mov -, unif           # ; width_height
  mov ra0, unif         # H filter coeffs

.endif

# rb17, rb26, rb29, ra31 inherited from B0 as w/h must be the same

mov ra3, unif #  V filter coeffs

# get filter coefficients

mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

# Get offset & weight stuff

# The unif read occurs unconditionally, only the write is conditional
mov      ra1, unif  ; mov rb8,  ra3.8a    # U offset/weight ;
mov.ifnz ra1, unif  ; mov rb9,  ra3.8b    # V offset/weight ;
add ra_frame_base_next, rb_x_next, r2 ; mov rb10, ra3.8c
mov r3, 0           ; mov rb11, ra3.8d    # Loop counter ;

shl r1, ra1.16b, rb13
asr rb12, r1, 1

# ra1.16a used directly in the loop

# retrieve texture results and pick out bytes
# then submit two more texture requests

# r3 = 0
:uvloop_b
# retrieve texture results and pick out bytes
# then submit two more texture requests

.if SRC_RASTER
sub.setf -, r3, rb17      ; v8adds r3, r3, ra_k1          ; ldtmu0     # loop counter increment
shr r0, r4, ra_xshift     ; mov.ifz ra_x, rb_x_next       ; ldtmu1
mov.ifz ra_frame_base, ra_frame_base_next ; mov rb31, r3
mov.ifz ra_y, ra_y_next   ; mov r3, rb_pitch
shr r1, r4, ra_xshift     ; v8min r0, r0, rb_k255  # v8subs masks out all but bottom byte

max r2, ra_y, 0  # y
min r2, r2, rb_frame_height_minus_1
add ra_y, ra_y, 1         ; mul24 r2, r2, r3
add t0s, ra_x, r2         ; v8min r1, r1, rb_k255
add t1s, ra_frame_base, r2
.else

  sub.setf -, r3, rb17      ; v8adds r3, r3, ra_k1          ; ldtmu0     # loop counter increment
  shr r0, r4, ra_xshift     ; mov.ifz ra_x, rb_x_next
  nop                   ; mov rb31, r3
  mov.ifz ra_y, ra_y_next ; mov r3, rb_pitch
  mov r1, r0            ; v8min r0, r0, rb_k255  # v8subs masks out all but bottom byte
  shr r1, r1, 8

  max r2, ra_y, 0  # y
  min r2, r2, rb_frame_height_minus_1
  add ra_y, ra_y, 1         ; mul24 r2, r2, r3
  add t0s, ra_x, r2         ; v8min r1, r1, rb_k255

.endif

# generate seven shifted versions
# interleave with scroll of vertical context

mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

nop                  ; mul24      r3, ra0.8a,       r0
nop                  ; mul24.ifnz r3, ra0.8a << 8,  r1 << 8     @ "mul_used", 0
nop                  ; mul24      r2, ra0.8b << 1,  r0 << 1     @ "mul_used", 0
nop                  ; mul24.ifnz r2, ra0.8b << 9,  r1 << 9     @ "mul_used", 0
sub r2, r2, r3       ; mul24      r3, ra0.8c << 2,  r0 << 2     @ "mul_used", 0
nop                  ; mul24.ifnz r3, ra0.8c << 10, r1 << 10    @ "mul_used", 0
add r2, r2, r3       ; mul24      r3, ra0.8d << 3,  r0 << 3     @ "mul_used", 0
nop                  ; mul24.ifnz r3, ra0.8d << 11, r1 << 11    @ "mul_used", 0
sub r0, r2, r3       ; mov r3, rb31
sub.setf -, r3, 4    ; mov ra12, ra13
brr.anyn -, r:uvloop_b
mov ra13, ra14          ; mul24 r1, ra14, rb9
mov ra14, ra15          ; mul24 r2, ra15, rb10
mov ra15, r0            ; mul24 r0, ra12, rb8
# >>> .anyn uvloop_b

# apply vertical filter and write to VPM

  sub r1, r1, r0        ; mov ra8.16b, ra7      # FIFO rotate (all ra/b4..7)
  add r1, r1, r2        ; mul24 r0, ra15, rb11
  sub r1, r1, r0        ; mul24 r0, ra7.16b, rb14
  mov ra7, rb6          ; mul24 r1, r1, ra_k256
  asr r1, r1, 14        ; mov rb6, ra5 # shift2=6

  mov ra5, rb4          ; mul24 r1, r1, ra1.16a
  add r1, r1, r0        ; mov rb4, ra4

  mov ra4, rb5          ; mul24 r1, r1, ra_k256 # Lose bad top 8 bits & sign extend
  add r1, r1, rb12      ; mov rb5, ra6          # rb12 = (offsetL0 + offsetL1 + 1) << (rb13 - 1)

  sub.setf -, r3, ra31  ; mov ra6, rb7
  brr.anyn -, r:uvloop_b
  asr ra3.8as, r1, rb13
  mov -, vw_wait        ; mov rb7, ra8          #  vw_wait is B-reg (annoyingly) ; Final FIFO mov
  mov vpm, ra3.8a
# >>>

# DMA out for U & stash for V

  mov vw_setup, rb26    ; mov ra9, rb26 # VDW setup 0
  bra -, ra_link
  mov vw_setup, rb29    ; mov ra10, rb29 # Stride
  mov vw_addr, unif     # u_dst_addr
  mov ra11, unif        # v_dst_addr



################################################################################

# mc_exit()

::mc_exit_c
  mov.setf -, ra9      ; mov -, vw_wait
# Annoyingly it looks iike condition codes don't work on writes to special
# registers so we have to branch around the writes
  brr.anyz -, r:exit_c_1
  nop
  nop
  nop
# >>>

  sub vw_setup, ra9, -16
  mov vw_setup, ra10
  mov vw_addr, ra11
  nop
:exit_c_1
  ldtmu0
  mov  -, vw_wait ; nop ; ldtmu0 # wait on the VDW

  mov -,srel(0)

  nop        ; nop ; thrend
  nop        ; nop # delay slot 1
  nop        ; nop # delay slot 2

::mc_exit
  ldtmu0
  ldtmu1
  ldtmu0
  mov  -, vw_wait ; nop ; ldtmu1 # wait on the VDW

  mov -,srel(0)

  nop        ; nop ; thrend
  nop        ; nop # delay slot 1
  nop        ; nop # delay slot 2

# mc_interrupt_exit8()
#::mc_interrupt_exit8
#mov  -, vw_wait # wait on the VDW
#
#ldtmu0
#ldtmu1
#ldtmu0
#ldtmu1
#
#mov -,sacq(0) # 1
#mov -,sacq(0) # 2
#mov -,sacq(0) # 3
#mov -,sacq(0) # 4
#mov -,sacq(0) # 5
#mov -,sacq(0) # 6
#mov -,sacq(0) # 7
#
#nop        ; nop ; thrend
#mov interrupt, 1; nop # delay slot 1
#nop        ; nop # delay slot 2
#




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
.if SRC_RASTER
  mov rb_pitch, unif    # src_pitch [ra3 delay]
.else
  mov rb_pitch, SRC_STRIPE_WIDTH
  mov rb_xpitch, unif    # src_pitch [ra3 delay]
.endif
  sub rb_frame_width_minus_1, ra3.16b, 1
  sub rb_frame_height_minus_1, ra3.16a, 1

# get destination pitch
  mov r1, vdw_setup_1(0)
  or  rb24, r1, unif    # dst_pitch

# Compute base address for first and second access
  mov r3, elem_num
  add r0, ra8.16a, r3   # Load x + elem_num
  max r0, r0, 0
  min r0, r0, rb_frame_width_minus_1
  shl ra_xshift_next, r0, 3 # Compute shifts

.if SRC_RASTER
  add ra_y, ra8.16b, 1
  and r0, r0, ~3        # r0 gives the clipped and aligned x coordinate
  add r2, ra9, r0       # ra9 is address for frame0 (not including y offset)
  max r1, ra8.16b, 0
  min r1, r1, rb_frame_height_minus_1
  nop                   ; mul24 r1, r1, rb_pitch   # r2 contains the addresses (not including y offset) for frame0
  add t0s, r2, r1       ; mov ra_frame_base, r2
.else

# In a single 32 bit word we get 4 Y Pels so mask 2 bottom bits of xs

  and r0, r0, -4        ; v8subs r2, r2, r2
  sub r2, r2, rb_pitch
  and r1, r0, r2
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        # Add stripe offsets
  add ra_frame_base, ra9, r0

  mov r1, ra8.16b       # Load y
  add ra_y, r1, 1       # Set for next
  max r1, r1, 0
  min r1, r1, rb_frame_height_minus_1

# submit texture requests for first line
  nop                   ; mul24 r1, r1, rb_pitch
  add t0s, ra_frame_base, r1

.endif

  # r3 still contains elem_num
  add r0, ra10.16a, r3  # Load x
  max r0, r0, 0
  min r0, r0, rb_frame_width_minus_1
  shl rx_xshift2_next, r0, 3 # Compute shifts

.if SRC_RASTER
  add ra_y2, ra10.16b, 1
  and r0, r0, ~3        # r0 gives the clipped and aligned x coordinate
  add r2, ra11, r0      # r2 is address for frame1 (not including y offset)
  max r1, ra10.16b, 0
  min r1, r1, rb_frame_height_minus_1
  nop                   ; mul24 r1, r1, rb_pitch   # r2 contains the addresses (not including y offset) for frame0
  add t1s, r2, r1       ; mov ra_frame_base2, r2
.else

  # r2 still contains mask
  and r0, r0, -4
  and r1, r0, r2
  xor r0, r0, r1        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        # Add stripe offsets
  add ra_frame_base2, ra11, r0

  mov r1, ra10.16b       # Load y
  add ra_y2, r1, 1       # Set for next
  max r1, r1, 0
  min r1, r1, rb_frame_height_minus_1

# submit texture requests for first line
  nop                   ; mul24 r1, r1, rb_pitch
  add t1s, ra_frame_base2, r1

.endif

# load constants

  mov ra_k1, 1
  mov ra_k256, 256
  mov rb_k255, 255

# touch vertical context to keep simulator happy

  mov ra8,  0           ; mov rb8,  0
  mov ra9,  0           ; mov rb9,  0
  mov ra10, 0           ; mov rb10, 0
  mov ra11, 0           ; mov rb11, 0

# Compute part of VPM to use
  m_calc_dma_regs rb28, rb27

# Weighted prediction denom
  add rb13, unif, 9     # unif = weight denom + 6

# submit texture requests for second line
  max r1, ra_y, 0
  min r1, r1, rb_frame_height_minus_1
  add ra_y, ra_y, 1
  mov -, unif           ; mul24 r1, r1, rb_pitch  # unused ;
  add t0s, r1, ra_frame_base

  max r1, ra_y2, 0
  min r1, r1, rb_frame_height_minus_1
  add ra_y2, ra_y2, 1
  nop                   ; mul24 r1, r1, rb_pitch
  add t1s, r1, ra_frame_base2

# FALL THROUGHT TO PER-BLOCK SETUP

# Start of per-block setup code
# P and B blocks share the same setup code to save on Icache space
:per_block_setup
  mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
  mov ra_link, unif

  mov ra1, unif  ; mov r3, elem_num  # y_x ; elem_num has implicit unpack??

# per-channel shifts were calculated on the *previous* invocation
  mov ra_xshift, ra_xshift_next
  mov rx_xshift2, rx_xshift2_next

# get base addresses and per-channel shifts for *next* invocation

.if SRC_RASTER
  add r0, ra1.16a, r3 # Load x
  max r0, r0, 0
  min r0, r0, rb_frame_width_minus_1 ; mov r2, unif  # Load the frame base
  shl ra_xshift_next, r0, 3 # Compute shifts
  mov ra_y_next, ra1.16b
  and r0, r0, ~3                     ; mov ra1, unif # y2_x2
  add ra_frame_base_next, r2, r0

  add r0, ra1.16a, r3 # Load x
  max r0, r0, 0
  min r0, r0, rb_frame_width_minus_1 ; mov r2, unif  # Load the frame base
  shl rx_xshift2_next, r0, 3         # Compute shifts
  mov ra_y2_next, ra1.16b
  and r0, r0, ~3                     ; mov ra1, unif  # width_height ; r0 gives the clipped and aligned x coordinate
  add rb_frame_base2_next, r2, r0    # r2 is address for frame1 (not including y offset)
.else
  add r0, ra1.16a, r3 # Load x
  max r0, r0, 0
  min r0, r0, rb_frame_width_minus_1

  mov r2, SRC_STRIPE_WIDTH - 4
  shr r1, r0, SRC_STRIPE_SHIFT
  and r0, r0, r2        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        # Add stripe offsets
  add ra_frame_base_next, unif, r0              # Base1
  mov ra1, unif         # x2_y2
  mov ra_y_next, ra1.16b                      # Load y

  add r0, ra1.16a, r3   # Load x
  max r0, r0, 0
  min r0, r0, rb_frame_width_minus_1

  shr r1, r0, SRC_STRIPE_SHIFT
  and r0, r0, r2        ; mul24 r1, r1, rb_xpitch
  add r0, r0, r1        # Add stripe offsets
  add rb_frame_base2_next, unif, r0              # Base1
  mov ra1, unif         # width_height
  mov ra_y2_next, ra1.16b                      # Load y

.endif

# set up VPM write
  mov vw_setup, rb28

# get width,height of block (unif load above)
  sub rb29, rb24, ra1.16b # Compute vdw_setup1(dst_pitch-width)
  add rb17, ra1.16a, 5
  add rb18, ra1.16a, 7
  shl r0,   ra1.16a, 7
  add r0,   r0, ra1.16b # Combine width and height of destination area
  shl r0,   r0, i_shift16 # Shift into bits 16 upwards of the vdw_setup0 register
  add rb26, r0, rb27                 ; mov r0, unif   # Packed filter offsets

# get filter coefficients and discard unused B frame values
  shl.ifz r0, r0, i_shift16          ; mov ra5, unif    #  Pick half to use ; L0 offset/weight
  mov r2, 0x01040400                 # [ra5 delay]
  shl ra8, r0, 3                     ; mov rb14, ra5.16a

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

  mov rb4, ra3.8a            ; mov ra18, unif
  mov rb5, ra3.8b
  mov rb6, ra3.8c
  mov.ifnz ra5, ra18

  bra -, ra_link

  shl r0, ra5.16b, rb13      # Offset calc
  asr rb12, r0, 9            # For B l1 & L0 offsets should be identical so it doesn't matter which we use
  mov r3, 0                  ; mov rb7, ra3.8d
# >>> branch ra_link
#
# r3 = 0
# ra18.16a = weight L1
# ra5.16a  = weight L0/L1 depending on side (wanted for 2x mono-pred)
# rb12     = (((is P) ? offset L0/L1 * 2 : offset L1 + offset L0) + 1) << (rb13 - 1)
# rb13     = weight denom + 6 + 9
# rb14     = weight L0


################################################################################
# mc_filter(y_x, frame_base, y2_x2, frame_base2, width_height, my2_mx2_my_mx, offsetweight0, this_dst, next_kernel)
# In a P block, y2_x2 should be y_x+8
# At this point we have already issued two pairs of texture requests for the current block

::mc_filter
# ra5.16a = weight << 16; We want weight * 2 in rb14

  shl rb14, ra5.16a, 1

# r3 = 0

:yloop
# retrieve texture results and pick out bytes
# then submit two more texture requests

# If we knew there was no clipping then this code would get simpler.
# Perhaps we could add on the pitch and clip using larger values?

# N.B. Whilst y == y2 as far as this loop is concerned we will start
# the grab for the next block before we finish with this block and that
# might be B where y != y2 so we must do full processing on both y and y2

  sub.setf -, r3, rb17      ; v8adds r3, r3, ra_k1                           ; ldtmu0
  shr r0, r4, ra_xshift     ; mov.ifz ra_frame_base2, rb_frame_base2_next    ; ldtmu1
  mov.ifz ra_frame_base, ra_frame_base_next ; mov rb31, r3
  mov.ifz ra_y, ra_y_next   ; mov r3, rb_pitch
  shr r1, r4, rx_xshift2    ; mov.ifz ra_y2, ra_y2_next

  max r2, ra_y, 0  # y
  min r2, r2, rb_frame_height_minus_1
  add ra_y, ra_y, 1            ; mul24 r2, r2, r3
  add t0s, ra_frame_base, r2   ; v8min r0, r0, rb_k255 # v8subs masks out all but bottom byte

  max r2, ra_y2, 0  # y
  min r2, r2, rb_frame_height_minus_1
  add ra_y2, ra_y2, 1          ; mul24 r2, r2, r3
  add t1s, ra_frame_base2, r2  ; v8min r1, r1, rb_k255

# generate seven shifted versions
# interleave with scroll of vertical context

  mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

# apply horizontal filter
  nop                  ; mul24      r3, ra0.8a,      r0
  nop                  ; mul24.ifnz r3, ra0.8a << 8, r1 << 8    @ "mul_used", 0
  nop                  ; mul24      r2, ra0.8b << 1, r0 << 1    @ "mul_used", 0
  nop                  ; mul24.ifnz r2, ra0.8b << 9, r1 << 9    @ "mul_used", 0
  sub r2, r2, r3       ; mul24      r3, ra0.8c << 2, r0 << 2    @ "mul_used", 0
  nop                  ; mul24.ifnz r3, ra0.8c << 10, r1 << 10  @ "mul_used", 0
  sub r2, r2, r3       ; mul24      r3, ra0.8d << 3, r0 << 3    @ "mul_used", 0
  nop                  ; mul24.ifnz r3, ra0.8d << 11, r1 << 11  @ "mul_used", 0
  add r2, r2, r3       ; mul24      r3, ra1.8a << 4, r0 << 4    @ "mul_used", 0
  nop                  ; mul24.ifnz r3, ra1.8a << 12, r1 << 12  @ "mul_used", 0
  add r2, r2, r3       ; mul24      r3, ra1.8b << 5, r0 << 5    @ "mul_used", 0
  nop                  ; mul24.ifnz r3, ra1.8b << 13, r1 << 13  @ "mul_used", 0
  sub r2, r2, r3       ; mul24      r3, ra1.8c << 6, r0 << 6    @ "mul_used", 0
  nop                  ; mul24.ifnz r3, ra1.8c << 14, r1 << 14  @ "mul_used", 0
  add r2, r2, r3       ; mul24      r3, ra1.8d << 7, r0 << 7    @ "mul_used", 0
  nop                  ; mul24.ifnz r3, ra1.8d << 15, r1 << 15  @ "mul_used", 0
  sub r0, r2, r3       ; mov r3, rb31

  sub.setf -, r3, 8       ; mov r1,   ra8
  mov ra8,  ra9           ; mov rb8,  rb9
  brr.anyn -, r:yloop
  mov ra9,  ra10          ; mov rb9,  rb10
  mov ra10, ra11          ; mov rb10, rb11
  mov ra11, r0            ; mov rb11, r1
  # >>> .anyn yloop

  # apply vertical filter and write to VPM

  nop                     ; mul24 r0, rb8,  ra2.8a
  nop                     ; mul24 r1, rb9,  ra2.8b
  sub r1, r1, r0          ; mul24 r0, rb10, ra2.8c
  sub r1, r1, r0          ; mul24 r0, rb11, ra2.8d
  add r1, r1, r0          ; mul24 r0, ra8,  rb4
  add r1, r1, r0          ; mul24 r0, ra9,  rb5
  sub r1, r1, r0          ; mul24 r0, ra10, rb6
  add r1, r1, r0          ; mul24 r0, ra11, rb7
  sub r1, r1, r0          ; mov -, vw_wait
# At this point r1 is a 22-bit signed quantity: 8 (original sample),
#  +6, +6 (each pass), +1 (the passes can overflow slightly), +1 (sign)
# The top 8 bits have rubbish in them as mul24 is unsigned
# The low 6 bits need discard before weighting
  sub.setf -, r3, rb18    ; mul24 r1, r1, ra_k256  # x256 - sign extend & discard rubbish
  asr r1, r1, 14
  nop                     ; mul24 r1, r1, rb14
  add r1, r1, rb12

  shl r1, r1, 8
  brr.anyn -, r:yloop
  asr r1, r1, rb13
# We have a saturating pack unit - I can't help feeling it should be useful here
  min r1, r1, rb_k255       # Delay 2  rb_k255 = 255
  max vpm, r1, 0         # Delay 3
# >>> branch.anyn yloop

# DMA out

  brr -, r:per_block_setup
  mov vw_setup, rb26 # VDW setup 0    Delay 1
  mov vw_setup, rb29 # Stride         Delay 2
  mov vw_addr, unif # start the VDW   Delay 3



################################################################################

# mc_filter_b(y_x, frame_base, y2_x2, frame_base2, width_height, my2_mx2_my_mx, offsetweight0, this_dst, next_kernel)
# In a P block, only the first half of coefficients contain used information.
# At this point we have already issued two pairs of texture requests for the current block
# May be better to just send 16.16 motion vector and figure out the coefficients inside this block (only 4 cases so can compute hcoeffs in around 24 cycles?)
# Can fill in the coefficients so only
# Can also assume default weighted prediction for B frames.
# Perhaps can unpack coefficients in a more efficient manner by doing H/V for a and b at the same time?
# Or possibly by taking advantage of symmetry?
# From 19->7 32bits per command.

::mc_filter_b
  # r0 = weightL0 << 16, we want it in rb14
#  asr rb14, r0, i_shift16

:yloopb
# retrieve texture results and pick out bytes
# then submit two more texture requests

# If we knew there was no clipping then this code would get simpler.
# Perhaps we could add on the pitch and clip using larger values?

  sub.setf -, r3, rb17      ; v8adds r3, r3, ra_k1                           ; ldtmu0
  shr r0, r4, ra_xshift     ; mov.ifz ra_frame_base2, rb_frame_base2_next    ; ldtmu1
  mov.ifz ra_frame_base, ra_frame_base_next ; mov rb31, r3
  mov.ifz ra_y, ra_y_next   ; mov r3, rb_pitch
  shr r1, r4, rx_xshift2    ; mov.ifz ra_y2, ra_y2_next

  max r2, ra_y, 0  # y
  min r2, r2, rb_frame_height_minus_1
  add ra_y, ra_y, 1            ; mul24 r2, r2, r3
  add t0s, ra_frame_base, r2   ; v8min r0, r0, rb_k255 # v8subs masks out all but bottom byte

  max r2, ra_y2, 0  # y
  min r2, r2, rb_frame_height_minus_1
  add ra_y2, ra_y2, 1          ; mul24 r2, r2, r3
  add t1s, ra_frame_base2, r2  ; v8min r1, r1, rb_k255

# generate seven shifted versions
# interleave with scroll of vertical context

  mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

# apply horizontal filter
  nop                  ; mul24      r3, ra0.8a,      r0
  nop                  ; mul24.ifnz r3, ra0.8a << 8, r1 << 8    @ "mul_used", 0
  nop                  ; mul24      r2, ra0.8b << 1, r0 << 1    @ "mul_used", 0
  nop                  ; mul24.ifnz r2, ra0.8b << 9, r1 << 9    @ "mul_used", 0
  sub r2, r2, r3       ; mul24      r3, ra0.8c << 2, r0 << 2    @ "mul_used", 0
  nop                  ; mul24.ifnz r3, ra0.8c << 10, r1 << 10  @ "mul_used", 0
  sub r2, r2, r3       ; mul24      r3, ra0.8d << 3, r0 << 3    @ "mul_used", 0
  nop                  ; mul24.ifnz r3, ra0.8d << 11, r1 << 11  @ "mul_used", 0
  add r2, r2, r3       ; mul24      r3, ra1.8a << 4, r0 << 4    @ "mul_used", 0
  nop                  ; mul24.ifnz r3, ra1.8a << 12, r1 << 12  @ "mul_used", 0
  add r2, r2, r3       ; mul24      r3, ra1.8b << 5, r0 << 5    @ "mul_used", 0
  nop                  ; mul24.ifnz r3, ra1.8b << 13, r1 << 13  @ "mul_used", 0
  sub r2, r2, r3       ; mul24      r3, ra1.8c << 6, r0 << 6    @ "mul_used", 0
  nop                  ; mul24.ifnz r3, ra1.8c << 14, r1 << 14  @ "mul_used", 0
  add r2, r2, r3       ; mul24      r3, ra1.8d << 7, r0 << 7    @ "mul_used", 0
  nop                  ; mul24.ifnz r3, ra1.8d << 15, r1 << 15  @ "mul_used", 0
  sub r0, r2, r3       ; mov r3, rb31

  sub.setf -, r3, 8       ; mov r1,   ra8
  mov ra8,  ra9           ; mov rb8,  rb9
  brr.anyn -, r:yloopb
  mov ra9,  ra10          ; mov rb9,  rb10
  mov ra10, ra11          ; mov rb10, rb11
  mov ra11, r0            ; mov rb11, r1
  # >>> .anyn yloopb

  # apply vertical filter and write to VPM
  nop                     ; mul24 r0, rb8,  ra2.8a
  nop                     ; mul24 r1, rb9,  ra2.8b
  sub r1, r1, r0          ; mul24 r0, rb10, ra2.8c
  sub r1, r1, r0          ; mul24 r0, rb11, ra2.8d
  add r1, r1, r0          ; mul24 r0, ra8,  rb4
  add r1, r1, r0          ; mul24 r0, ra9,  rb5
  sub r1, r1, r0          ; mul24 r0, ra10, rb6
  add r1, r1, r0          ; mul24 r0, ra11, rb7
  sub r1, r1, r0          ; mov r2, rb12
# As with P-pred r1 is a 22-bit signed quantity in 32-bits
# Top 8 bits are bad - low 6 bits should be discarded
  sub.setf -, r3, rb18    ; mul24 r1, r1, ra_k256

  asr r1, r1, 14
  nop                     ; mul24 r0, r1, rb14
  add r0, r0, r2          ; mul24 r1, r1 << 8, ra18.16a << 8    @ "mul_used", 0

  add r1, r1, r0          ; mov -, vw_wait
  shl r1, r1, 8

  brr.anyn -, r:yloopb
  asr r1, r1, rb13         # Delay 1
  min r1, r1, rb_k255       # Delay 2
  max vpm, r1, 0         # Delay 3

# DMA out
  brr -, r:per_block_setup
  mov vw_setup, rb26 # VDW setup 0    Delay 1
  mov vw_setup, rb29 # Stride         Delay 2
  mov vw_addr, unif # start the VDW   Delay 3

################################################################################
::mc_interrupt_exit12c
  mov.setf -, ra9      ; mov -, vw_wait
  brr.anyz -, r:exit12_c_1
  nop
  nop
  nop
# >>>

  sub vw_setup, ra9, -16
  mov vw_setup, ra10
  mov vw_addr, ra11
  mov ra9, 0
:exit12_c_1
  ldtmu0
  mov  -, vw_wait ; nop ; ldtmu0  # wait on the VDW

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

  nop        ; nop ; thrend
  mov interrupt, 1; nop # delay slot 1
  nop        ; nop # delay slot 2

# mc_interrupt_exit12()
::mc_interrupt_exit12
  ldtmu0
  ldtmu1
  ldtmu0
  mov  -, vw_wait ; nop ; ldtmu1  # wait on the VDW

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

  nop        ; nop ; thrend
  mov interrupt, 1; nop # delay slot 1
  nop        ; nop # delay slot 2


::mc_exit1
  mov  -, vw_wait # wait on the VDW

  ldtmu0
  ldtmu1
  ldtmu0
  ldtmu1
  nop        ; nop ; thrend
  mov interrupt, 1; nop # delay slot 1
  nop        ; nop # delay slot 2

::mc_end
# Do not add code here because mc_end must appear after all other code.
