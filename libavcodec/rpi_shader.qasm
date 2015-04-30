# register allocation
#
# ra0...ra7                                     eight horizontal filter coefficients
#
# rb1...rb7                                     seven shifted copies of the current unfiltered row
#
# ra8...ra15                                    eight filtered rows of context (rb15 == most recent)
#
#                                               (ra15 isn't clamped to zero - this happens during the
#                                                copy to ra14, and during its use in the vertical filter)
#
# rb8...rb15                                    eight vertical filter coefficients
#
# ra16                                          clipped(row start address+elem_num)&~3
# ra17                                          per-channel shifts
# ra19                                          next ra17
#
# rb16                                          pitch
# rb17                                          height + 5
# rb18                                          height + 7
# rb19                                          next ra16
#
# ra20                                          1
# ra21                                          64
# ra22                                          256
# ra23                                          8
#
# rb20                                          0xffffff00
# rb21                                          64
# rb22                                          255
# rb23                                          24
#
# rb24                                          vdw_setup_1(dst_pitch)
# rb25                                          frame width-1
# rb26                                          height<<23 + width<<16 + vdw_setup_0
# rb27                                          vdw_setup_0 (depends on QPU number)
# rb28                                          vpm_setup (depends on QPU number)
# rb29                                          vdw_setup_1(dst_pitch-width)
# rb30                                          frame height-1
# rb31                                          used as temp to count loop iterations
#
# ra24...ra30                                   15, 14, 13, 12, 11, 10, 9
# ra24                                          clipped(row start address+8+elem_num)&~3
# ra25                                          per-channel shifts 2
# ra26                                          next ra24
# ra27                                          next ra25
# ra28                                          next y
# ra29                                          y for next texture access
#
# ra31                                          next kernel address

.set rb_frame_width_minus_1,       rb25
.set rb_frame_height_minus_1,      rb30
.set rb_pitch,                     rb16
.set ra_x_base,                    ra16
.set rb_x_base_next,               rb19
.set ra_x2_base,                   ra24
.set ra_x2_base_next,              ra26
.set ra_xshift,                    ra17

.set ra_x2shift,                   ra25
.set ra_u2v_ref_offset,            ra25

.set ra_xshift_next,               ra19

.set ra_x2shift_next,              ra27
.set ra_u2v_dst_offset,            ra27

.set ra_y_next,                    ra28
.set ra_y,                         ra29

.set rb_const_64,                  rb21

# mc_setup(next_kernel, x, y, ref_base, frame_width, frame_height, pitch, dst_pitch, pad0, pad1)
::mc_setup

# Read starting kernel
mov ra31, unif

# Load first request location
add ra_x_base, unif, elem_num # Store x
mov ra_y, unif # Store y
mov ra_x2_base, unif # Store frame base

# Read image dimensions
sub rb25,unif,1
sub rb30,unif,1

# get source pitch
mov rb16, unif

# get destination pitch
mov r0, unif
mov r1, vdw_setup_1(0)
add rb24, r1, r0

# load constants

mov ra20, 1
mov ra21, 64
mov ra22, 256
mov ra23, 8

mov rb20, 0xffffff00
mov rb21, 64
mov rb22, 255
mov rb23, 24

# touch vertical context to keep simulator happy

mov ra8, 0
mov ra9, 0
mov ra10, 0
mov ra11, 0
mov ra12, 0
mov ra13, 0
mov ra14, 0
mov ra15, 0

# Compute part of VPM to use for DMA output
mov r2, qpu_num
and r2, r2, 15
mov r1, r2
asr r1, r1, 2
shl r1, r1, 6
mov r0, r2
and r0, r0, 3
add r0, r0, r1
mov r1, vdw_setup_0(0, 0, dma_h8p(0,0,0)) # height,width added later
shl r0, r0, 5
add rb27, r0, r1

# Compute part of VPM to save data into
mov r2, qpu_num
and r2, r2, 15
mov r1, r2
asr r1, r1, 2
shl r1, r1, 6
mov r0, r2
and r0, r0, 3
add r0, r0, r1
mov r1, vpm_setup(0, 4, h8p(0, 0))
add rb28, r0, r1

# Compute base address for first and second access
#add r0, unif, elem_num     # x
mov r0, ra_x_base           # Load x
add r2, r0, 8               # x+8
max r0, r0, 0; mov r1, ra_y # Load y
min r0, r0, rb_frame_width_minus_1 ; mov r3, ra_x2_base  # Load the frame base
shl ra_xshift_next, r0, 3
max r2, r2, 0
add ra_y, r1, 1
min r2, r2, rb_frame_width_minus_1
shl ra_x2shift_next, r2, 3
max r1, r1, 0  # y
min r1, r1, rb_frame_height_minus_1
add r0, r0, r3; mul24 r1, r1, rb_pitch
add r2, r2, r3
and r0, r0, ~3
and r2, r2, ~3; mov ra_x_base, r0
# submit texture requests for first line
add t0s, r0, r1 ; mov ra_x2_base, r2
add t0s, r2, r1

# Dump padding words
mov r0, unif
mov r0, unif

# submit texture requests for second line
max r1, ra_y, 0
min r1, r1, rb_frame_height_minus_1
add ra_y, ra_y, 1
bra -, ra31
nop ; mul24 r1, r1, rb_pitch
add t0s, r1, ra_x_base
add t0s, r1, ra_x2_base

################################################################################

# mc_filter_uv(next_kernel, x, y, frame_u_base, frame_v_base, height, hcoeffs[0], hcoeffs[1], vcoeffs[0], vcoeffs[1], this_u_dst, this_v_dst)

# At this point we have already issued two pairs of texture requests for the current block
# ra_x_base, ra_x16_base point to the current coordinates for this block
::mc_filter_uv
mov ra31, unif

# per-channel shifts were calculated on the *previous* invocation

mov ra_xshift, ra_xshift_next

# get base addresses and per-channel shifts for *next* invocation
add r0, unif, elem_num    # x
max r0, r0, 0; mov r1, unif # y
min r0, r0, rb_frame_width_minus_1 ; mov r3, unif # frame_base
shl ra_xshift_next, r0, 3
sub r2, unif, r3 # compute offset from frame base u to frame base v
add r0, r0, r3
and rb_x_base_next, r0, ~3
mov ra_y_next, r1
add ra_x2_base_next, rb_x_base_next, r2

# set up VPM write
mov vw_setup, rb28

# get width,height of block
mov r2, 16
mov r0, unif
shr r1, r0, r2 # Extract width
sub rb29, rb24, r1 # Compute vdw_setup1(dst_pitch-width)
and r0, r0, rb22 # Extract height
add rb17, r0, 5
add rb18, r0, 7
shl r0, r0, 7
add r0, r0, r1 # Combine width and height of destination area
shl r0, r0, r2 # Shift into bits 16 upwards of the vdw_setup0 register
add rb26, r0, rb27

sub.setf -,8,r1 # 8-r1, so if <0 (negative) we need to use the full code

# get filter coefficients

mov r0, unif
asr ra3, r0, rb23;      mul24 r0, r0, ra22
asr ra2, r0, rb23;      mul24 r0, r0, ra22
asr ra1, r0, rb23;      mul24 r0, r0, ra22
asr ra0, r0, rb23;      mov r0, unif
asr ra7, r0, rb23;      mul24 r0, r0, ra22
asr ra6, r0, rb23;      mul24 r0, r0, ra22
asr ra5, r0, rb23;      mul24 r0, r0, ra22
asr ra4, r0, rb23;      mov r0, unif
asr rb11, r0, rb23;     mul24 r0, r0, ra22
asr rb10, r0, rb23;     mul24 r0, r0, ra22
asr rb9, r0, rb23;      mul24 r0, r0, ra22
asr rb8, r0, rb23;      mov r0, unif
asr rb15, r0, rb23;     mul24 r0, r0, ra22
asr rb14, r0, rb23;     mul24 r0, r0, ra22
asr rb13, r0, rb23;     mul24 r0, r0, ra22
asr rb12, r0, rb23

# r2 is elem_num
# r3 is loop counter

mov r5rep, -8
mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

# retrieve texture results and pick out bytes
# then submit two more texture requests

mov r3, 0

:uvloop
# retrieve texture results and pick out bytes
# then submit two more texture requests

sub.setf -, r3, rb17      ; v8adds r3, r3, ra20                     ; ldtmu0     # loop counter increment
shr r0, r4, ra_xshift     ; mov.ifz ra_x_base, rb_x_base_next       ; ldtmu0
mov.ifz ra_x2_base, ra_x2_base_next ; mov rb31, r3
mov.ifz ra_y, ra_y_next   ; mov r3, rb_pitch
shr r1, r4, ra_xshift    ; v8subs r0, r0, rb20  # v8subs masks out all but bottom byte

max r2, ra_y, 0  # y
min r2, r2, rb_frame_height_minus_1
add ra_y, ra_y, 1         ; mul24 r2, r2, r3
add t0s, ra_x_base, r2    ; v8subs r1, r1, rb20
add t0s, ra_x2_base, r2

# generate seven shifted versions
# interleave with scroll of vertical context

mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

mov r2, rb21         ; mul24 r3, r0, ra0
nop                  ; mul24.ifnz r3, ra0 << 8, r1 << 8
sub r2, r2, r3       ; mul24      r3, ra1 << 1, r0 << 1
nop                  ; mul24.ifnz r3, ra1 << 9, r1 << 9
sub r2, r2, r3       ; mul24    r3, ra2 << 2, r0 << 2
nop                  ; mul24.ifnz r3, ra2 << 10, r1 << 10
sub r2, r2, r3       ; mul24    r3, ra3 << 3, r0 << 3
nop                  ; mul24.ifnz r3, ra3 << 11, r1 << 11
sub r2, r2, r3       ; mul24    r3, ra4 << 4, r0 << 4
nop                  ; mul24.ifnz r3, ra4 << 12, r1 << 12
sub r2, r2, r3       ; mul24    r3, ra5 << 5, r0 << 5
nop                  ; mul24.ifnz r3, ra5 << 13, r1 << 13
sub r2, r2, r3       ; mul24    r3, ra6 << 6, r0 << 6
nop                  ; mul24.ifnz r3, ra6 << 14, r1 << 14
sub r2, r2, r3       ; mul24    r3, ra7 << 7, r0 << 7
nop                  ; mul24.ifnz r3, ra7 << 15, r1 << 15
sub r0, r2, r3

mov r3, rb31

mov ra8, ra9
mov ra9, ra10
mov ra10, ra11
mov ra11, ra12
mov ra12, ra13
mov ra13, ra14

sub.setf -, r3, 8 ; mov r1, ra22

# apply horizontal filter
brr.anyn -, r:uvloop
max ra14, ra15, 0       ; mul24 r0, r0, r1         # last bit of context scroll, including clamp to zero
asr r0, r0, 15          ; mov r1, ra21
min.setf ra15, r0, rb22

# apply vertical filter and write to VPM

nop                     ; mul24 r0, ra14, rb14
sub r1, r1, r0          ; mul24 r0, ra13, rb13
sub r1, r1, r0          ; mul24 r0, ra12, rb12
sub r1, r1, r0          ; mul24 r0, ra11, rb11
sub r1, r1, r0          ; mul24 r0, ra10, rb10
sub r1, r1, r0          ; mul24 r0, ra9, rb9
sub r1, r1, r0          ; mul24 r0, ra8, rb8
sub r1, r1, r0          ; mul24 r0, ra15, rb15
sub.ifnn r1, r1, r0     ; mov -, vw_wait
sub.setf -, r3, rb18    ; mul24 r1, r1, ra22
brr.anyn -, r:uvloop
asr r1, r1, 15
min r1, r1, rb22
max vpm, r1, 0

# DMA out for U

mov vw_setup, rb26 # VDW setup 0
mov vw_setup, rb29 # Stride
mov vw_addr, unif # start the VDW

# DMA out for V
# We need to wait for the U to complete first, but have nothing useful to compute while we wait.
# Could potentially push this write into the start of the next pipeline stage.
mov r0, 16
mov -, vw_wait

bra -, ra31
add vw_setup, rb26, r0 # VDW setup 0
mov vw_setup, rb29 # Stride
mov vw_addr, unif # start the VDW

################################################################################


# mc_filter(next_kernel, x, y, frame_base, height, hcoeffs[0], hcoeffs[1], vcoeffs[0], vcoeffs[1], this_dst)

# At this point we have already issued two pairs of texture requests for the current block
# ra_x_base, ra_x16_base point to the current coordinates for this block
::mc_filter
mov ra31, unif

# per-channel shifts were calculated on the *previous* invocation

mov ra_xshift, ra_xshift_next
mov ra_x2shift, ra_x2shift_next

# get base addresses and per-channel shifts for *next* invocation
add r0, unif, elem_num    # x
add r2, r0, 8 # x+8
max r0, r0, 0; mov r1, unif # y
min r0, r0, rb_frame_width_minus_1 ; mov r3,unif # frame_base
shl ra_xshift_next, r0, 3
max r2, r2, 0
min r2, r2, rb_frame_width_minus_1
shl ra_x2shift_next, r2, 3
add r0, r0, r3
add r2, r2, r3
and rb_x_base_next, r0, ~3
and ra_x2_base_next, r2, ~3
mov ra_y_next, r1

# set up VPM write
mov vw_setup, rb28

# get width,height of block
mov r2, 16
mov r0, unif
shr r1, r0, r2 # Extract width
sub rb29, rb24, r1 # Compute vdw_setup1(dst_pitch-width)
and r0, r0, rb22 # Extract height
add rb17, r0, 5
add rb18, r0, 7
shl r0, r0, 7
add r0, r0, r1 # Combine width and height of destination area
shl r0, r0, r2 # Shift into bits 16 upwards of the vdw_setup0 register
add rb26, r0, rb27

sub.setf -,8,r1 # 8-r1, so if <0 (negative) we need to use the full code

# get filter coefficients

mov r0, unif
asr ra3, r0, rb23;      mul24 r0, r0, ra22
asr ra2, r0, rb23;      mul24 r0, r0, ra22
asr ra1, r0, rb23;      mul24 r0, r0, ra22
asr ra0, r0, rb23;      mov r0, unif
asr ra7, r0, rb23;      mul24 r0, r0, ra22
asr ra6, r0, rb23;      mul24 r0, r0, ra22
asr ra5, r0, rb23;      mul24 r0, r0, ra22
asr ra4, r0, rb23;      mov r0, unif
asr rb11, r0, rb23;     mul24 r0, r0, ra22
asr rb10, r0, rb23;     mul24 r0, r0, ra22
asr rb9, r0, rb23;      mul24 r0, r0, ra22
asr rb8, r0, rb23;      mov r0, unif
asr rb15, r0, rb23;     mul24 r0, r0, ra22
asr rb14, r0, rb23;     mul24 r0, r0, ra22
asr rb13, r0, rb23;     mul24 r0, r0, ra22
brr.anynn -, r:fast_path
asr rb12, r0, rb23  # delay slot 1

# r2 is elem_num
# r3 is loop counter

mov r5rep, -8 # delay slot 2

mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]  # delay slot 3

# retrieve texture results and pick out bytes
# then submit two more texture requests

## nop                                                                 ; ldtmu0     # loop counter increment
## shr r0, r4, ra17                                                    ; ldtmu0
## shr r1, r4, ra17     ; v8subs r0, r0, rb20
## add t0s, ra16, r5    ; v8subs r1, r1, rb20
## add ra16, ra16, rb16 ; mov t0s, ra16
##
## # generate seven shifted versions
## # interleave with scroll of vertical context
##
## mov r2, rb21         ; mul24 r3, r0, ra0
## sub r2, r2, r3       ; mul24      r3, ra1 << 1, r0 << 1
## nop                  ; mul24.ifnz r3, ra1 << 9, r1 << 9
## sub r2, r2, r3       ; mul24    r3, ra2 << 2, r0 << 2
## nop                  ; mul24.ifnz r3, ra2 << 10, r1 << 10
## sub r2, r2, r3       ; mul24    r3, ra3 << 3, r0 << 3
## nop                  ; mul24.ifnz r3, ra3 << 11, r1 << 11
## sub r2, r2, r3       ; mul24    r3, ra4 << 4, r0 << 4
## nop                  ; mul24.ifnz r3, ra4 << 12, r1 << 12
## sub r2, r2, r3       ; mul24    r3, ra5 << 5, r0 << 5
## nop                  ; mul24.ifnz r3, ra5 << 13, r1 << 13
## sub r2, r2, r3       ; mul24    r3, ra6 << 6, r0 << 6
## nop                  ; mul24.ifnz r3, ra6 << 14, r1 << 14
## sub r2, r2, r3       ; mul24    r3, ra7 << 7, r0 << 7
## nop                  ; mul24.ifnz r3, ra7 << 15, r1 << 15
## sub r2, r2, r3                                                    ; ldtmu0
##
## mov r0, ra22
## shr r0, r4, ra17     ; mul24 r2, r2, r0                           ; ldtmu0
## shr r1, r4, ra17     ; v8subs r0, r0, rb20
## add t0s, ra16, r5    ; v8subs r1, r1, rb20
## add ra16, ra16, rb16 ; mov t0s, ra16
##
## # apply horizontal filter
##
## asr r2, r2, 15    ; mul24 r3, r0, ra0
## min r2, r2, rb22
## max ra13, r2, 0
##
## # generate seven shifted versions
## # interleave with scroll of vertical context
##
## mov r2, rb21
## sub r2, r2, r3 ; mul24      r3, ra1 << 1, r0 << 1
## nop            ; mul24.ifnz r3, ra1 << 9, r1 << 9
## sub r2, r2, r3 ; mul24    r3, ra2 << 2, r0 << 2
## nop            ; mul24.ifnz r3, ra2 << 10, r1 << 10
## sub r2, r2, r3 ; mul24    r3, ra3 << 3, r0 << 3
## nop            ; mul24.ifnz r3, ra3 << 11, r1 << 11
## sub r2, r2, r3 ; mul24    r3, ra4 << 4, r0 << 4
## nop            ; mul24.ifnz r3, ra4 << 12, r1 << 12
## sub r2, r2, r3 ; mul24    r3, ra5 << 5, r0 << 5
## nop            ; mul24.ifnz r3, ra5 << 13, r1 << 13
## sub r2, r2, r3 ; mul24    r3, ra6 << 6, r0 << 6
## nop            ; mul24.ifnz r3, ra6 << 14, r1 << 14
## sub r2, r2, r3 ; mul24    r3, ra7 << 7, r0 << 7
## nop            ; mul24.ifnz r3, ra7 << 15, r1 << 15
## sub r0, r2, r3
##
## # apply horizontal filter
##
## nop          ; mul24 r0, r0, ra22         # last bit of context scroll, including clamp to zero
## asr r0, r0, 15
## min r0, r0, rb22
## max ra14, r0, 0
##
##
##
##
## nop                                                                 ; ldtmu0     # loop counter increment
## shr r0, r4, ra17                                                    ; ldtmu0
## shr r1, r4, ra17     ; v8subs r0, r0, rb20
## add t0s, ra16, r5    ; v8subs r1, r1, rb20
## add ra16, ra16, rb16 ; mov t0s, ra16
##
## # generate seven shifted versions
## # interleave with scroll of vertical context
##
## mov r2, rb21         ; mul24 r3, r0, ra0
## sub r2, r2, r3       ; mul24      r3, ra1 << 1, r0 << 1
## nop                  ; mul24.ifnz r3, ra1 << 9, r1 << 9
## sub r2, r2, r3       ; mul24    r3, ra2 << 2, r0 << 2
## nop                  ; mul24.ifnz r3, ra2 << 10, r1 << 10
## sub r2, r2, r3       ; mul24    r3, ra3 << 3, r0 << 3
## nop                  ; mul24.ifnz r3, ra3 << 11, r1 << 11
## sub r2, r2, r3       ; mul24    r3, ra4 << 4, r0 << 4
## nop                  ; mul24.ifnz r3, ra4 << 12, r1 << 12
## sub r2, r2, r3       ; mul24    r3, ra5 << 5, r0 << 5
## nop                  ; mul24.ifnz r3, ra5 << 13, r1 << 13
## sub r2, r2, r3       ; mul24    r3, ra6 << 6, r0 << 6
## nop                  ; mul24.ifnz r3, ra6 << 14, r1 << 14
## sub r2, r2, r3       ; mul24    r3, ra7 << 7, r0 << 7
## nop                  ; mul24.ifnz r3, ra7 << 15, r1 << 15
## sub r0, r2, r3
##
## # apply horizontal filter
##
## nop          ; mul24 r0, r0, ra22         # last bit of context scroll, including clamp to zero
## asr r0, r0, 15
## min r0, r0, rb22
## max ra15, r0, 0




mov r3, 0

:loop
# retrieve texture results and pick out bytes
# then submit two more texture requests

sub.setf -, r3, rb17      ; v8adds r3, r3, ra20                     ; ldtmu0     # loop counter increment
shr r0, r4, ra_xshift     ; mov.ifz ra_x_base, rb_x_base_next       ; ldtmu0
mov.ifz ra_x2_base, ra_x2_base_next ; mov rb31, r3
mov.ifz ra_y, ra_y_next   ; mov r3, rb_pitch
shr r1, r4, ra_x2shift    ; v8subs r0, r0, rb20  # v8subs masks out all but bottom byte

max r2, ra_y, 0  # y
min r2, r2, rb_frame_height_minus_1
add ra_y, ra_y, 1         ; mul24 r2, r2, r3
add t0s, ra_x_base, r2    ; v8subs r1, r1, rb20
add t0s, ra_x2_base, r2

# generate seven shifted versions
# interleave with scroll of vertical context

mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

mov r2, rb21         ; mul24 r3, r0, ra0
sub r2, r2, r3       ; mul24      r3, ra1 << 1, r0 << 1
nop                  ; mul24.ifnz r3, ra1 << 9, r1 << 9
sub r2, r2, r3       ; mul24    r3, ra2 << 2, r0 << 2
nop                  ; mul24.ifnz r3, ra2 << 10, r1 << 10
sub r2, r2, r3       ; mul24    r3, ra3 << 3, r0 << 3
nop                  ; mul24.ifnz r3, ra3 << 11, r1 << 11
sub r2, r2, r3       ; mul24    r3, ra4 << 4, r0 << 4
nop                  ; mul24.ifnz r3, ra4 << 12, r1 << 12
sub r2, r2, r3       ; mul24    r3, ra5 << 5, r0 << 5
nop                  ; mul24.ifnz r3, ra5 << 13, r1 << 13
sub r2, r2, r3       ; mul24    r3, ra6 << 6, r0 << 6
nop                  ; mul24.ifnz r3, ra6 << 14, r1 << 14
sub r2, r2, r3       ; mul24    r3, ra7 << 7, r0 << 7
nop                  ; mul24.ifnz r3, ra7 << 15, r1 << 15
sub r0, r2, r3

mov r3, rb31

mov ra8, ra9
mov ra9, ra10
mov ra10, ra11
mov ra11, ra12
mov ra12, ra13
mov ra13, ra14

sub.setf -, r3, 8 ; mov r1, ra22

# apply horizontal filter
brr.anyn -, r:loop
max ra14, ra15, 0       ; mul24 r0, r0, r1         # last bit of context scroll, including clamp to zero
asr r0, r0, 15          ; mov r1, ra21
min.setf ra15, r0, rb22

# apply vertical filter and write to VPM

nop                     ; mul24 r0, ra14, rb14
sub r1, r1, r0          ; mul24 r0, ra13, rb13
sub r1, r1, r0          ; mul24 r0, ra12, rb12
sub r1, r1, r0          ; mul24 r0, ra11, rb11
sub r1, r1, r0          ; mul24 r0, ra10, rb10
sub r1, r1, r0          ; mul24 r0, ra9, rb9
sub r1, r1, r0          ; mul24 r0, ra8, rb8
sub r1, r1, r0          ; mul24 r0, ra15, rb15
sub.ifnn r1, r1, r0     ; mov -, vw_wait
sub.setf -, r3, rb18    ; mul24 r1, r1, ra22
brr.anyn -, r:loop
asr r1, r1, 15
min r1, r1, rb22
max vpm, r1, 0

# DMA out

bra -, ra31
mov vw_setup, rb26 # VDW: height rows, 16 8-bit units long
mov vw_setup, rb29
mov vw_addr, unif # start the VDW

####################################################

:fast_path
## nop                                                                 ; ldtmu0     # loop counter increment
## shr r0, r4, ra17                                                    ; ldtmu0
## shr r1, r4, ra17     ; v8subs r0, r0, rb20
## add t0s, ra16, r5    ; v8subs r1, r1, rb20
## add ra16, ra16, rb16 ; mov t0s, ra16
##
## # generate seven shifted versions
## # interleave with scroll of vertical context
##
## mov r2, rb21         ; mul24 r3, r0, ra0
## sub r2, r2, r3       ; mul24 r3, ra1 << 1, r0 << 1
## sub r2, r2, r3       ; mul24 r3, ra2 << 2, r0 << 2
## sub r2, r2, r3       ; mul24 r3, ra3 << 3, r0 << 3
## sub r2, r2, r3       ; mul24 r3, ra4 << 4, r0 << 4
## sub r2, r2, r3       ; mul24 r3, ra5 << 5, r0 << 5
## sub r2, r2, r3       ; mul24 r3, ra6 << 6, r0 << 6
## sub r2, r2, r3       ; mul24 r3, ra7 << 7, r0 << 7
## sub r2, r2, r3                                                    ; ldtmu0
##
## mov r0, ra22
## shr r0, r4, ra17     ; mul24 r2, r2, r0                           ; ldtmu0
## shr r1, r4, ra17     ; v8subs r0, r0, rb20
## add t0s, ra16, r5    ; v8subs r1, r1, rb20
## add ra16, ra16, rb16 ; mov t0s, ra16
##
## # apply horizontal filter
##
## asr r2, r2, 15    ; mul24 r3, r0, ra0
## min r2, r2, rb22
## max ra13, r2, 0
##
## # generate seven shifted versions
## # interleave with scroll of vertical context
##
## mov r2, rb21
## sub r2, r2, r3 ; mul24    r3, ra1 << 1, r0 << 1
## sub r2, r2, r3 ; mul24    r3, ra2 << 2, r0 << 2
## sub r2, r2, r3 ; mul24    r3, ra3 << 3, r0 << 3
## sub r2, r2, r3 ; mul24    r3, ra4 << 4, r0 << 4
## sub r2, r2, r3 ; mul24    r3, ra5 << 5, r0 << 5
## sub r2, r2, r3 ; mul24    r3, ra6 << 6, r0 << 6
## sub r2, r2, r3 ; mul24    r3, ra7 << 7, r0 << 7
## sub r0, r2, r3
##
## # apply horizontal filter
##
## nop          ; mul24 r0, r0, ra22         # last bit of context scroll, including clamp to zero
## asr r0, r0, 15
## min r0, r0, rb22
## max ra14, r0, 0
##
##
##
##
## nop                                                                 ; ldtmu0     # loop counter increment
## shr r0, r4, ra17                                                    ; ldtmu0
## shr r1, r4, ra17     ; v8subs r0, r0, rb20
## add t0s, ra16, r5    ; v8subs r1, r1, rb20
## add ra16, ra16, rb16 ; mov t0s, ra16
##
## # generate seven shifted versions
## # interleave with scroll of vertical context
##
## mov r2, rb21   ; mul24    r3, r0, ra0
## sub r2, r2, r3 ; mul24    r3, ra1 << 1, r0 << 1
## sub r2, r2, r3 ; mul24    r3, ra2 << 2, r0 << 2
## sub r2, r2, r3 ; mul24    r3, ra3 << 3, r0 << 3
## sub r2, r2, r3 ; mul24    r3, ra4 << 4, r0 << 4
## sub r2, r2, r3 ; mul24    r3, ra5 << 5, r0 << 5
## sub r2, r2, r3 ; mul24    r3, ra6 << 6, r0 << 6
## sub r2, r2, r3 ; mul24    r3, ra7 << 7, r0 << 7
## sub r0, r2, r3
##
## # apply horizontal filter
##
## nop          ; mul24 r0, r0, ra22         # last bit of context scroll, including clamp to zero
## asr r0, r0, 15
## min r0, r0, rb22
## max ra15, r0, 0


mov r3, 0  # This signifies the amount of unrolling

:fast_loop
# retrieve texture results and pick out bytes
# then submit two more texture requests

# Due to pipelining we can only skip second pipeline instructions related to the fetched pixels
sub.setf -, r3, rb17      ; v8adds r3, r3, ra20                     ; ldtmu0     # loop counter increment
shr r0, r4, ra_xshift     ; mov.ifz ra_x_base, rb_x_base_next       ; ldtmu0
mov.ifz ra_y, ra_y_next   ; mov rb31, r3
mov.ifz ra_x2_base, ra_x2_base_next   ; mov r3, rb_pitch

max r2, ra_y, 0
min r2, r2, rb_frame_height_minus_1 ; mov r1, r4  # discard texture read
add ra_y, ra_y, 1         ; mul24 r2, r2, r3
add t0s, ra_x_base, r2    ; v8subs r0, r0, rb20
add t0s, ra_x2_base, r2

# generate seven shifted versions
# interleave with scroll of vertical context

mov r2, rb21         ; mul24 r3, r0, ra0
sub r2, r2, r3       ; mul24 r3, ra1 << 1, r0 << 1
sub r2, r2, r3       ; mul24 r3, ra2 << 2, r0 << 2
sub r2, r2, r3       ; mul24 r3, ra3 << 3, r0 << 3
sub r2, r2, r3       ; mul24 r3, ra4 << 4, r0 << 4
sub r2, r2, r3       ; mul24 r3, ra5 << 5, r0 << 5
sub r2, r2, r3       ; mul24 r3, ra6 << 6, r0 << 6
sub r2, r2, r3       ; mul24 r3, ra7 << 7, r0 << 7
sub r0, r2, r3       ; mov r3, rb31

mov ra8, ra9
mov ra9, ra10
mov ra10, ra11
mov ra11, ra12
mov ra12, ra13
mov ra13, ra14

sub.setf -, r3, 8       ; mov r1, ra22

# apply horizontal filter

brr.anyn -, r:fast_loop
max ra14, ra15, 0       ; mul24 r0, r0, r1         # last bit of context scroll, including clamp to zero
asr r0, r0, 15          ; mov r1, ra21
min.setf ra15, r0, rb22

# apply vertical filter and write to VPM

nop                     ; mul24 r0, ra14, rb14
sub r1, r1, r0          ; mul24 r0, ra13, rb13
sub r1, r1, r0          ; mul24 r0, ra12, rb12
sub r1, r1, r0          ; mul24 r0, ra11, rb11
sub r1, r1, r0          ; mul24 r0, ra10, rb10
sub r1, r1, r0          ; mul24 r0, ra9, rb9
sub r1, r1, r0          ; mul24 r0, ra8, rb8
sub r1, r1, r0          ; mul24 r0, ra15, rb15
sub.ifnn r1, r1, r0     ; mov -, vw_wait
sub.setf -, r3, rb18    ; mul24 r1, r1, ra22
brr.anyn -, r:fast_loop
asr r1, r1, 15
min r1, r1, rb22
max vpm, r1, 0

# DMA out

bra -, ra31
mov vw_setup, rb26 # VDW: height rows, 16 8-bit units long
mov vw_setup, rb29
mov vw_addr, unif # start the VDW

################################################################################

# mc_filter_b(next_kernel, x, y, frame_base, height, hcoeffs[0], hcoeffs[1], vcoeffs[0], vcoeffs[1], this_dst)

# At this point we have already issued two pairs of texture requests for the current block
# ra_x_base, ra_x16_base point to the current coordinates for this block
::mc_filter_b
mov ra31, unif

# per-channel shifts were calculated on the *previous* invocation

mov ra_xshift, ra_xshift_next
mov ra_x2shift, ra_x2shift_next

# get base addresses and per-channel shifts for *next* invocation
add r0, unif, elem_num    # x
add r2, r0, 8 # x+8
max r0, r0, 0; mov r1, unif # y
min r0, r0, rb_frame_width_minus_1 ; mov r3,unif # frame_base
shl ra_xshift_next, r0, 3
max r2, r2, 0
min r2, r2, rb_frame_width_minus_1
shl ra_x2shift_next, r2, 3
add r0, r0, r3
add r2, r2, r3
and rb_x_base_next, r0, ~3
and ra_x2_base_next, r2, ~3
mov ra_y_next, r1

# set up VPM write
mov vw_setup, rb28

# get width,height of block
mov r2, 16
mov r0, unif
shr r1, r0, r2 # Extract width
sub rb29, rb24, r1 # Compute vdw_setup1(dst_pitch-width)
and r0, r0, rb22 # Extract height
add rb17, r0, 5
add rb18, r0, 7
shl r0, r0, 7
# r0 is currently height<<7
# For vr_setup we want height<<20 (so 20-7=13 additional bits)
shl r3, r0, 13
shl r3, r3, 8 # Mask off top 8 bits
shr r3, r3, 8
add r0, r0, r1 # Combine width and height of destination area
shl r0, r0, r2 # Shift into bits 16 upwards of the vdw_setup0 register
add rb26, r0, rb27
# In a B frame, so also set up VPM read
add vr_setup, r3, rb28

# get filter coefficients

mov r0, unif
asr ra3, r0, rb23;      mul24 r0, r0, ra22
asr ra2, r0, rb23;      mul24 r0, r0, ra22
asr ra1, r0, rb23;      mul24 r0, r0, ra22
asr ra0, r0, rb23;      mov r0, unif
asr ra7, r0, rb23;      mul24 r0, r0, ra22
asr ra6, r0, rb23;      mul24 r0, r0, ra22
asr ra5, r0, rb23;      mul24 r0, r0, ra22
asr ra4, r0, rb23;      mov r0, unif
asr rb11, r0, rb23;     mul24 r0, r0, ra22
asr rb10, r0, rb23;     mul24 r0, r0, ra22
asr rb9, r0, rb23;      mul24 r0, r0, ra22
asr rb8, r0, rb23;      mov r0, unif
asr rb15, r0, rb23;     mul24 r0, r0, ra22
asr rb14, r0, rb23;     mul24 r0, r0, ra22
asr rb13, r0, rb23;     mul24 r0, r0, ra22
asr rb12, r0, rb23

# r2 is elem_num
# r3 is loop counter

mov r5rep, -8
mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]
mov r3, 0

:bloop
# retrieve texture results and pick out bytes
# then submit two more texture requests

sub.setf -, r3, rb17      ; v8adds r3, r3, ra20                     ; ldtmu0     # loop counter increment
shr r0, r4, ra_xshift     ; mov.ifz ra_x_base, rb_x_base_next       ; ldtmu0
mov.ifz ra_x2_base, ra_x2_base_next ; mov rb31, r3
mov.ifz ra_y, ra_y_next   ; mov r3, rb_pitch
shr r1, r4, ra_x2shift    ; v8subs r0, r0, rb20  # v8subs masks out all but bottom byte

max r2, ra_y, 0  # y
min r2, r2, rb_frame_height_minus_1
add ra_y, ra_y, 1         ; mul24 r2, r2, r3
add t0s, ra_x_base, r2    ; v8subs r1, r1, rb20
add t0s, ra_x2_base, r2

# generate seven shifted versions
# interleave with scroll of vertical context

mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

mov r2, rb21         ; mul24 r3, r0, ra0
sub r2, r2, r3       ; mul24      r3, ra1 << 1, r0 << 1
nop                  ; mul24.ifnz r3, ra1 << 9, r1 << 9
sub r2, r2, r3       ; mul24    r3, ra2 << 2, r0 << 2
nop                  ; mul24.ifnz r3, ra2 << 10, r1 << 10
sub r2, r2, r3       ; mul24    r3, ra3 << 3, r0 << 3
nop                  ; mul24.ifnz r3, ra3 << 11, r1 << 11
sub r2, r2, r3       ; mul24    r3, ra4 << 4, r0 << 4
nop                  ; mul24.ifnz r3, ra4 << 12, r1 << 12
sub r2, r2, r3       ; mul24    r3, ra5 << 5, r0 << 5
nop                  ; mul24.ifnz r3, ra5 << 13, r1 << 13
sub r2, r2, r3       ; mul24    r3, ra6 << 6, r0 << 6
nop                  ; mul24.ifnz r3, ra6 << 14, r1 << 14
sub r2, r2, r3       ; mul24    r3, ra7 << 7, r0 << 7
nop                  ; mul24.ifnz r3, ra7 << 15, r1 << 15
sub r0, r2, r3

mov r3, rb31

mov ra8, ra9
mov ra9, ra10
mov ra10, ra11
mov ra11, ra12
mov ra12, ra13
mov ra13, ra14

sub.setf -, r3, 8 ; mov r1, ra22

# apply horizontal filter
brr.anyn -, r:bloop
max ra14, ra15, 0       ; mul24 r0, r0, r1         # last bit of context scroll, including clamp to zero
asr r0, r0, 15          ; mov r1, ra21
min.setf ra15, r0, rb22

# apply vertical filter and write to VPM

nop                     ; mul24 r0, ra14, rb14
sub r1, r1, r0          ; mul24 r0, ra13, rb13
sub r1, r1, r0          ; mul24 r0, ra12, rb12
sub r1, r1, r0          ; mul24 r0, ra11, rb11
sub r1, r1, r0          ; mul24 r0, ra10, rb10
sub r1, r1, r0          ; mul24 r0, ra9, rb9
sub r1, r1, r0          ; mul24 r0, ra8, rb8
sub r1, r1, r0          ; mul24 r0, ra15, rb15
sub.ifnn r1, r1, r0     ; mov -, vw_wait
sub.setf -, r3, rb18    ; mul24 r1, r1, ra22
asr r1, r1, 15          ; mov -, vr_wait
min r1, r1, rb22
add r0, vpm, 1          # Blend in previous VPM contents at this location
brr.anyn -, r:bloop
max r1, r1, 0
add r1, r1, r0
shr vpm, r1, 1

# DMA out

bra -, ra31
mov vw_setup, rb26 # VDW: height rows, 16 8-bit units long
mov vw_setup, rb29
mov vw_addr, unif # start the VDW

################################################################################

# mc_filter_honly(next_kernel, x, y, frame_base, height, hcoeffs[0], hcoeffs[1], vcoeffs[0], vcoeffs[1], this_dst)
# This filter only does horizontal filtering.
# It is assumed that the region to fetch does not include extra rows above.

# At this point we have already issued two pairs of texture requests for the current block
# ra_x_base, ra_x16_base point to the current coordinates for this block
::mc_filter_honly
mov ra31, unif

# per-channel shifts were calculated on the *previous* invocation

mov ra_xshift, ra_xshift_next
mov ra_x2shift, ra_x2shift_next

# get base addresses and per-channel shifts for *next* invocation
add r0, unif, elem_num    # x
add r2, r0, 8 # x+8
max r0, r0, 0; mov r1, unif # y
min r0, r0, rb_frame_width_minus_1 ; mov r3,unif # frame_base
shl ra_xshift_next, r0, 3
max r2, r2, 0
min r2, r2, rb_frame_width_minus_1
shl ra_x2shift_next, r2, 3
add r0, r0, r3
add r2, r2, r3
and rb_x_base_next, r0, ~3
and ra_x2_base_next, r2, ~3
mov ra_y_next, r1

# set up VPM write
mov vw_setup, rb28

# get width,height of block
mov r2, 16
mov r0, unif
shr r1, r0, r2 # Extract width
sub rb29, rb24, r1 # Compute vdw_setup1(dst_pitch-width)
and r0, r0, rb22 # Extract height
add rb17, r0, -2 # Pipelining means we move data across 2 iterations early
shl r0, r0, 7 ; mov rb18,r0
add r0, r0, r1 # Combine width and height of destination area
shl r0, r0, r2 # Shift into bits 16 upwards of the vdw_setup0 register
add rb26, r0, rb27

# get filter coefficients

mov r0, unif
asr ra3, r0, rb23;      mul24 r0, r0, ra22
asr ra2, r0, rb23;      mul24 r0, r0, ra22
asr ra1, r0, rb23;      mul24 r0, r0, ra22
asr ra0, r0, rb23;      mov r0, unif
asr ra7, r0, rb23;      mul24 r0, r0, ra22
asr ra6, r0, rb23;      mul24 r0, r0, ra22
asr ra5, r0, rb23;      mul24 r0, r0, ra22
asr ra4, r0, rb23;      mov r0, unif
mov r0, unif

# r2 is elem_num
# r3 is loop counter
mov r5rep, -8
mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]  # delay slot 3
mov r3, 0

:loop_honly
# retrieve texture results and pick out bytes
# then submit two more texture requests

sub.setf -, r3, rb17      ; v8adds r3, r3, ra20                     ; ldtmu0     # loop counter increment
shr r0, r4, ra_xshift     ; mov.ifz ra_x_base, rb_x_base_next       ; ldtmu0
mov.ifz ra_x2_base, ra_x2_base_next ; mov rb31, r3
mov.ifz ra_y, ra_y_next   ; mov r3, rb_pitch
shr r1, r4, ra_x2shift    ; v8subs r0, r0, rb20  # v8subs masks out all but bottom byte

max r2, ra_y, 0  # y
min r2, r2, rb_frame_height_minus_1
add ra_y, ra_y, 1         ; mul24 r2, r2, r3
add t0s, ra_x_base, r2    ; v8subs r1, r1, rb20
add t0s, ra_x2_base, r2

# generate seven shifted versions
# interleave with scroll of vertical context

mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

mov r2, rb21         ; mul24 r3, r0, ra0
sub r2, r2, r3       ; mul24      r3, ra1 << 1, r0 << 1
nop                  ; mul24.ifnz r3, ra1 << 9, r1 << 9
sub r2, r2, r3       ; mul24    r3, ra2 << 2, r0 << 2
nop                  ; mul24.ifnz r3, ra2 << 10, r1 << 10
sub r2, r2, r3       ; mul24    r3, ra3 << 3, r0 << 3
nop                  ; mul24.ifnz r3, ra3 << 11, r1 << 11
sub r2, r2, r3       ; mul24    r3, ra4 << 4, r0 << 4
nop                  ; mul24.ifnz r3, ra4 << 12, r1 << 12
sub r2, r2, r3       ; mul24    r3, ra5 << 5, r0 << 5
nop                  ; mul24.ifnz r3, ra5 << 13, r1 << 13
sub r2, r2, r3       ; mul24    r3, ra6 << 6, r0 << 6
nop                  ; mul24.ifnz r3, ra6 << 14, r1 << 14
sub r2, r2, r3       ; mul24    r3, ra7 << 7, r0 << 7
nop                  ; mul24.ifnz r3, ra7 << 15, r1 << 15
sub r0, r2, r3       ; mov r3, rb31

sub.setf -, r3, rb18 ; mov r1, ra22

mov -, vw_wait   ; mul24 r0, r0, r1
brr.anyn -, r:loop_honly
asr r0, r0, 15          # delay 1
min r0, r0, rb22        # delay 2
max vpm, r0, 0          # delay 3

# DMA out
bra -, ra31
mov vw_setup, rb26 # VDW: height rows, 16 8-bit units long
mov vw_setup, rb29
mov vw_addr, unif # start the VDW


################################################################################

# mc_exit()

::mc_exit
mov  -, vw_wait # wait on the VDW

mov -,srel(0)

ldtmu0
ldtmu0
ldtmu0
ldtmu0

nop        ; nop ; thrend
nop        ; nop # delay slot 1
nop        ; nop # delay slot 2

::mc_exit1
mov  -, vw_wait # wait on the VDW

#mov -,srel(1)

ldtmu0
ldtmu0
ldtmu0
ldtmu0

nop        ; nop ; thrend
mov interrupt, 1; nop # delay slot 1
nop        ; nop # delay slot 2

# mc_interrupt_exit()
::mc_interrupt_exit
mov  -, vw_wait # wait on the VDW

ldtmu0
ldtmu0
ldtmu0
ldtmu0

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

# mc_interrupt_exit4()
::mc_interrupt_exit4
mov  -, vw_wait # wait on the VDW

ldtmu0
ldtmu0
ldtmu0
ldtmu0

mov -,sacq(0) # 1
mov -,sacq(0) # 2
mov -,sacq(0) # 3

nop        ; nop ; thrend
mov interrupt, 1; nop # delay slot 1
nop        ; nop # delay slot 2

# mc_interrupt_exit8()
::mc_interrupt_exit8
mov  -, vw_wait # wait on the VDW

ldtmu0
ldtmu0
ldtmu0
ldtmu0

mov -,sacq(0) # 1
mov -,sacq(0) # 2
mov -,sacq(0) # 3
mov -,sacq(0) # 4
mov -,sacq(0) # 5
mov -,sacq(0) # 6
mov -,sacq(0) # 7

nop        ; nop ; thrend
mov interrupt, 1; nop # delay slot 1
nop        ; nop # delay slot 2

################################################################################
# mc_setup_uv(next_kernel, x, y, ref_u_base, ref_v_base, frame_width, frame_height, pitch, dst_pitch, pad0, pad1, pad2)
::mc_setup_uv

# Read starting kernel
mov ra31, unif

# Load first request location
add ra_x_base, unif, elem_num # Store x
mov ra_y, unif # Store y
mov ra_x2_base, unif # Store frame u base
nop
sub ra_u2v_ref_offset, unif, ra_x2_base # Store offset to add to move from u to v in reference frame

# Read image dimensions
sub rb25,unif,1
sub rb30,unif,1

# get source pitch
mov rb16, unif

# get destination pitch
mov r0, unif
mov r1, vdw_setup_1(0)
add rb24, r1, r0

# load constants

mov ra20, 1
mov ra21, 64
mov ra22, 256
mov ra23, 8

mov rb20, 0xffffff00
mov rb21, 64
mov rb22, 255
mov rb23, 24

# touch vertical context to keep simulator happy

mov ra8, 0
mov ra9, 0
mov ra10, 0
mov ra11, 0
mov ra12, 0
mov ra13, 0
mov ra14, 0
mov ra15, 0

# Compute part of VPM to use for DMA output
mov r2, qpu_num
and r2, r2, 15
mov r1, r2
asr r1, r1, 2
shl r1, r1, 6
mov r0, r2
and r0, r0, 3
add r0, r0, r1
mov r1, vdw_setup_0(0, 0, dma_h8p(0,0,0)) # height,width added later
shl r0, r0, 5
add rb27, r0, r1

# Compute part of VPM to save data into
mov r2, qpu_num
and r2, r2, 15
mov r1, r2
asr r1, r1, 2
shl r1, r1, 6
mov r0, r2
and r0, r0, 3
add r0, r0, r1
mov r1, vpm_setup(0, 4, h8p(0, 0))
add rb28, r0, r1

# Compute base address for first and second access
mov r0, ra_x_base           # Load x
max r0, r0, 0; mov r1, ra_y # Load y
min r0, r0, rb_frame_width_minus_1 ; mov r3, ra_x2_base  # Load the frame base
shl ra_xshift_next, r0, 3 ; mov r2, ra_u2v_ref_offset
add ra_y, r1, 1
add r0, r0, r3
and r0, r0, ~3
max r1, r1, 0 ; mov ra_x_base, r0 # y
min r1, r1, rb_frame_height_minus_1
# submit texture requests for first line
add r2, r2, r0 ; mul24 r1, r1, rb_pitch
add t0s, r0, r1 ; mov ra_x2_base, r2
add t0s, r2, r1

# Dump padding words
mov r0, unif
mov r0, unif
mov r0, unif

# submit texture requests for second line
max r1, ra_y, 0
min r1, r1, rb_frame_height_minus_1
add ra_y, ra_y, 1
bra -, ra31
nop ; mul24 r1, r1, rb_pitch
add t0s, r1, ra_x_base
add t0s, r1, ra_x2_base



################################################################################

::mc_filter_uv_b
mov ra31, unif

# per-channel shifts were calculated on the *previous* invocation

mov ra_xshift, ra_xshift_next

# get base addresses and per-channel shifts for *next* invocation
add r0, unif, elem_num    # x
max r0, r0, 0; mov r1, unif # y
min r0, r0, rb_frame_width_minus_1 ; mov r3, unif # frame_base
shl ra_xshift_next, r0, 3
sub r2, unif, r3 # compute offset from frame base u to frame base v
add r0, r0, r3
and rb_x_base_next, r0, ~3
mov ra_y_next, r1
add ra_x2_base_next, rb_x_base_next, r2

# set up VPM write
mov vw_setup, rb28

# get width,height of block
mov r2, 16
mov r0, unif
shr r1, r0, r2 # Extract width
sub rb29, rb24, r1 # Compute vdw_setup1(dst_pitch-width)
and r0, r0, rb22 # Extract height
add rb17, r0, 5
add rb18, r0, 7
shl r0, r0, 7

# r0 is currently height<<7
# For vr_setup we want height<<20 (so 20-7=13 additional bits)
shl r3, r0, 13
shl r3, r3, 8 # Mask off top 8 bits
shr r3, r3, 8

add r0, r0, r1 # Combine width and height of destination area
shl r0, r0, r2 # Shift into bits 16 upwards of the vdw_setup0 register
add rb26, r0, rb27

# In a B frame, so also set up VPM read
add vr_setup, r3, rb28

sub.setf -,8,r1 # 8-r1, so if <0 (negative) we need to use the full code

# get filter coefficients

mov r0, unif
asr ra3, r0, rb23;      mul24 r0, r0, ra22
asr ra2, r0, rb23;      mul24 r0, r0, ra22
asr ra1, r0, rb23;      mul24 r0, r0, ra22
asr ra0, r0, rb23;      mov r0, unif
asr ra7, r0, rb23;      mul24 r0, r0, ra22
asr ra6, r0, rb23;      mul24 r0, r0, ra22
asr ra5, r0, rb23;      mul24 r0, r0, ra22
asr ra4, r0, rb23;      mov r0, unif
asr rb11, r0, rb23;     mul24 r0, r0, ra22
asr rb10, r0, rb23;     mul24 r0, r0, ra22
asr rb9, r0, rb23;      mul24 r0, r0, ra22
asr rb8, r0, rb23;      mov r0, unif
asr rb15, r0, rb23;     mul24 r0, r0, ra22
asr rb14, r0, rb23;     mul24 r0, r0, ra22
asr rb13, r0, rb23;     mul24 r0, r0, ra22
asr rb12, r0, rb23

# r2 is elem_num
# r3 is loop counter

mov r5rep, -8
mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

# retrieve texture results and pick out bytes
# then submit two more texture requests

mov r3, 0

:uvloop_b
# retrieve texture results and pick out bytes
# then submit two more texture requests

sub.setf -, r3, rb17      ; v8adds r3, r3, ra20                     ; ldtmu0     # loop counter increment
shr r0, r4, ra_xshift     ; mov.ifz ra_x_base, rb_x_base_next       ; ldtmu0
mov.ifz ra_x2_base, ra_x2_base_next ; mov rb31, r3
mov.ifz ra_y, ra_y_next   ; mov r3, rb_pitch
shr r1, r4, ra_xshift    ; v8subs r0, r0, rb20  # v8subs masks out all but bottom byte

max r2, ra_y, 0  # y
min r2, r2, rb_frame_height_minus_1
add ra_y, ra_y, 1         ; mul24 r2, r2, r3
add t0s, ra_x_base, r2    ; v8subs r1, r1, rb20
add t0s, ra_x2_base, r2

# generate seven shifted versions
# interleave with scroll of vertical context

mov.setf -, [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1]

mov r2, rb21         ; mul24 r3, r0, ra0
nop                  ; mul24.ifnz r3, ra0 << 8, r1 << 8
sub r2, r2, r3       ; mul24      r3, ra1 << 1, r0 << 1
nop                  ; mul24.ifnz r3, ra1 << 9, r1 << 9
sub r2, r2, r3       ; mul24    r3, ra2 << 2, r0 << 2
nop                  ; mul24.ifnz r3, ra2 << 10, r1 << 10
sub r2, r2, r3       ; mul24    r3, ra3 << 3, r0 << 3
nop                  ; mul24.ifnz r3, ra3 << 11, r1 << 11
sub r2, r2, r3       ; mul24    r3, ra4 << 4, r0 << 4
nop                  ; mul24.ifnz r3, ra4 << 12, r1 << 12
sub r2, r2, r3       ; mul24    r3, ra5 << 5, r0 << 5
nop                  ; mul24.ifnz r3, ra5 << 13, r1 << 13
sub r2, r2, r3       ; mul24    r3, ra6 << 6, r0 << 6
nop                  ; mul24.ifnz r3, ra6 << 14, r1 << 14
sub r2, r2, r3       ; mul24    r3, ra7 << 7, r0 << 7
nop                  ; mul24.ifnz r3, ra7 << 15, r1 << 15
sub r0, r2, r3

mov r3, rb31

mov ra8, ra9
mov ra9, ra10
mov ra10, ra11
mov ra11, ra12
mov ra12, ra13
mov ra13, ra14

sub.setf -, r3, 8 ; mov r1, ra22

# apply horizontal filter
brr.anyn -, r:uvloop_b
max ra14, ra15, 0       ; mul24 r0, r0, r1         # last bit of context scroll, including clamp to zero
asr r0, r0, 15          ; mov r1, ra21
min.setf ra15, r0, rb22

# apply vertical filter and write to VPM

nop                     ; mul24 r0, ra14, rb14
sub r1, r1, r0          ; mul24 r0, ra13, rb13
sub r1, r1, r0          ; mul24 r0, ra12, rb12
sub r1, r1, r0          ; mul24 r0, ra11, rb11
sub r1, r1, r0          ; mul24 r0, ra10, rb10
sub r1, r1, r0          ; mul24 r0, ra9, rb9
sub r1, r1, r0          ; mul24 r0, ra8, rb8
sub r1, r1, r0          ; mul24 r0, ra15, rb15
sub.ifnn r1, r1, r0     ; mov -, vw_wait
sub.setf -, r3, rb18    ; mul24 r1, r1, ra22
asr r1, r1, 15
min r1, r1, rb22
add r0, vpm, 1          # Blend in previous VPM contents at this location
brr.anyn -, r:uvloop_b
max r1, r1, 0
add r1, r1, r0
shr vpm, r1, 1


# DMA out for U

mov vw_setup, rb26 # VDW setup 0
mov vw_setup, rb29 # Stride
mov vw_addr, unif # start the VDW

# DMA out for V
# We need to wait for the U to complete first, but have nothing useful to compute while we wait.
# Could potentially push this write into the start of the next pipeline stage.
mov r0, 16
mov -, vw_wait

bra -, ra31
add vw_setup, rb26, r0 # VDW setup 0
mov vw_setup, rb29 # Stride
mov vw_addr, unif # start the VDW

::mc_end
