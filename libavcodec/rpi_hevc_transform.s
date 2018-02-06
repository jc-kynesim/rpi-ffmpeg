# ******************************************************************************
# Argon Design Ltd.
# (c) Copyright 2015 Argon Design Ltd. All rights reserved.
#
# Module : HEVC
# Author : Peter de Rivaz
# ******************************************************************************

# Lines that fail to assemble start with #:
# The script insert_magic_opcodes.sh inserts the machine code directly for these.

# HEVC VPU Transform
#
# Transform matrix can be thought of as
#   output row vector = input row vector * transMatrix2
#
# The even rows of the matrix are symmetric
# The odd rows of the matrix are antisymmetric
#
# So only need to compute the first half of the results, then can compute the remainder with a butterfly
#
# EXAMPLE
#   (a b c d) (1 2  2  1)
#             (3 4 -4 -3)
#             (5 6  6  5)
#             (7 8 -8 -7)
#
#  x=(a c)(1 2) = 1a+5c 2a+6c
#         (5 6)
#
#  y=(b d)(3 4) = 3b+7d 4b+8d
#         (7 8)
#
#  u=x+y = 1a+5c+3b+7d 2a+4b+6c+8d
#  v=x-y = 1a+5c-3b-7d 2a+6c-4b-8d
#
#  Final results are (u , v[::-1])
#
#
#  For 32x1 input, load even rows into HX(0++,0), odd rows into HX(16++,0)
#  Apply the even matrix first and stop before rounding
#  Then apply the odd matrix in a full manner:
#
#   First step is to compute partial products with the first input (16 cycles)
#   1a 3b 5c 7d   16x1 input coefficients produce 16x16 output
#   2a 4b 6c 8d
#   2a -4b 6c -8d
#   1a -3b 5c -7d
#
#   Second step is to sum partial products into final position (8 cycles)
#   1a+3b+5c+7d
#   2a+4b+6c+8d
#   2a-4b+6c-8d
#   1a-3b+5c-7d
#
#   Then can apply butterfly to combine even results and odd results + rounding to produce 16 rows of output at a time (need to save in transposed format)
#
#   For 16x16 no butterfly is required and can store final results in original location  (Could do 2 16x16s in parallel to make use of the trick - saves on the adds)
#
#   For 8x8 we could compute two in parallel.
#
#

# Columns are transformed first
#
# Store top left half of transMatrix2 in
# Store bottom left half of transMatrix2 in HX(32,32)
#
# For 16x16
# HX(0:15,0) contains input data before transform
# HY(0:15,0) contains 32bit output data after transform
# HX(32,0) contains even rows of left half of transMatrix2
# HX(32,32) contains odd rows of left half of transMatrix2
# HY(48,0) contains partial products ready for summing
#


# hevc_trans_16x16(short *transMatrix2, short *coeffs, int num) # TODO add size so we can branch to correct implementation (or perhaps have coeffs32 and num32 as secondary inputs!)
# transMatrix2: address of the constant matrix (must be at 32 byte aligned address in Videocore memory)
# coeffs: address of the transform coefficients (must be at 32 byte aligned address in Videocore memory)
# num: number of 16x16 transforms to be done
# coeffs32
# num32: number of 32x32 transforms
# command 0 for transform, 1 for memclear16(int16_t *dst,num16)
#

.equ TRANS_SHIFT, 20 - BIT_DEPTH
.equ TRANS_RND2, 1 << (TRANS_SHIFT - 1)
.equ TRANS_ASL2, 16 - TRANS_SHIFT


hevc_trans_16x16:
  cmp r5,1
  beq memclear16
  #cmp r5,2
  #beq hevc_deblock_16x16
  #cmp r5,3
  #beq hevc_uv_deblock_16x16
  #cmp r5,4
  #beq hevc_uv_deblock_16x16_with_clear
  cmp r5,5
  beq hevc_run_command_list
  cmp r5,6
  beq sao_process_row
  cmp r5,7
  beq hevc_uv_deblock_16x16_striped
  cmp r5,8
  beq hevc_residual
  b do_transform

  .balign 32
packed_buffer:
  .space 16*2
intermediate_results:
  .space 32*32*2
unpacked_buffer:
  .space 32*32*2

packed_buffer2:
  .space 16*2
intermediate_results2:
  .space 32*32*2
unpacked_buffer2:
  .space 32*32*2
vpu_bank:
  .word 0


do_transform:
  push r6-r15, lr # TODO cut down number of used registers


  mov r14,r3 # coeffs32
  mov r15,r4 # num32
  mov r3, 16*2 # Stride of transMatrix2 in bytes
  vldh HX(32++,0),(r0 += r3) REP 16 # This is the 16x16 matrix, a transform is equivalent to multiplying input row vector * matrix

  add r0, 16*16*2 # For 32x32 transforms we also need this matrix
  vldh HX(32++,32),(r0 += r3) REP 16 # This is the odd 16x16 matrix

  # Now use r0 to describe which matrix we are working on.
  # Allows us to prefetch the next block of coefficients for efficiency.
  mov r0,0 # This describes the location where we read our coefficients from
  mov r3,16*2 # Stride of coefficients in bytes (TODO remove)
  mov r7,16*16*2 # Total block size
  mov r8,64*16 # Value used to swap from current to next VRF location
  mov r4,64 # Constant used for rounding first pass
  mov r5,TRANS_RND2 # Constant used for rounding second pass

  sub sp,sp,64+16*16*2 # Move on stack pointer in case interrupt occurs and uses stack

  add r11,sp,64 # Space for 32 bytes before, and rounding
  lsr r11,5
  lsl r11,5 # Make sure r11 is rounded to multiple of 2**5==32

  lsr r10, r2, 16 # Number of compressed blocks stored in top short
#:and r2,r2,0xffff
  .half 0x6f02 #AUTOINSERTED


  # At start of block r0,r1 point to the current block (that has already been loaded)

  # r0 VRF location of current block
  # r1 address of current block
  # r2 number of 16*16 transforms to do
  # r3 Stride of coefficients (==32)
  # r4 TRANS_RND1 (64)
  # r5 TRANS_RND2
  # r6 temporary used inside col_trans16
  # r7 16*16*2 total bytes in block
  # r8 64*16 VRF switch locations
  # r9 temporary in unpack_coeff for index
  # r10 number of 16x16 transforms using compression
  # r11 unpacked data buffer (16*16 shorts) (preceded by 16 shorts of packed data buffer)
  # r12 temporary counter in unpack_coeff
  # r13
  # r14 Save information for 32 bit transform (coeffs location)
  # r15 Save information for 32 bit transform (number of transforms)
block_loop:
  # With compressed coefficients, we don't use prefetch as we don't want to issue unnecessary memory requests
  cmp r10,0
  mov r6, r1
  beq not_compressed
  sub r10, 1
  bl unpack16x16
not_compressed:
  #mov r6,r1 # DEBUG without compress
  vldh HX(0++,0)+r0,(r6 += r3) REP 16

  #eor r0,r8
  #add r1,r7
  # Prefetch the next block
  #bl unpack16x16
  #vldh HX(0++,0)+r0,(r6 += r3) REP 16
  #vmov HX(0++,0)+r0,0 REP 16  # DEBUG
  #eor r0,r8
  #sub r1,r7

  # Transform the current block
  bl col_trans_16
  vadd HY(0++,0)+r0,HY(0++,0)+r0,r4 REP 16   # Now add on rounding, shift down by 7, and saturate
  #vsasls HY(0++,0)+r0,HY(0++,0)+r0,9 REP 16 # 9+7=16 so this ends up with the output saturated and in the top half of the word.
  vasl HY(0++,0)+r0,HY(0++,0)+r0,9 REP 16    # This should be saturating, but the instruction above does not assemble?
  vmov VX(0,0++)+r0, HX(0++,32)+r0 REP 16    # For simplicity transpose this back to the original position

  bl col_trans_16
  vadd HY(0++,0)+r0,HY(0++,0)+r0,r5 REP 16   # Now add on rounding, shift down by 7, and saturate
  #vsasls HY(0++,0)+r0,HY(0++,0)+r0,4 REP 16 # 4+12=16 so this ends up with the output saturated and in the top half of the word.
  vasl HY(0++,0)+r0,HY(0++,0)+r0,TRANS_ASL2 REP 16    # This should be saturating, but the instruction above does not assemble?  (Probably because it ends with ls which is interpreted as a condition flag)

  # Save results - note there has been a transposition during the processing so we save columns
  vsth VX(0,32++)+r0, (r1 += r3) REP 16

  # Move onto next block
  eor r0,r8
  add r1,r7

  addcmpbgt r2,-1,0,block_loop

  add sp,sp,64+16*16*2 # Move on stack pointer in case interrupt occurs and uses stack

  # Now go and do any 32x32 transforms
  b hevc_trans_32x32

  pop r6-r15, pc

# This returns a value in r6 that says where to load the data from.
# We load data 16 shorts at a time from memory (uncached), and store to stack space to allow us to process it.
unpack16x16:
# Clear out destination
  vmov HX(0,0)+r0,0
  mov r6, r11
  vsth HX(0,0)+r0,(r6 += r3) REP 16
  mov r5, r1 # Moving pointer to input coefficients
unpack_outer_loop:
  # Loop until we find the end
  vldh HX(0,0)+r0,(r5)  # TODO would prefetch help here while unpacking previous?
  sub r6,r11,32
  #add r6,pc,packed_data-$ # Packed data
  vsth HX(0,0)+r0,(r6)  # Store into packed data
  mov r12,0
unpack_loop:
  ld r4,(r6)
  add r6,r6,4
  lsr r9,r4,16 # r9 is destination value
  cmp r4,0 # {value,index}
#:and r4,r4,0xff
  .half 0x6e84 #AUTOINSERTED
  beq done_unpack
  sth r9,(r11, r4)
  addcmpblt r12,1,8,unpack_loop
#  # Read next 16
  add r5,32
  b unpack_outer_loop
done_unpack:
#  # Set new load location
  mov r6, r11
  #add r6,pc,unpacked_data-$
#  # Restore constants
  mov r4,64
  mov r5,TRANS_RND2

#  pop r6-r15, pc
  b lr

# r1,r2,r3 r7,r8 should be preserved
# HX(0++,0)+r0 is the block to be transformed
# HX(32++,0)+r6 is the 16x16 matrix of transform coefficients
# Use HY(48,0) for intermediate results
# r0 can be used, but should be returned to its original value at the end
col_trans_16:
  add r6,r0,16 # Final value for this loop
col_trans_16_loop:
  # First compute partial products for a single column
  vmul32s HY(48++,0), VX(0,0)+r0, VX(32,0++) REP 16
  # Then sum up the results and place back
  vadd VY(0,0)+r0, VY(48,0++), VY(48,8++) REP 8 CLRA SACC
  addcmpblt r0,1,r6,col_trans_16_loop
  sub r0,16  # put r0 back to its original value
  b lr

col_trans_odd_16:
  add r6,r0,16 # Final value for this loop
col_trans_odd_16_loop:
  # First compute partial products for a single column
  vmul32s HY(48++,0), VX(0,0)+r0, VX(32,32++) REP 16
  # Then sum up the results and place back
  vadd VY(0,0)+r0, VY(48,0++), VY(48,8++) REP 8 CLRA SACC
  addcmpblt r0,1,r6,col_trans_odd_16_loop
  sub r0,16  # put r0 back to its original value
  b lr

# r1/r10 input pointer
# r0,r4,r5,r6 free
# r8/r9 output storage
#
# Store packed coefficients at r9-32
# Store unpacked at r9+32*32 (because transform works on even/odd rows on input, but writes all rows)

unpack32x32:
# Clear out destination
  vmov HX(0,0),0
  add r0, r9, 32*32*2 # Unpacked buffer
  mov r4, 32
  vsth HX(0,0),(r0 += r4) REP 64
unpack_outer_loop32:
  # Loop until we find the end
  vldh HX(0,0),(r1)  # TODO would prefetch help here while unpacking previous?
  sub r6,r9,32
  #add r6,pc,packed_data-$ # Packed data
  vsth HX(0,0),(r6)  # Store into packed data
  mov r8,0
unpack_loop32:
  ld r4,(r6)
  add r6,r6,4
  lsr r5,r4,16 # r5 is destination value
  cmp r4,0 # {value,index}
#:and r4,r4,0x3ff
  .half 0x6ea4 #AUTOINSERTED
  beq done_unpack
  sth r5,(r0, r4)
  addcmpblt r8,1,8,unpack_loop32
#  # Read next 16
  add r1,32
  b unpack_outer_loop32
done_unpack32:
  b lr

# hevc_trans_32x32(short *transMatrix2, short *coeffs, int num)
# transMatrix2: address of the constant matrix (must be at 32 byte aligned address in Videocore memory) Even followed by odd
# coeffs: address of the transform coefficients (must be at 32 byte aligned address in Videocore memory)
# num: number of 16x16 transforms to be done in low 16, number of packed in high 16
#
# Note that the 32x32 transforms are stored in reverse order, this means that the unpacked ones appear first!
hevc_trans_32x32:
  mov r1,r14 # coeffs
  mov r2,r15 # num
  lsr r15,r15,16 # Number that are packed
#:and r2,r2,0xffff # Total number
  .half 0x6f02 #AUTOINSERTED

  # Fetch odd transform matrix
  #mov r3, 16*2 # Stride of transMatrix2 in bytes (and of coefficients)
  #vldh HX(32++,0),(r0 += r3) REP 16 # This is the even 16x16 matrix
  #add r0, 16*16*2
  #vldh HX(32++,32),(r0 += r3) REP 16 # This is the odd 16x16 matrix

  mov r3, 32*2*2 # Stride used to fetch alternate rows of our input coefficient buffer
  mov r7, 16*16*2 # Total block size
  #sub sp,sp,32*32*2+64 # Allocate some space on the stack for us to store 32*32 shorts as temporary results (needs to be aligned) and another 32*32 for unpacking
  # set r8 to 32byte aligned stack pointer with 32 bytes of space before it
  #add r8,sp,63
  #lsr r8,5
  #lsl r8,5

#:di
  .half 0x0005 #AUTOINSERTED
#try_again: 
#  add r9,pc,vpu_bank-$
#  ld r8,(r9)
#  add r8,1
#  st r8,(r9)
#  ld r9,(r9)
#  cmp r8,r9
#  bne try_again

#:version r8
  .half 0x00e8 #AUTOINSERTED
  lsr r8,r8,16
#:btest r8,1
  .half 0x6c18 #AUTOINSERTED
  add r8,pc,intermediate_results-$
  beq on_vpu1
  add r8,r8,32*32*2*2+16*2 # Move to secondary storage
on_vpu1:
  mov r9,r8  # Backup of the temporary storage
  mov r10,r1 # Backup of the coefficient buffer
block_loop32:

  # Transform the first 16 columns
  mov r1,r10  # Input Coefficient buffer
  mov r8,r9   # Output temporary storage

  # Unpacked are first, so need to only do unpacking when r2(=num left) <= r15 (=num packed)
  cmp r2,r15
  bgt not_compressed_32
  bl unpack32x32
  add r1,r9,32*32*2   # Uncompressed into temporary storage
  mov r8,r9           # Transform into here
not_compressed_32:

  # COLUMN TRANSFORM
  mov r4, 64 # Constant used for rounding first pass
  mov r5, 9 # left shift used for rounding first pass

  bl trans32
  # Transform the second 16 columns
  add r8,32*16*2
  add r1,32
  bl trans32

  # ROW TRANSFORM
  mov r4, TRANS_RND2 # Constant used for rounding second pass
  mov r5, TRANS_ASL2 # left shift used for rounding second pass

  mov r1,r9  # Input temporary storage
  mov r8,r10   # Output Coefficient buffer
  bl trans32
  # Transform the second 16 columns
  add r8,32*16*2
  add r1,32
  bl trans32

### :version r1
#  st r1,(r10)

  add r10, 32*32*2 # move onto next block of coefficients
  addcmpbgt r2,-1,0,block_loop32

  #add sp,sp,32*32*2+64# Restore stack

#:ei
  .half 0x0004 #AUTOINSERTED

  pop r6-r15, pc

trans32:
  push lr
  # We can no longer afford the VRF space to do prefetching when doing 32x32
  # Fetch the even rows
  vldh HX(0++,0),(r1 += r3) REP 16
  #vmov HX(0++,0)+r0,0 REP 16 # DEBUG
  # Fetch the odd rows
  vldh HX(16++,0),64(r1 += r3) REP 16 # First odd row is 32 shorts ahead of r1
  #vmov HX(16++,0)+r0,0 REP 16 # DEBUG

  # Transform the even rows using even matrix
  mov r0, 0 # Even rows
  bl col_trans_16

  # Now transform the odd rows using odd matrix
  mov r0, 64*16 # Odd rows
  bl col_trans_odd_16

  # Now apply butterfly to compute the first 16 results
  vadd HY(48++,0),HY(0++,0),HY(16++,0) REP 16
  vadd HY(48++,0),HY(48++,0),r4 REP 16   # add on rounding,
  vasl HY(48++,0),HY(48++,0),r5 REP 16    # shift down by 7, and saturate
  # 16bit results now in HX(48,32)
  mov r0,r8
  mov r6,32*2
  vsth VX(48,32++),(r0+=r6) REP 16

  # Now apply butterfly to compute the second 16 results (in reverse order)
  vsub HY(63,0),HY(0 ,0),HY(16,0)
  vsub HY(62,0),HY(1 ,0),HY(17,0)
  vsub HY(61,0),HY(2 ,0),HY(18,0)
  vsub HY(60,0),HY(3 ,0),HY(19,0)
  vsub HY(59,0),HY(4 ,0),HY(20,0)
  vsub HY(58,0),HY(5 ,0),HY(21,0)
  vsub HY(57,0),HY(6 ,0),HY(22,0)
  vsub HY(56,0),HY(7 ,0),HY(23,0)
  vsub HY(55,0),HY(8 ,0),HY(24,0)
  vsub HY(54,0),HY(9 ,0),HY(25,0)
  vsub HY(53,0),HY(10,0),HY(26,0)
  vsub HY(52,0),HY(11,0),HY(27,0)
  vsub HY(51,0),HY(12,0),HY(28,0)
  vsub HY(50,0),HY(13,0),HY(29,0)
  vsub HY(49,0),HY(14,0),HY(30,0)
  vsub HY(48,0),HY(15,0),HY(31,0)
  vadd HY(48++,0),HY(48++,0),r4 REP 16   # add on rounding,
  vasl HY(48++,0),HY(48++,0),r5 REP 16    # shift down by 7, and saturate
  add r0,r8,32
  vsth VX(48,32++),(r0+=r6) REP 16
  pop pc

memclear16:
  # r0 is address
  # r1 is number of 16bits values to set to 0 (may overrun past end and clear more than specified)
  vmov HX(0++,0),0 REP 16
  mov r2,32
loop:
  vsth HX(0++,0),(r0+=r2) REP 16
  add r0,16*16*2
  sub r1,16*16
  cmp r1,0
  bgt loop
  b lr


################################################################################
# HEVC VPU Deblock
#
# Vertical edges before horizontal
# Decision can change every 4 pixels, but only 8 pixel boundaries are deblocked
#
# ARM is responsible for storing beta and tc for each 4 pixels horiz and vert edge.
# The VPU code works in units of 16x16 blocks.
# We do vertical filtering for the current block followed by horizontal filtering for the previous (except for the first time).
# One final horizontal filter is required at the end.
# PCM is not allowed in this code.
#
#
# H(16-4:16+15,0) contains previous block (note that we need 4 lines above of context that may get altered during filtering)
# H(16:31,16) contains current block (note that we do not need the upper lines until the horizontal filtering.

.set P0,63
.set P1,62
.set P2,61
.set P3,60
.set Q0,59
.set Q1,58
.set Q2,57
.set Q3,56

.set dp,32
.set dq,33
.set d,34
.set decision,35
.set beta,36
.set beta2,37
.set beta3,38
.set ptest,39
.set qtest,40
.set pqtest,41
.set thresh,42
.set deltatest, 44
.set deltap1, 45
.set tc25, 46
.set setup,47
.set tc,48
.set tc25,49
.set tc2, 50
.set do_filter, 51
.set delta, 52
.set tc10, 53
.set delta0, 54
.set delta1, 55
.set setup_old, 56
.set maxvalue, 57
.set zeros, 0
.set setup_input, 1
.set deltaq1, 2



# hevc_deblock_16x16 deblocks an entire row that is 16 pixels high by the full width of the image.
# Row has num16 16x16 blocks across
# Beta goes from 0 to 64
# tc goes from 0 to 24
# setup[block_idx][0=vert,1=horz][0=first edge, 1=second edge][0=beta,1=tc][0..3=edge number]
#   has 8 bytes per edge
#   has 16 bytes per direction
#   has 32 bytes per 16x16 block
# hevc_deblock_16x16(uint8_t *img (r0), int stride (r1), int num16w (r2), uint8_t setup[num16][2][2][2][4](r3),int num16h(r4))
hevc_deblock_16x16:
  push r6-r15, lr
  mov r9,r4
  mov r4,r3
  mov r13,r2
  mov r2,r0
  mov r10,r0
  subscale4 r0,r1
  mov r8,63
  mov r6,-3
  vmov H(zeros,0),0
# r7 is number of blocks still to load
# r0 is location of current block - 4 * stride
# r1 is stride
# r2 is location of current block
# r3 is offset of start of block (actual edges start at H(16,16)+r3 for horizontal and H(16,0)+r3 for vertical
# r4 is setup
# r5 is for temporary calculations
# r8 holds 63
# r6 holds -3
# r9 holds the number of 16 high rows to process
# r10 holds the original img base
# r11 returns 0 if no filtering was done on the edge
# r12 saves a copy of this
# r13 is copy of width

process_row:
  # First iteration does not do horizontal filtering on previous
  mov r7, r13
  mov r3,0
  vldb H(12++,16)+r3,(r0 += r1) REP 4    # Load the current block
  vldb H(16++,16)+r3,(r2 += r1) REP 16
  vldb H(setup_input,0), (r4)  # We may wish to prefetch these
  vstb H(zeros,0),(r4)
  bl vert_filter
  add r3,8
  vadd H(setup_input,0),H(setup_input,8),0 # Rotate to second set of 8
  bl vert_filter
  sub r3,8
  b start_deblock_loop
deblock_loop:
  # Middle iterations do vertical on current block and horizontal on preceding
  vldb H(12++,16)+r3,(r0 += r1) REP 4  # load the current block
  vldb H(16++,16)+r3,(r2 += r1) REP 16
  vldb H(setup_input,0), (r4)
  vstb H(zeros,0),(r4)
  bl vert_filter
  add r3,8
  vadd H(setup_input,0),H(setup_input,8),0
  bl vert_filter
  sub r3,8
  vldb H(setup_input,0), -16(r4)
  vstb H(zeros,0),-16(r4)
  bl horz_filter
  mov r12,r11
  add r3,8*64
  vadd H(setup_input,0),H(setup_input,8),0
  bl horz_filter
  sub r3,8*64
  addcmpbeq r12,0,0,skip_save_top
  vstb H(12++,0)+r3,-16(r0 += r1) REP 4  # Save the deblocked pixels for the previous block
skip_save_top:
  vstb H(16++,0)+r3,-16(r2 += r1) REP 16
start_deblock_loop:
  # move onto next 16x16 (could do this with circular buffer support instead)
  add r3,16
  and r3,r8
  add r4,32
  # Perform loop counter operations (may work with an addcmpbgt as well?)
  add r0,16
  add r2,16
  sub r7,1
  cmp r7,0 # Are there still more blocks to load
  bgt deblock_loop

  # Final iteration needs to just do horizontal filtering
  vldb H(setup_input,0), -16(r4)
  vstb H(zeros,0),-16(r4)
  bl horz_filter
  mov r12,r11
  add r3,8*64
  vadd H(setup_input,0),H(setup_input,8),0
  bl horz_filter
  sub r3,64*8
  addcmpbeq r12,0,0,skip_save_top2
  vstb H(12++,0)+r3,-16(r0 += r1) REP 4  # Save the deblocked pixels for the previous block
skip_save_top2:
  vstb H(16++,0)+r3,-16(r2 += r1) REP 16

# Now look to see if we should do another row
  sub r9,1
  cmp r9,0
  bgt start_again
  pop r6-r15, pc
start_again:
  # Need to sort out r0,r2 to point to next row down
  addscale16 r10,r1
  mov r2,r10
  subscale4 r0,r2,r1
  b process_row


# At this stage H(16,16)+r3 points to the first pixel of the 16 high edge to be filtered
# So we can reuse the code we move the parts to be filtered into HX(P0/P1/P2/P3/Q0/Q1/Q2/Q3,0) - we will perform a final saturation step on placing them back into the correct locations

vert_filter:
  push lr

  vmov HX(P3,0), V(16,12)+r3
  vmov HX(P2,0), V(16,13)+r3
  vmov HX(P1,0), V(16,14)+r3
  vmov HX(P0,0), V(16,15)+r3
  vmov HX(Q0,0), V(16,16)+r3
  vmov HX(Q1,0), V(16,17)+r3
  vmov HX(Q2,0), V(16,18)+r3
  vmov HX(Q3,0), V(16,19)+r3

  bl do_luma_filter

  vadds V(16,13)+r3, HX(P2,0), 0
  vadds V(16,14)+r3, HX(P1,0), 0
  vadds V(16,15)+r3, HX(P0,0), 0
  # P3 and Q3 never change so don't bother saving back
  vadds V(16,16)+r3, HX(Q0,0), 0
  vadds V(16,17)+r3, HX(Q1,0), 0
  vadds V(16,18)+r3, HX(Q2,0), 0

  pop pc

# Filter edge at H(16,0)+r3
horz_filter:
  push lr

  vmov HX(P3,0), H(12,0)+r3
  vmov HX(P2,0), H(13,0)+r3
  vmov HX(P1,0), H(14,0)+r3
  vmov HX(P0,0), H(15,0)+r3
  vmov HX(Q0,0), H(16,0)+r3
  vmov HX(Q1,0), H(17,0)+r3
  vmov HX(Q2,0), H(18,0)+r3
  vmov HX(Q3,0), H(19,0)+r3

  bl do_luma_filter

  vadds H(13,0)+r3, HX(P2,0), 0
  vadds H(14,0)+r3, HX(P1,0), 0
  vadds H(15,0)+r3, HX(P0,0), 0
  # P3 and Q3 never change so don't bother saving back
  vadds H(16,0)+r3, HX(Q0,0), 0
  vadds H(17,0)+r3, HX(Q1,0), 0
  vadds H(18,0)+r3, HX(Q2,0), 0

  pop pc

# r4 points to array of beta/tc for each 4 length edge
do_luma_filter:
  valtl H(setup,0),H(setup_input,0),H(setup_input,0) # b*8tc*8
  valtl HX(beta,0),H(setup,0),H(setup,0)
  valtu HX(tc,0),H(setup,0),H(setup,0)
  vmul HX(tc25,0), HX(tc,0), 5
  vadd HX(tc25,0),HX(tc25,0), 1
  vasr HX(tc25,0), HX(tc25,0), 1

  # Compute decision
  vadd HX(dp,0),HX(P1,0),HX(P1,0) # 2*P1
  vsub HX(dp,0),HX(P2,0),HX(dp,0) # P2-2*P1
  vadd HX(dp,0),HX(dp,0),HX(P0,0) # P2-2*P1+P0
  vdist HX(dp,0),HX(dp,0),0 # abs(P2-2*P1+P0) # dp0

  vadd HX(dq,0),HX(Q1,0),HX(Q1,0) # 2*Q1
  vsub HX(dq,0),HX(Q2,0),HX(dq,0) # Q2-2*Q1
  vadd HX(dq,0),HX(dq,0),HX(Q0,0) # Q2-2*Q1+Q0
  vdist HX(dq,0),HX(dq,0),0 # abs(Q2-2*Q1+Q0) # dq0

  vadd HX(d,0), HX(dp,0), HX(dq,0)
  vasr HX(beta2,0),HX(beta,0),2
  vasr HX(beta3,0),HX(beta,0),3

  # Compute flags that are negative if all conditions pass
  vdist HX(decision,0), HX(P0,0), HX(P3,0) CLRA SACC
  vdist HX(decision,0), HX(Q0,0), HX(Q3,0) SACC
  vsub HX(decision,0), HX(decision,0), HX(beta3,0) SETF

  vdist HX(decision,0), HX(P0,0), HX(Q0,0) IFN
  vsub HX(decision,0), HX(decision,0), HX(tc25,0) IFN SETF
  vadd HX(decision,0), HX(d,0), HX(d,0) IFN
  vsub HX(decision,0), HX(decision,0), HX(beta2,0) IFN SETF
  vmov HX(decision,0), 1 IFNN
  vadd H(decision,0),H(decision,3),0 IFN
  vadd H(decision,16),H(decision,19),0 IFN
  vmov -,HX(decision,0) SETF   # N marks strong filter
  vmov HX(decision,0), 1 IFNN  # NN marks normal filter

  vadd HX(do_filter,0), HX(d,3), HX(d,0)
  vsub HX(do_filter,0), HX(do_filter,0), HX(beta,0) SETF # IFNN means no filter
  vmov HX(decision,0),0 IFNN # Z marks no filter

  # Expand out decision (currently valid one every 4 pixels)  0...1...2...3
  # First extract out even terms
  vodd HX(decision,0),HX(decision,0),HX(decision,0)  # 0.1.2.3
  vodd HX(decision,0),HX(decision,0),HX(decision,0)  # 0123
  # Now expand back
  valtl HX(decision,0),HX(decision,0),HX(decision,0) # 00112233
  valtl HX(decision,0),HX(decision,0),HX(decision,0) SETF # 0000111122223333

  # HX(decision,0) is negative if want strong filtering, 1 if want normal filtering, 0 if want no filtering

  # Do a quick check to see if there is anything to do
  mov r11, 0 # Signal no filtering
  vmov -,1 IFNZ SUMS r5
  cmp r5,0
  beq filtering_done
  mov r11, 1 # Signal some filtering
  # And whether there is any strong filtering
  vmov -,1 IFN SUMS r5
  cmp r5,0
  beq normal_filtering

  ##############################################################################
  # Strong filtering - could maybe fast case if all have same sign? (especially if all disabled!)
  vshl HX(tc2,0), HX(tc,0), 1  # Note that in normal filtering tx2 is tc/2, while here it is tc*2

  # Take a copy of the original pixels for use in decision calculation
  vmov HX(P0,32),HX(P0,0)
  vmov HX(Q0,32),HX(Q0,0)
  vmov HX(P1,32),HX(P1,0)
  vmov HX(Q1,32),HX(Q1,0)
  vmov HX(P2,32),HX(P2,0)
  vmov HX(Q2,32),HX(Q2,0)

  vadd -,HX(P2,32),4 CLRA SACC
  vshl -,HX(P1,32),1 SACC
  vshl -,HX(P0,32),1 SACC
  vshl -,HX(Q0,32),1 SACC
  vshl HX(delta,0),HX(Q1,32),0 SACC
  vasr HX(delta,0),HX(delta,0), 3
  vsub HX(delta,0),HX(delta,0),HX(P0,32)
  vclamps HX(delta,0), HX(delta,0), HX(tc2,0)
  vadd HX(P0,0),HX(P0,32),HX(delta,0) IFN

  vadd -,HX(P2,32),2 CLRA SACC
  vadd -,HX(P1,32),HX(P0,32) SACC
  vshl HX(delta,0),HX(Q0,32),0 SACC
  vasr HX(delta,0),HX(delta,0), 2
  vsub HX(delta,0),HX(delta,0),HX(P1,32)
  vclamps HX(delta,0), HX(delta,0), HX(tc2,0)
  vadd HX(P1,0),HX(P1,32),HX(delta,0) IFN

  vadd -,HX(Q0,32),4 CLRA SACC
  vadd -,HX(P1,32),HX(P0,32) SACC
  vmul -,HX(P2,32),3 SACC
  vshl HX(delta,0),HX(P3,0),1 SACC # Note that we have not made a copy of P3, so using P3,0 is correct
  vasr HX(delta,0),HX(delta,0), 3
  vsub HX(delta,0),HX(delta,0),HX(P2,32)
  vclamps HX(delta,0), HX(delta,0), HX(tc2,0)
  vadd HX(P2,0),HX(P2,32),HX(delta,0) IFN
  #vmov HX(P2,0),3 IFN

  # Now reverse all P/Qs

  vadd -,HX(Q2,32),4 CLRA SACC
  vshl -,HX(Q1,32),1 SACC
  vshl -,HX(Q0,32),1 SACC
  vshl -,HX(P0,32),1 SACC
  vshl HX(delta,0),HX(P1,32),0 SACC
  vasr HX(delta,0),HX(delta,0), 3
  vsub HX(delta,0),HX(delta,0),HX(Q0,32)
  vclamps HX(delta,0), HX(delta,0), HX(tc2,0)
  vadd HX(Q0,0),HX(Q0,32),HX(delta,0) IFN

  vadd -,HX(Q2,32),2 CLRA SACC
  vadd -,HX(Q1,32),HX(Q0,32) SACC
  vshl HX(delta,0),HX(P0,32),0 SACC
  vasr HX(delta,0),HX(delta,0), 2
  vsub HX(delta,0),HX(delta,0),HX(Q1,32)
  vclamps HX(delta,0), HX(delta,0), HX(tc2,0)
  vadd HX(Q1,0),HX(Q1,32),HX(delta,0) IFN

  vadd -,HX(P0,32),4 CLRA SACC
  vadd -,HX(Q1,32),HX(Q0,32) SACC
  vmul -,HX(Q2,32),3 SACC
  vshl HX(delta,0),HX(Q3,0),1 SACC # Note that we have not made a copy of Q3, so using Q3,0 is correct
  vasr HX(delta,0),HX(delta,0), 3
  vsub HX(delta,0),HX(delta,0),HX(Q2,32)
  vclamps HX(delta,0), HX(delta,0), HX(tc2,0)
  vadd HX(Q2,0),HX(Q2,32),HX(delta,0) IFN

  ##############################################################################
  # Normal filtering
normal_filtering:
  # Invert the decision flags
  # make instruction more complicated as assembler has error and loses SETF
  vrsub HX(tc10,0), HX(decision,0), 0 SETF # IFN means normal filtering
  vmov  -, HX(tc10,0) SETF # IFN means normal filtering

  vmov -,1 IFN SUMS r5
  cmp r5,0
  beq filtering_done

  vasr HX(tc2,0), HX(tc,0), 1
  vmul HX(tc10,0), HX(tc,0), 10

  vasr HX(thresh,0), HX(beta,0), 1
  vadd HX(thresh,0), HX(thresh,0), HX(beta,0)
  vasr HX(thresh,0), HX(thresh,0), 3 CLRA SACC

  vadd HX(ptest,0),HX(dp,3),HX(dp,0)
  vsub HX(ptest,0),HX(ptest,0),HX(thresh,0) # ptest is negative if we need to do the P2 pixel
  vadd HX(qtest,0),HX(dq,3),HX(dq,0)
  vsub HX(qtest,0),HX(qtest,0),HX(thresh,0) # qtest is negative if we need to do the Q2 pixel
  # Expand ptest and qtest together
  vodd HX(pqtest,0),HX(ptest,0),HX(qtest,0)  # p.p.p.p.q.q.q.q
  vodd HX(pqtest,0),HX(pqtest,0),HX(pqtest,0) # ppppqqqq........
  valtl HX(pqtest,0),HX(pqtest,0),HX(pqtest,0) # ppppppppqqqqqqqq
  valtl HX(ptest,0),HX(pqtest,0),HX(pqtest,0)
  valtu HX(qtest,0),HX(pqtest,0),HX(pqtest,0)

  vsub HX(delta0,0), HX(Q0,0), HX(P0,0)
  vsub HX(delta1,0), HX(Q1,0), HX(P1,0)
  vmov -,8 CLRA SACC
  vmul -,HX(delta0,0), 9 SACC
  vmul HX(delta0,0),HX(delta1,0), r6 SACC
  vasr HX(delta0,0), HX(delta0,0), 4
  vdist HX(deltatest,0), HX(delta0,0), 0
  vsub HX(deltatest,0), HX(deltatest,0), HX(tc10,0) IFN SETF # negative if still need to do something
  vmov HX(deltatest,0), 0 IFNN # clear if no need to do anything so we can reload flags later

  vclamps HX(delta0,0), HX(delta0,0), HX(tc,0)

  vadd HX(deltap1,0), HX(P2,0), HX(P0,0)
  vadd HX(deltap1,0), HX(deltap1,0), 1
  vasr HX(deltap1,0), HX(deltap1,0), 1 CLRA SACC
  vsub HX(deltap1,0), HX(delta0,0), HX(P1,0) SACC
  vasr HX(deltap1,0), HX(deltap1,0), 1
  vclamps HX(deltap1,0), HX(deltap1,0), HX(tc2,0)

  vadd HX(deltaq1,0), HX(Q2,0), HX(Q0,0)
  vadd HX(deltaq1,0), HX(deltaq1,0), 1
  vasr HX(deltaq1,0), HX(deltaq1,0), 1 CLRA SACC
  vadd HX(deltaq1,0), HX(delta0,0), HX(Q1,0)
  vrsub -, HX(delta0,0), 0 SACC
  vrsub HX(deltaq1,0), HX(Q1,0), 0 SACC
  vasr HX(deltaq1,0), HX(deltaq1,0), 1
  vclamps HX(deltaq1,0), HX(deltaq1,0), HX(tc2,0)

  vadds HX(P0,0), HX(P0,0), HX(delta0,0) IFN
  vsubs HX(Q0,0), HX(Q0,0), HX(delta0,0) IFN

  vmov -,HX(ptest,0) IFN SETF # Negative if need to do p1
  vadds HX(P1,0), HX(P1,0), HX(deltap1,0) IFN

  vmov -,HX(deltatest,0) SETF
  vmov -,HX(qtest,0) IFN SETF # Negative if need to do q1
  vadds HX(Q1,0), HX(Q1,0), HX(deltaq1,0) IFN

  #vmov HX(P2,0),1 IFN

filtering_done:
  b lr

# Deblocks an entire row of the image
# [0] = base address;
# [1] = pitch in bytes
# [2] = num 16x16 to process
# [3] = pointer to setup data
# [4] = num16highchroma;
# [5] = 3/4 command number (4 says to clear setup data as well)

# Strengths are stored as:
# setup[block_idx][0=vert,1=horz][0=first edge, 1=second edge][0=u,1=v][0..3=edge number]
#
# We need to:
#  Adjust r0 and r2 appropriately when reach 128 stride
#  Convert strengths into right locations
#     u0u1u2u3v0v1v2v3 -> u0v0u0v0u0v0u0v0 u1v1u1v1u1v1u1v1 for filtering across horizontal edges
#     u0u1u2u3v0v1v2v3 -> first extract all u edges to filter, then extract all v edges to filter for vertical edges
# We will processing a pair of 8*16 blocks of samples in each iteration (allowing the same underlying code to work once we extract the strengths appropriately)
# A single load of setup contains enough information for the whole 16x16 block

hevc_uv_deblock_16x16:
  push r6-r15, lr
  mov r14,0
  b hevc_uv_start
hevc_uv_deblock_16x16_with_clear:
  push r6-r15, lr
  mov r14,1
  b hevc_uv_start

hevc_uv_start:
  mov r9,r4
  mov r4,r3
  mov r13,r2
  mov r2,r0
  mov r10,r0
  subscale4 r0,r1
  mov r8,63
  mov r6,-3
  vmov H(zeros,0),0
# r7 is number of blocks still to load
# r0 is location of current block - 4 * stride
# r1 is stride
# r2 is location of current block
# r3 is offset of start of block (actual edges start at H(16,16)+r3 for horizontal and H(16,0)+r3 for vertical
# r4 is setup
# r5 is for temporary calculations
# r8 holds 63
# r6 holds -3
# r9 holds the number of 16 high rows to process
# r10 holds the original img base
# r11 returns 0 if no filtering was done on the edge
# r12 saves a copy of this
# r13 is copy of width
# r14 is 1 if we should clear the old contents, or 0 if not

uv_process_row:
  # First iteration does not do horizontal filtering on previous
  mov r7, r13
  mov r3,0
  vldb H(setup_input,0), (r4)  # We may wish to prefetch these
.if BIT_DEPTH==8
  vldb H(12++,16)+r3,(r0 += r1) REP 4    # Load the current block
  vldb H(16++,16)+r3,(r2 += r1) REP 16
.else
  vldh HX(12++,0)+r3,(r0 += r1) REP 4    # Load the current block
  vldh HX(16++,0)+r3,(r2 += r1) REP 16
  vshl H(setup_input,0),H(setup_input,0),(BIT_DEPTH-8)
.endif
  cmp r14,1
  bne uv_skip0
  vadd H(setup_input,0),H(setup_input,4),0 # Rotate by 4 to access V strengths
  vstb H(zeros,0),(r4)
uv_skip0:
  bl uv_vert_filter
  add r3,8
  vadd H(setup_input,0),H(setup_input,8),0 # Rotate to second set of 8
  bl uv_vert_filter
  sub r3,8
  b uv_start_deblock_loop
uv_deblock_loop:
  # Middle iterations do vertical on current block and horizontal on preceding
  vldb H(setup_input,0), (r4)
.if BIT_DEPTH==8
  vldb H(12++,16)+r3,(r0 += r1) REP 4  # load the current block
  vldb H(16++,16)+r3,(r2 += r1) REP 16
.else
  vldb HX(12++,0)+r3,(r0 += r1) REP 4  # load the current block
  vldb HX(16++,0)+r3,(r2 += r1) REP 16
  vshl H(setup_input,0),H(setup_input,0),(BIT_DEPTH-8)
.endif
  cmp r14,1
  bne uv_skip1
  vadd H(setup_input,0),H(setup_input,4),0 # Rotate by 4 to access V strengths
  vstb H(zeros,0),(r4)
uv_skip1:
  bl uv_vert_filter
  add r3,8
  vadd H(setup_input,0),H(setup_input,8),0
  bl uv_vert_filter
  sub r3,8
  vldb H(setup_input,0), -16(r4)
  cmp r14,1
  bne uv_skip3
  vadd H(setup_input,0),H(setup_input,4),0 # Rotate by 4 to access V strengths
  vstb H(zeros,0),-16(r4)
uv_skip3:
  bl uv_horz_filter
  mov r12,r11
  add r3,8*64
  vadd H(setup_input,0),H(setup_input,8),0
  bl uv_horz_filter
  sub r3,8*64
  addcmpbeq r12,0,0,uv_skip_save_top
  vstb H(12++,0)+r3,-16(r0 += r1) REP 4  # Save the deblocked pixels for the previous block
uv_skip_save_top:
  vstb H(16++,0)+r3,-16(r2 += r1) REP 16
uv_start_deblock_loop:
  # move onto next 16x16 (could do this with circular buffer support instead)
  add r3,16

  and r3,r8
  add r4,32
  # Perform loop counter operations (may work with an addcmpbgt as well?)
  add r0,16
  add r2,16
  sub r7,1
  cmp r7,0 # Are there still more blocks to load
  bgt uv_deblock_loop

  # Final iteration needs to just do horizontal filtering
  vldb H(setup_input,0), -16(r4)
  cmp r14,1
  bne uv_skip2
  vadd H(setup_input,0),H(setup_input,4),0 # Rotate by 4 to access V strengths
  vstb H(zeros,0),-16(r4)
uv_skip2:
  bl uv_horz_filter
  mov r12,r11
  add r3,8*64
  vadd H(setup_input,0),H(setup_input,8),0
  bl uv_horz_filter
  sub r3,64*8
  addcmpbeq r12,0,0,uv_skip_save_top2
  vstb H(12++,0)+r3,-16(r0 += r1) REP 4  # Save the deblocked pixels for the previous block
uv_skip_save_top2:
  vstb H(16++,0)+r3,-16(r2 += r1) REP 16

# Now look to see if we should do another row
  sub r9,1
  cmp r9,0
  bgt uv_start_again
  pop r6-r15, pc
uv_start_again:
  # Need to sort out r0,r2 to point to next row down
  addscale16 r10,r1
  mov r2,r10
  subscale4 r0,r2,r1
  b uv_process_row


# At this stage H(16,16)+r3 points to the first pixel of the 16 high edge to be filtered
# So we can reuse the code we move the parts to be filtered into HX(P0/P1/P2/P3/Q0/Q1/Q2/Q3,0) - we will perform a final saturation step on placing them back into the correct locations

uv_vert_filter:
  push lr

  vmov HX(P1,0), V(16,14)+r3
  vmov HX(P0,0), V(16,15)+r3
  vmov HX(Q0,0), V(16,16)+r3
  vmov HX(Q1,0), V(16,17)+r3

  bl do_chroma_filter

  vadds V(16,15)+r3, HX(P0,0), 0
  vadds V(16,16)+r3, HX(Q0,0), 0

  pop pc

# Filter edge at H(16,0)+r3
uv_horz_filter:
  push lr

  vmov HX(P1,0), H(14,0)+r3
  vmov HX(P0,0), H(15,0)+r3
  vmov HX(Q0,0), H(16,0)+r3
  vmov HX(Q1,0), H(17,0)+r3

  bl do_chroma_filter

  vadds H(15,0)+r3, HX(P0,0), 0
  # P3 and Q3 never change so don't bother saving back
  vadds H(16,0)+r3, HX(Q0,0), 0

  pop pc

# r4 points to array of beta/tc for each 4 length edge
do_chroma_filter:
  valtl H(setup,0),H(setup_input,0),H(setup_input,0) # tc*8
  valtl HX(tc,0),H(setup,0),H(setup,0)
do_chroma_filter_with_tc: # Alternative entry point when tc already prepared
  vsub HX(delta,0),HX(Q0,0),HX(P0,0)
  vshl HX(delta,0),HX(delta,0),2 CLRA SACC
  vsub -,HX(P1,0),HX(Q1,0) SACC
  vmov HX(delta,0),4 SACC
  vasr HX(delta,0),HX(delta,0),3
  vclamps HX(delta,0), HX(delta,0), HX(tc,0)
  vadd HX(P0,0),HX(P0,0),HX(delta,0)
  vsub HX(Q0,0),HX(Q0,0),HX(delta,0)
  b lr

# This version works in a striped format
#
# samples are stored UVUV in stripes 128 bytes wide.
#
# Deblocks an entire row of the image
# [0] = base address;
# [1] = pitch in bytes to move to next stripe
# [2] = num 8x16 to process
# [3] = pointer to setup data
# [4] = num16highchroma;
# [5] = 7 command number

# Strengths are stored for each 8x16 as:
#
# Vertical filtering (vertical edges)
#   setup[block_idx][1][0=u,1=v][0..3 edge number]
#
# Horizontal filtering (horizontal edges)
#   setup[block_idx][0][0=first edge, 1=second edge][0=u,1=v][0..1 edge number]
#
# Note order is different for horiz and vert
#
# We need to:
#  Adjust r0 and r2 appropriately when reach 128 stride
#  Convert strengths into right locations
#     u0u1v0v1u2u3v2v3 -> u0v0u0v0u0v0u0v0 u1v1u1v1u1v1u1v1 for filtering across horizontal edges
#     u0u1u2u3v0v1v2v3 -> first extract all u edges to filter, then extract all v edges to filter for vertical edges
# We will process a 8*16 block of U samples interleaved with a 8x16 block of V samples in each iteration
#
# Use setup_old to store setup for previous 8x16 (because we do the horiz filters one step behind)
#
# Vertical filtering does one pass for top edge, one for middle edge
# Horiz filtering does one pass for U, then one pass for V

hevc_uv_deblock_16x16_striped:
  push r6-r15, lr
  mov r14, r1 # Save stride between stripes
  mov r1, 128 # Stride in bytes between rows
  mov r9,r4   # Save num16 high
  mov r4,r3
  mov r13,r2
  mov r2,r0
  mov r10,r0
  subscale4 r0,r1
  mov r8,63
  mov r6,-3
  vmov H(zeros,0),0
  mov r3, (1<<BIT_DEPTH)-1
  vmov HX(maxvalue,0), r3

# r0 is location of current block - 4 * stride
# r1 is stride between rows = 128
# r2 is location of current block
# r3 is offset of start of block (actual edges start at H(16,16)+r3 for horizontal and H(16,0)+r3 for vertical
# r4 is pointer to setup data
# r5 is for temporary calculations
# r6 holds -3
# r7 is how much to add (for r0/r2) to move right (16 in the middle or 16-128+r4 otherwise)
# r8 holds 63
# r9 holds the number of 16 high rows to process
# r10 holds the original img base
# r11 returns 0 if no filtering was done on the edge
# r12 saves a copy of r11 so we can disable writeback if nothing done
# r13 stores num8
# r14 is stride between tiles
# r15 counts up to r13 when we reach 8*16 we need to move to the next stripe

uv_process_row_striped:
  # First iteration does not do horizontal filtering on previous
  mov r15, 0
  mov r3,0
  vldb H(setup_input,0), (r4)  # We may wish to prefetch these
.if BIT_DEPTH==8
  vldb H(12++,16)+r3,(r0 += r1) REP 4    # Load the current block
  vldb H(16++,16)+r3,(r2 += r1) REP 16
.else
  vldh HX(12++,0)+r3,(r0 += r1) REP 4    # Load the current block
  vldh HX(16++,0)+r3,(r2 += r1) REP 16
  vshl H(setup_input,0),H(setup_input,0),(BIT_DEPTH-8)
.endif
  vstb H(zeros,0),(r4)
  # Filter the left edge
  bl uv_vert_filter_striped
  b uv_start_deblock_loop_striped
uv_deblock_loop_striped:
  # Middle iterations do vertical on current block and horizontal on preceding
  vldb H(setup_input,0), (r4)
.if BIT_DEPTH==8
  vldb H(12++,16)+r3,(r0 += r1) REP 4  # load the current block
  vldb H(16++,16)+r3,(r2 += r1) REP 16
.else
  vldh HX(12++,0)+r3,(r0 += r1) REP 4
  vldh HX(16++,0)+r3,(r2 += r1) REP 16
  vshl H(setup_input,0),H(setup_input,0),(BIT_DEPTH-8)
.endif
  # TODO if the setup is entirely zeros, we can skip the following processing!
  vstb H(zeros,0),(r4)
  bl uv_vert_filter_striped

  bl uv_horz_filter_striped
  mov r12,r11
  add r3,8*64
  vadd H(setup_old,0),H(setup_old,4),0 # Shift to bottom edge
  bl uv_horz_filter_striped
  sub r3,8*64
  # Move back to previous location temporarily
  sub r0,r7
  sub r2,r7
  addcmpbeq r12,0,0,uv_skip_save_top_striped
.if BIT_DEPTH==8
  vstb H(12++,0)+r3,(r0 += r1) REP 4  # Save the deblocked pixels for the previous block
uv_skip_save_top_striped:
  vstb H(16++,0)+r3,(r2 += r1) REP 16
  add r0,r7
  add r2,r7
uv_start_deblock_loop_striped:
  # move onto next 16x16 (could do this with circular buffer support instead)
  add r3,16
.else
  vsth HX(12++,32)+r3,(r0 += r1) REP 4  # Save the deblocked pixels for the previous block
uv_skip_save_top_striped:
  vsth HX(16++,32)+r3,(r2 += r1) REP 16
  add r0,r7
  add r2,r7
uv_start_deblock_loop_striped:
  # move onto next 16x16 (could do this with circular buffer support instead)
  add r3,32
.endif
  vadd H(setup_old,0), H(setup_input,8),0 # Save setup for use for horizontal edges
  and r3,r8
  add r4,16
  # Perform loop counter operations (may work with an addcmpbgt as well?)
  add r15,1 # When we reach 8 we need to switch r7
.if BIT_DEPTH==8
  mov r7, 16 # Amount to move on
  and r5,r15,7
.else
  mov r7, 32
  and r5,r15,3
.endif
  cmp r5,0
  bne normal_case
.if BIT_DEPTH==8
  add r7,r14,16-128 # move back to left edge, then move on one stripe
.else
  add r7,r14,32-128
.endif
normal_case:
  add r0,r7
  add r2,r7
  cmp r15,r13 # Are there still more blocks to load
  blt uv_deblock_loop_striped

  # Final iteration needs to just do horizontal filtering
  bl uv_horz_filter_striped
  mov r12,r11
  add r3,8*64
  vadd H(setup_old,0),H(setup_old,4),0
  bl uv_horz_filter_striped
  sub r3,64*8
  sub r0,r7
  sub r2,r7
  addcmpbeq r12,0,0,uv_skip_save_top2_striped
.if BIT_DEPTH==8
  vstb H(12++,0)+r3,(r0 += r1) REP 4  # Save the deblocked pixels for the last block
uv_skip_save_top2_striped:
  vstb H(16++,0)+r3,(r2 += r1) REP 16
.else
  vsth HX(12++,32)+r3,(r0 += r1) REP 4  # Save the deblocked pixels for the last block
uv_skip_save_top2_striped:
  vsth HX(16++,32)+r3,(r2 += r1) REP 16
.endif
# Now look to see if we should do another row
  sub r9,1
  cmp r9,0
  bgt uv_start_again_striped
  pop r6-r15, pc
uv_start_again_striped:
  # Need to sort out r0,r2 to point to next row down
  addscale16 r10,r1
  mov r2,r10
  subscale4 r0,r2,r1
  b uv_process_row_striped


# At this stage H(16,16)+r3 points to the first pixel of the 16 high edge to be filtered
# So we can reuse the code we move the parts to be filtered into HX(P0/P1/P2/P3/Q0/Q1/Q2/Q3,0) - we will perform a final saturation step on placing them back into the correct locations

# This routine assumes H(setup_input,0) contains u0u1u2u3v0v1v2v3
# We first filter the U samples, then the V samples
uv_vert_filter_striped:
  push lr

  # U filtering
.if BIT_DEPTH==8
  vmov HX(P1,0), V(16,12)+r3
  vmov HX(P0,0), V(16,14)+r3
  vmov HX(Q0,0), V(16,16)+r3
  vmov HX(Q1,0), V(16,18)+r3
.else
  vmov HX(P1,0), VX(16,44)+r3
  vmov HX(P0,0), VX(16,46)+r3
  vmov HX(Q0,0), VX(16,0)+r3
  vmov HX(Q1,0), VX(16,2)+r3
.endif
  valtl H(setup,0),H(setup_input,0),H(setup_input,0) # tc*8
  valtl HX(tc,0),H(setup,0),H(setup,0)
  bl do_chroma_filter_with_tc

.if BIT_DEPTH==8
  vadds V(16,14)+r3, HX(P0,0), 0
  vadds V(16,16)+r3, HX(Q0,0), 0
.else
  vmax HX(P0,0),HX(P0,0),0
  vmax HX(Q0,0),HX(Q0,0),0
  vmin VX(16,46)+r3, HX(P0,0), HX(maxvalue,0)
  vmin VX(16,0)+r3, HX(Q0,0), HX(maxvalue,0)
.endif

  # V filtering
.if BIT_DEPTH==8
  vmov HX(P1,0), V(16,13)+r3
  vmov HX(P0,0), V(16,15)+r3
  vmov HX(Q0,0), V(16,17)+r3
  vmov HX(Q1,0), V(16,19)+r3
.else
  vmov HX(P1,0), VX(16,45)+r3
  vmov HX(P0,0), VX(16,47)+r3
  vmov HX(Q0,0), VX(16,1)+r3
  vmov HX(Q1,0), VX(16,3)+r3
.endif

  vadd  H(setup,0),H(setup_input,4),0 # Shift to V samples
  valtl H(setup,0),H(setup,0),H(setup,0)
  valtl HX(tc,0),H(setup,0),H(setup,0)
  bl do_chroma_filter_with_tc

.if BIT_DEPTH==8
  vadds V(16,15)+r3, HX(P0,0), 0
  vadds V(16,17)+r3, HX(Q0,0), 0
.else
  vmax HX(P0,0),HX(P0,0),0
  vmax HX(Q0,0),HX(Q0,0),0
  vmin VX(16,47)+r3, HX(P0,0), HX(maxvalue,0)
  vmin VX(16,1)+r3, HX(Q0,0), HX(maxvalue,0)
.endif

  pop pc

# Filter edge at H(16,0)+r3
# weights in setup_old: u0u1v0v1
uv_horz_filter_striped:
  push lr

.if BIT_DEPTH==8
  vmov HX(P1,0), H(14,0)+r3
  vmov HX(P0,0), H(15,0)+r3
  vmov HX(Q0,0), H(16,0)+r3
  vmov HX(Q1,0), H(17,0)+r3
.else
  vmov HX(P1,0), HX(14,32)+r3
  vmov HX(P0,0), HX(15,32)+r3
  vmov HX(Q0,0), HX(16,32)+r3
  vmov HX(Q1,0), HX(17,32)+r3
.endif

  valtl H(setup,0),H(setup_old,0),H(setup_old,0) # tc*8
  valtl H(setup,0),H(setup,0),H(setup,0) # u0u0u0u0u1u1u1u1v0v0v0v0v1v1v1v1
  vadd H(tc,0),H(setup,8),0 # Could remove this if we store u/v in alternate order (issue is only ra port has shifted access)
  valtl HX(tc,0),H(setup,0),H(tc,0) # u0v0u1v1...
  bl do_chroma_filter_with_tc

.if BIT_DEPTH==8
  vadds H(15,0)+r3, HX(P0,0), 0
  # P3 and Q3 never change so don't bother saving back
  vadds H(16,0)+r3, HX(Q0,0), 0
.else
  vmax HX(P0,0),HX(P0,0),0
  vmax HX(Q0,0),HX(Q0,0),0
  vmin HX(15,32)+r3, HX(P0,0), HX(maxvalue,0)
  vmin HX(16,32)+r3, HX(Q0,0), HX(maxvalue,0)
.endif
  pop pc

# r0 = list
# r1 = number
hevc_run_command_list:
  push r6-r7, lr
  mov r6, r0
  mov r7, r1
loop_cmds:
  ld r0,(r6) # How to encode r6++?
  add r6,4
  ld r1,(r6)
  add r6,4
  ld r2,(r6)
  add r6,4
  ld r3,(r6)
  add r6,4
  ld r4,(r6)
  add r6,4
  ld r5,(r6)
  add r6,4
  bl hevc_trans_16x16
  sub r7,1
  cmp r7,0
  bgt loop_cmds

  pop r6-r7, pc


# SAO FILTER
#
# Works in units of 8x16 blocks with interleaved U/V samples
#
# r0 = recPicture
# r1 = used as temporary
# r2 = upper context
# r3 pointer to current block (0,16,32,48)
# r6 sao data
#
# EDGE MODE
# {typeidx(2) spare(4) eoclass(2) saooffsetval_u(6*4)}
# {flags(8) saooffsetval_v(6*4)}
#
# BAND MODE
# {typeidx(2) spare(1) leftclass_u(5) saooffsetval_u(6*4)}
# {spare(3) leftclass_v(5) saooffsetval_v(6*4)}
# For edge, sidedata is 8 flags
# For band, sidedata is leftclass
#
# sao values packed 6 bits each, LSBits for offset[4].
#
# Flag values mark which parts of the boundary to restore
#
# r7  number of 8x16 blocks to process
# r8  counter of 8x16 blocks processed
# r9  stride between tiles
# r10 type
# r11 pointer to sao data
# r12 width left
# r13 pos of bottom row * 64
# r14 amount to add to go right by 8 UV pairs (either 16 or a new tile)
# r15 upper limit for sao_block loop
.set Vprevabove, 0
.set Vprevmid, 1
.set Vprevbelow, 3
.set Vnextabove, 4
.set Vnextmid, 5
.set Vnextbelow, 7

# sao offsets need 32bits of precision
.set VsaoOffsetVal, 8
# Voffsets need 32bits of precision
.set Voffset, 9

.set Vbottomrightcontext, 32

.set Vdiff0, 51
.set Vdiff1, 52
.set Vband, 53
.set Vzeros, 54
.set Vtemp, 55
.set VLeftClass, 56
.set Vk, 57
.set Vsaomax, 58

.set LeftFlag, 16+8
.set TopFlag, 17+8
.set RightFlag, 18+8
.set BottomFlag, 19+8

.set TopLeftFlag, 20+8
.set TopRightFlag, 21+8
.set BottomLeftFlag, 22+8
.set BottomRightFlag, 23+8

.set EoClass0, 24
.set EoClass1, 25
.set LeftClass, 24

# src(r0), stride(r1), upper context(r2), sao_data(r3), h_w (r4)
#
# h_w stores height of row in top 16 bits, and width in bottom 16 bits
# this allows the correct determination of the corners to be made for restoring the edges
#
sao_process_row:
  push r6-r15, lr
  mov r9, r1
  mov r11, r3
  mov r12, 0xffff
  and r12, r12, r4 # Width
  lsr r13, r4, 16 # Height
  sub r13, r13, 1 # last row
  min r13, r13, 15
  lsl r13, r13, 6 # last row * 64
  add r7, r12, 7
  lsr r7, r7, 3 # Number of 8x16 blocks to process
  mov r8, 0 # Number processed
  vmov HX(Vzeros,0),0
  mov r1, (1<<BIT_DEPTH)-1
  vmov HX(Vsaomax,0), r1
  mov r1, 128

.if BIT_DEPTH==8
  mov r14, 16 # Amount to move onto next block
  # Prepare by loading the first block
  vldb H(15,32), (r2)
  addscale16 r6, r0, r1
  vldb H(16++,32), (r0 += r1) REP 16
  vldb H(32,32), (r6)
.else
  mov r14, 32
  vldh HX(15,32), (r2)
  addscale16 r6, r0, r1
  vldh HX(16++,32), (r0 += r1) REP 16
  vldh HX(32,32), (r6)
  # Loop expects some of these values to be saved
  vmov HX(Vnextabove,0), HX(15,32)
  vmov HX(Vnextbelow,0), HX(32,32)
  vmov HX(Vnextmid++,0), VX(16,46++) REP 2
.endif

  # We have no need to store horizontal context as we always do an entire 16 high row each time.

  # However, we may need diagonals and right pixels so fetch the block data one ahead

sao_block_loop:
  # At start of loop:
  #  r0 points to current block (that we will need to save)
  #  r5 gives offset to next block
  #  r8 gives index (in units of 8x16) of current block

  # Load data into H(16,16) -> transform into H(16,48)

.if BIT_DEPTH==8
  # Shift context (corresponding to recPicture)
  vmov H(15,0), H(15,16)
  vmov V(16,14++), V(16,30++) REP 2
  vmov H(32,0), H(32,16)

  # Shift prefetched block into current block
  vmov H(15,16), H(15,32)
  vmov H(16++,16), H(16++,32) REP 16
  vmov H(32,16), H(32,32)
  mov r1, 128

  # Load upper context
  vldb H(15,32), 16(r2)
  # Load next block
  add r0, r14
  addscale16 r6, r0, r1
  vldb H(16++,32), (r0 += r1) REP 16  # TODO prefetch to avoid stalls
  # Load bottom right data
  vldb H(32,32), (r6)
  sub r0, r14
.else
  # At this stage
  # HX(15++,0) REP 18 has current block
  # HX(15++,32) REP 18 has next block, except for right hand two columns which contain prev
  # HX(nextmid++,0) REP 2 has right two columns of next
  # HX(nextabove,0) has next upper context
  # HX(nextbelow,0) has next lower context

  # First repair the next block
  vmov HX(15,32), HX(Vnextabove,0)
  vmov HX(32,32), HX(Vnextbelow,0)
  vmov VX(16,46++), HX(Vnextmid++,0) REP 2
  # Then save contents of this that we will need to replace
  vmov HX(Vprevabove,0), HX(15,0)
  vmov HX(Vprevbelow,0), HX(32,0)
  vmov HX(Vprevmid++,0), VX(16,14++) REP 2
  # Copy next into this
  vmov HX(15,0), HX(15,32)
  vmov HX(16++,0), HX(16++,32) REP 16
  vmov HX(32,0), HX(32,32)
  # Load new next
  mov r1, 128
  vldh HX(15,32), 32(r2)
  add r0, r14
  addscale16 r6, r0, r1
  vldh HX(16++,32), (r0 += r1) REP 16
  # Load bottom right data
  vldh HX(32,32), (r6)

  sub r0, r14
  ### Store next values
  vmov HX(Vnextabove,0), HX(15,32)
  vmov HX(Vnextbelow,0), HX(32,32)
  vmov HX(Vnextmid++,0), VX(16,46++) REP 2
  # replace with prev values
  mov r4, 0xC000
  #vbitplanes -, r4 SETF
  .half 0xf408
  .half 0xe038
  .half 0x03c4
  vmov VX(16,46++),HX(Vprevmid++,0) REP 2
  vmov HX(15,32), HX(Vprevabove,0) IFNZ
  vmov HX(32,32), HX(Vprevbelow,0) IFNZ
.endif

  # Load data:
  ld r6, (r11)
  ld r3, 4(r11)

  # Move the offset values into the VRF
  vmov HY(VsaoOffsetVal,0), r6
  valtl HY(VsaoOffsetVal,0),HY(VsaoOffsetVal,0), r3 # Alternate u with v offsets
  vshl HY(VsaoOffsetVal,0),HY(VsaoOffsetVal,0),8 # Move into MSBs

  # Process and save block
  bl sao_block

  # Save upper context
.if BIT_DEPTH==8
  vstb H(31,16),(r2)
.else
  vsth HX(31,0),(r2)
.endif
  # Move onto next block
  add r0, r14

  # Prepare r5 to move to the following block
  add r8,1 # When we reach 8 we need to switch r7

.if BIT_DEPTH==8
  mov r14,16 # Amount to move on
  and r1,r8,7
  cmp r1,7
  bne normal_case_sao
  add r14,r9,16-128 # move back to left edge, then move on one stripe
normal_case_sao:
  # Update loop variables
  add r2, 16
.else
  mov r14,32 # Amount to move on in bytes
  and r1,r8,3
  cmp r1,3
  bne normal_case_sao
  add r14,r9,32-128 # move back to left edge, then move on one stripe
normal_case_sao:
  # Update loop variables
  add r2, 32
.endif

  add r11, 8
  sub r12, 8

  cmp r8,r7
  blt sao_block_loop
  pop r6-r15, pc


sao_block:
  lsr r10, r6, 30 # Top 2 bits are class
  cmp r10,2
  beq sao_edge
  cmp r10,1
  beq sao_band
  # sao_copy has nothing to do
  b lr

sao_band:
  asr r6,LeftClass # only bottom 5 bits are used
  asr r3,LeftClass
  vmov H(VLeftClass,0), r6
  valtl H(VLeftClass,0), H(VLeftClass,0), r3 # Alternate U and V left class

  vmov HX(Vtemp,0), 4

  # Set up loop to process a single 16x16 block
  mov r3, 0
  mov r1,64
  addscale16 r15,r3,r1
sao_band_loop:
.if BIT_DEPTH==8
  vmov H(16,48)+r3, H(16,16)+r3  # Set up default values
  vasr HX(Vband,0),H(16,16)+r3,3
.else
  vmov HX(48,32)+r3, HX(16,0)+r3
  vasr HX(Vband,0),HX(16,0)+r3,BIT_DEPTH-5
.endif
  # bandIdx = bandTable[band]
  # bandTable[(k+leftClass)&31] = k+1 . [0<=k<4]
  # (k+leftClass)&31 = band
  # (k+leftClass) = band (mod 32)
  # k = band - leftClass (mod 32)
  vsub HX(Vk,0), HX(Vband,0), H(VLeftClass,0)
  vand HX(Vk,0), HX(Vk,0), 31 SETF
  # WARNING the assembler seems to ignore SETF if we use an immediate instead of HX(Vtemp,0)
  vsub -, HX(Vk,0), HX(Vtemp,0) SETF  # Flags are N if k in legal range
  vmul HX(Vk,0), HX(Vk,0), 6 # Amount to shift
  vshl HY(Voffset,0),HY(VsaoOffsetVal,0), HX(Vk,0)
  vasr HY(Voffset,0),HY(Voffset,0),26 # 32-6
.if BIT_DEPTH==8
  vadds H(16,48)+r3, H(16,16)+r3, HX(Voffset,0) IFN
.else
  vadd HX(48,32)+r3, HX(16,0)+r3, HX(Voffset,0) IFN
  vmax HX(48,32)+r3, HX(48,32)+r3, 0
  vmin HX(48,32)+r3, HX(48,32)+r3, HX(Vsaomax,0)
.endif
  addcmpblt r3,r1,r15,sao_band_loop
  subscale16 r3,r15,r1 # Put r3 back again

  # Store processed block
  mov r1, 128
.if BIT_DEPTH==8
  vstb H(16++,48), (r0 += r1) REP 16
.else
  vsth HX(48++,32), (r0 += r1) REP 16
.endif
  b lr

.if BIT_DEPTH==8
# We pass in r3,r4,r5 as the three locations to be examined
sao_edge:
  # Extract eoclass
  btst r6, EoClass1
  bne class_23
  btst r6, EoClass0
  bne class_1
class_0:
  mov r4, 16-2+(16-0)*64 # -1,0
  mov r5, 16+2+(16-0)*64  # 1,0
  b got_class
class_1:
  mov r4, 16+(16-1)*64 # 0,-1
  mov r5, 16+(16+1)*64    # 0,1
  b got_class
class_23:
  btst r6, EoClass0
  bne class_3
class_2:
  mov r4, 16-2+(16-1)*64 # -1,-1
  mov r5, 16+2+(16+1)*64 # 1,1
  b got_class
class_3:
  mov r4, 16+2+(16-1)*64 # 1,-1
  mov r5, 16-2+(16+1)*64 # -1,1
got_class:
  mov r6,r3 # Save flags for later

  # Set up loop to process a single 16x16 block
  mov r3, 0
  mov r1,64
  addscale16 r15,r3,r1
sao_edge_loop:
  vmov H(16,48)+r3, H(16,16)+r3 # Default values are unchanged
  vsub HX(Vdiff0,0),H(16,16)+r3,H(0,0)+r4
  vsgn HX(Vk,0),HX(Vdiff0,0),HX(Vzeros,0) CLRA SACC
  vsub HX(Vdiff1,0),H(16,16)+r3,H(0,0)+r5
  vsgn HX(Vk,0),HX(Vdiff1,0),HX(Vzeros,0) SACC SETF
  vadd HX(Vk,0),HX(Vk,0),HX(Vzeros,0) SETF  # Not sure why the flags are not already set correctly, but this instruction seems necessary
  # Now we have a value of:
  # -2 -> 1 -> 0 shift
  # -1 -> 2 -> 1 shift
  # 0 -> 0
  # 1 -> 3 -> 2 shift
  # 2 -> 4 -> 3 shift
  # So add 1, then another 1 if negative
  vadd HX(Vk,0), HX(Vk,0), 1
  vadd HX(Vk,0), HX(Vk,0), 1 IFN
  ##vadd H(16,48)+r3,HX(Vk,0), 0

  vmul HX(Vk,0), HX(Vk,0), 6 # Amount to shift
  vshl HY(Voffset,0),HY(VsaoOffsetVal,0), HX(Vk,0)
  vasr HY(Voffset,0),HY(Voffset,0),26
  #vmov H(16,48)+r3,0
  #vmov H(16,48)+r3,1 IFZ
  #vmov H(16,48)+r3,4 IFNZ

  vadds H(16,48)+r3, H(16,16)+r3, HX(Voffset,0) IFNZ

  add r4,r1
  add r5,r1
  addcmpblt r3,r1,r15,sao_edge_loop

  mov r4, 0x3ffc  # Repair all but corners (done afterwards)
  #vbitplanes -, r4 SETF
  .half 0xf408
  .half 0xe038
  .half 0x03c4

  # Check if we need to repair some edges
  mov r5, r13 # Offset to reach bottom edge
  sub r3, r12 , 1
  min r3, r3, 7 # Offset for right edge
  add r3, r3, r3 # * 2 for U/V offsets
  btst r6, TopFlag # Set if at the top edge, so ne means we need to restore, eq means not
  beq done_top
  vmov H(16,48), H(16,16) IFNZ
done_top:
  btst r6, BottomFlag
  beq done_bottom
  vmov H(16,48)+r5, H(16,16)+r5 IFNZ
done_bottom:

  mov r4, 0x7ffe  # Repair all but corners (done afterwards)
  #vbitplanes -, r4 SETF
  .half 0xf408
  .half 0xe038
  .half 0x03c4

  btst r6, LeftFlag
  beq done_left
  vadd V(16,48++), V(16,16++), 0 REP 2 IFNZ
done_left:
  btst r6, RightFlag
  beq done_right
  vmov V(16,48++)+r3, V(16,16++)+r3 REP 2 IFNZ
done_right:

  # Now repair the left corners
  mov r4, 3
  #vbitplanes -, r4 SETF
  .half 0xf408
  .half 0xe038
  .half 0x03c4

  btst r6, TopLeftFlag
  beq done_topleft
  vmov H(16,48), H(16,16) IFNZ
done_topleft:
  btst r6, BottomLeftFlag
  beq done_bottomleft
  vmov H(16,48)+r5, H(16,16)+r5 IFNZ
done_bottomleft:

  # And the right corners
  # Prepare flags with nonzero marking right edge
  mov r4, 3
  lsl r4,r4,r3 # offset to reach right edge
  #vbitplanes -, r4 SETF
  .half 0xf408
  .half 0xe038
  .half 0x03c4

  btst r6, TopRightFlag
  beq done_topright
  vmov H(16,48), H(16,16) IFNZ
done_topright:
  btst r6, BottomRightFlag
  beq done_bottomright

  vmov H(16,48)+r5, H(16,16)+r5 IFNZ
done_bottomright:

  mov r1, 128

  # Store processed block
  vstb H(16++,48), (r0 += r1) REP 16
  b lr
.else
###############################################################################
# High bitdepth version of sao_edge                                           #
###############################################################################

# We pass in r3,r4,r5 as the three locations to be examined
sao_edge:
  #vsth HX(16++,0), (r0 += r1) REP 16
  #b lr
  # Extract eoclass
  btst r6, EoClass1
  bne class_23
  btst r6, EoClass0
  bne class_1
class_0:
  mov r4, 48-2+(16-0)*64 # -1,0
  mov r5, 0+2+(16-0)*64  # 1,0
  b got_class
class_1:
  mov r4, 0+(16-1)*64 # 0,-1
  mov r5, 0+(16+1)*64    # 0,1
  b got_class
class_23:
  btst r6, EoClass0
  bne class_3
class_2:
  mov r4, 48-2+(16-1)*64 # -1,-1
  mov r5, 0+2+(16+1)*64 # 1,1
  b got_class
class_3:
  mov r4, 0+2+(16-1)*64 # 1,-1
  mov r5, 48-2+(16+1)*64 # -1,1
got_class:
  mov r6,r3 # Save flags for later

  # Set up loop to process a single 16x16 block
  mov r3, 0
  mov r1,64
  addscale16 r15,r3,r1
sao_edge_loop:
  vmov HX(48,32)+r3, HX(16,0)+r3 # Default values are unchanged
  vsub HX(Vdiff0,0),HX(16,0)+r3,HX(0,0)+r4
  vsgn HX(Vk,0),HX(Vdiff0,0),HX(Vzeros,0) CLRA SACC
  vsub HX(Vdiff1,0),HX(16,0)+r3,HX(0,0)+r5
  vsgn HX(Vk,0),HX(Vdiff1,0),HX(Vzeros,0) SACC SETF
  vadd HX(Vk,0),HX(Vk,0),HX(Vzeros,0) SETF  # Not sure why the flags are not already set correctly, but this instruction seems necessary
  # Now we have a value of:
  # -2 -> 1 -> 0 shift
  # -1 -> 2 -> 1 shift
  # 0 -> 0
  # 1 -> 3 -> 2 shift
  # 2 -> 4 -> 3 shift
  # So add 1, then another 1 if negative
  vadd HX(Vk,0), HX(Vk,0), 1
  vadd HX(Vk,0), HX(Vk,0), 1 IFN

  vmul HX(Vk,0), HX(Vk,0), 6 # Amount to shift
  vshl HY(Voffset,0),HY(VsaoOffsetVal,0), HX(Vk,0)
  vasr HY(Voffset,0),HY(Voffset,0),26

  vadds HX(48,32)+r3, HX(16,0)+r3, HX(Voffset,0) IFNZ
  vmax HX(48,32)+r3,HX(48,32)+r3,0
  vmin HX(48,32)+r3,HX(48,32)+r3,HX(Vsaomax,0)

  add r4,r1
  add r5,r1
  addcmpblt r3,r1,r15,sao_edge_loop

  mov r4, 0x3ffc  # Repair all but corners (done afterwards)
  #vbitplanes -, r4 SETF
  .half 0xf408
  .half 0xe038
  .half 0x03c4

  # Check if we need to repair some edges
  mov r5, r13 # Offset to reach bottom edge
  sub r3, r12 , 1
  min r3, r3, 7 # Offset for right edge
  add r3, r3, r3 # * 2 for U/V offsets
  btst r6, TopFlag # Set if at the top edge, so ne means we need to restore, eq means not
  beq done_top
  vmov HX(48,32), HX(16,0) IFNZ
done_top:
  btst r6, BottomFlag
  beq done_bottom
  vmov HX(48,32)+r5, HX(16,0)+r5 IFNZ
done_bottom:

  mov r4, 0x7ffe  # Repair all but corners (done afterwards)
  #vbitplanes -, r4 SETF
  .half 0xf408
  .half 0xe038
  .half 0x03c4

  btst r6, LeftFlag
  beq done_left
  vadd VX(48,32++), VX(16,0++), 0 REP 2 IFNZ
done_left:
  btst r6, RightFlag
  beq done_right
  vmov VX(48,32++)+r3, VX(16,0++)+r3 REP 2 IFNZ
done_right:

  # Now repair the left corners
  mov r4, 3
  #vbitplanes -, r4 SETF
  .half 0xf408
  .half 0xe038
  .half 0x03c4

  btst r6, TopLeftFlag
  beq done_topleft
  vmov HX(48,32), HX(16,0) IFNZ
done_topleft:
  btst r6, BottomLeftFlag
  beq done_bottomleft
  vmov HX(48,32)+r5, HX(16,0)+r5 IFNZ
done_bottomleft:

  # And the right corners
  # Prepare flags with nonzero marking right edge
  mov r4, 3
  lsl r4,r4,r3 # offset to reach right edge
  #vbitplanes -, r4 SETF
  .half 0xf408
  .half 0xe038
  .half 0x03c4

  btst r6, TopRightFlag
  beq done_topright
  vmov HX(48,32), HX(16,0) IFNZ
done_topright:
  btst r6, BottomRightFlag
  beq done_bottomright

  vmov HX(48,32)+r5, HX(16,0)+r5 IFNZ
done_bottomright:

  mov r1, 128

  # Store processed block
  vsth HX(48++,32), (r0 += r1) REP 16
  b lr
.endif

###############################################################################
# Intra prediction
#
# Chroma intra prediction is either DC/plane or 0/45/90 degrees.
#
# We will have a stream of intra prediction commands along with
# residual commands
#
###############################################################################

# We don't support constrained intra prediction as this complicates where the
# samples can be extended from.

# INITIAL SETUP
# base address
# stride (in bytes)
#
# PER INTRA COMMAND
# numleft(8) numabove(8) code(8) size(8)
# x(16) y(16)
#
# PER RESIDUAL COMMAND
# spare(16) code(8) size(8)
# pred address (32)
# residual address (32)  (Or DC coefficient)
#
# Looks like ref code uses a DC mode in some cases and tries to combine consecutive DC modes together
# Would be better, but for the moment avoid the combination.

# Sizes 4/8/16/32

.equ CODE_INTRA_PLANAR, 0
.equ CODE_INTRA_DC, 1
.equ CODE_INTRA_LEFT, 10
.equ CODE_INTRA_DOWN, 26
.equ CODE_INTRA_DIAGONAL, 34
# Could compress these codes if desired
.equ CODE_RESID_U, 128
.equ CODE_RESID_V, 129
.equ CODE_RESID_DC_U, 130
.equ CODE_RESID_DC_V, 131

# Rows 0..32 used for left context
.set Vxplus1_orig, 33
.set Vxplus1, 34
.set VselectU, 35
.set VselectV, 36
.set Vtemp, 37
.set Vtop, 38 # 4 rows of top samples here
.set Vtop1, 39
.set Vtop4, 42 # another 4 rows of top-right samples here
.set Vleft, 46 # these left samples are used during dc prediction
.set Vleft1, 47
.set Vleft2, 48
.set Vleft3, 49
.set Vmaxvalue, 50
.set Vsize, 51
.set Vtrafosizeplus1, 52
.set Vleftspecial, 53
.set Vtopspecial, 54
.set Vleftcoeff, 55
.set Vleftcoeff2, 56
.set Vtopcoeff, 57
.set Vtopcoeff2, 58

# Share the left/top registers for the general prediction
.set Vacross, 52
.set Vlastleft, 53
.set Vlastabove, 54
.set VintraPredAngle, 55
.set VinvAngle, 56
.set Vflags, 57
.set Vidx, 58
.set Vfact, 59
.set Vref, 60
.set Vref2, 61

# Constant across all intra predictions
.set VuvOffset_14_15, 61
.set VuvOffsetTop_0_1, 62

# Overall loop
# hevc_residual(uint8_t *img (r0), int stride (r1), int numcmds (r2), uint32_t cmds[numcmds] (r3))
#
# r2 size
# r4 code
#
# r12 img
# r13 stride
# r14 pointer to final cmd
# r15 cmds
hevc_residual:
  push r6-r15, lr
  mov r12,r0
  mov r13,r1
  mov r14,r2
  mov r15,r3
  lsl r14,r14,2
  add r14,r14,r15 # Adjust to point to end
  # Prepare some constants in the VRF
  mov r3, (1<<BIT_DEPTH)-1
  # Prepare x plus 1 (pairs due to UV format)
  vmov VX(32,0++),1 REP 2
  vmov VX(32,2++),2 REP 2
  vmov VX(32,4++),3 REP 2
  vmov VX(32,6++),4 REP 2
  vmov VX(32,8++),5 REP 2
  vmov VX(32,10++),6 REP 2
  vmov VX(32,12++),7 REP 2
  vmov VX(32,14++),8 REP 2
  vmov HX(Vmaxvalue,0), r3
  vand -,HX(Vxplus1_orig,0),1 SETF # NZ for U, Z for V
  vmov HX(VselectU,0),0
  vmov HX(VselectV,0),0
  mov r1,0xffff
  vmov HX(VselectU,0),r1 IFNZ
  vmov HX(VselectV,0),r1 IFZ
  vmov HX(VuvOffset_14_15,0),14 IFNZ
  vmov HX(VuvOffset_14_15,0),15 IFZ
  mov r1, Vtop*64
  vmov HX(VuvOffsetTop_0_1,0),r1 IFNZ
  mov r1, Vtop*64 + 1
  vmov HX(VuvOffsetTop_0_1,0),r1 IFZ
  mov r1, 128
residual_cmd_loop:
  ld r0, (r15)
  add r15,4
  lsr r9, r0, 16 # r9: numleft, numabove
  lsr r4,r0,8
#:and r0,r0,255
  .half 0x6e80 #AUTOINSERTED
#:and r4,r4,255
  .half 0x6e84 #AUTOINSERTED
  # TODO could use a switch statement here to reduce number of comparisons?  Or bisect decisions instead?
  cmp r4, CODE_RESID_U
  beq resid_u
  cmp r4, CODE_RESID_V
  beq resid_v
  cmp r4, CODE_RESID_DC_U
  beq resid_dc_u
  cmp r4, CODE_RESID_DC_V
  beq resid_dc_v
  # Now we have an intra command
  # load x,y
# r2 address of block
# r1 x&63
# r9 numleft, numabove
# r10 x
  ld r2, (r15)
  add r15,4
  lsr r10, r2, 16
#:and r2, r2, 0xffff # y
  .half 0x6f02 #AUTOINSERTED
  lsl r2, 7 # Multiply by 128 bytes
  add r2, r12
  lsr r1, r10, 6 # r1 = x/64 = number of tiles
  mul r1, r13
  add r2, r1
#:and r1, r10, 63 # r1 = x&63
  .half 0xc1c1 #AUTOINSERTED
  .half 0x5746 #AUTOINSERTED
  addscale2 r2, r2, r1 # r2 is now the address of the block
  cmp r4, CODE_INTRA_LEFT
  beq straight_across_prediction
  cmp r4, CODE_INTRA_DOWN
  beq straight_down_prediction
  cmp r4, CODE_INTRA_DC
  beq dc_prediction
  cmp r4, CODE_INTRA_PLANAR
  beq planar_prediction
  cmp r4, CODE_INTRA_DIAGONAL
  beq diagonal_prediction
  b diagonal_prediction # TODO this should call general_prediction - but that code is currently broken
  #b general_prediction

next_residual_cmd:
  cmp r15, r14
  blt residual_cmd_loop
  pop r6-r15, pc

# Residual

# r2 prediction pointer
# r1 128 stride
# r0 size
# r3 residual pointer
# r4 residual stride (2*size)
#
# r8 upper bound for loop
# r9 16

# At entry we already have r0 set up, but need to prepare the rest

# Set flags corresponding to appropriate U/V channel and size

resid_v:
  mov r4,0x5555
  b resid_common
resid_u:
  mov r4,0xaaaa # Z marks important places so bits are cleared for U samples
resid_common:
  #vbitplanes -, r4 SETF
  .half 0xf408
  .half 0xe038
  .half 0x03c4

  # Set flags based on the size
  vsub HX(0,0),HX(Vxplus1_orig,0),r0
#:vmax -,HX(0,0),0 IFZ SETF  # Z means x values are important
  .half 0xf4c8 #AUTOINSERTED
  .half 0xe020 #AUTOINSERTED
  .half 0x0540 #AUTOINSERTED
  ld r2, (r15)
  add r15,4
  ld r3, (r15)
  add r15,4
  lsl r4, r0, 1
  add r8, r3, r4
  mov r9, 16

residual_loop:
.if BIT_DEPTH==8
  vmov HX(0++,0),0 REP r0  # TODO could combine this with later operations
  vldb H(0++,0), (r2 += r1) REP r0
.else
  vldh HX(0++,0), (r2 += r1) REP r0
.endif
  vldh HX(0++,32), (r3 += r4) REP r0
  valtl HX(0++,32), HX(0++,32), HX(0++,32) REP r0 # Copy U/V samples into pairs
#:vadd HX(0++,0), HX(0++,0), HX(0++,32) REP r0 IFZ
  .half 0xfd07 #AUTOINSERTED
  .half 0x8020 #AUTOINSERTED
  .half 0x0280 #AUTOINSERTED
  .half 0xfbe0 #AUTOINSERTED
  .half 0x403e #AUTOINSERTED
  vmax HX(0++,0), HX(0++,0), 0 REP r0 IFZ
  vmin HX(0++,0), HX(0++,0), HX(Vmaxvalue,0) REP r0 IFZ
.if BIT_DEPTH==8
  vstb HX(0++,0), (r2 += r1) REP r0 IFZ
  add r2,16
.else
  vsth HX(0++,0), (r2 += r1) REP r0 IFZ
  add r2,32
.endif
  # Now need to repeat for each 8 wide strip
  addcmpblt r3,r9,r8,residual_loop
  b next_residual_cmd

resid_dc_v:
  mov r4,0x5555
  b resid_dc_common
resid_dc_u:
  mov r4,0xaaaa # Z marks important places so bits are cleared for U samples
resid_dc_common:
#:vbitplanes HX(Vtemp,0), r4 SETF
  .half 0xf408 #AUTOINSERTED
  .half 0x8978 #AUTOINSERTED
  .half 0x03c4 #AUTOINSERTED

  # Set flags based on the size
  vsub HX(0,0),HX(Vxplus1_orig,0),r0
#:vmax -,HX(0,0),0 IFZ SETF  # Z means x values are important
  .half 0xf4c8 #AUTOINSERTED
  .half 0xe020 #AUTOINSERTED
  .half 0x0540 #AUTOINSERTED
  ld r2, (r15)
  add r15,4
  ld r3, (r15)
  add r15,4
  lsl r4, r0, 1 # UV pairs
.if BIT_DEPTH==8
  mov r9, 16
.else
  mov r9, 32
  lsl r4, r4, 1 # Each sample is 16bits
.endif
  add r8, r2, r4
  mov r1, 128
residual_dc_loop:
.if BIT_DEPTH==8
  vmov HX(0++,0), 0 REP r0
  vldb H(0++,0), (r2 += r1) REP r0
  # vmov H(0++,0), 128 REP r0 # HACK to default value
.else
  vldh HX(0++,0), (r2 += r1) REP r0
.endif
#:vadd HX(0++,0), HX(0++,0), r3 REP r0 IFZ
  .half 0xfd07 #AUTOINSERTED
  .half 0x8020 #AUTOINSERTED
  .half 0x0380 #AUTOINSERTED
  .half 0xfbe0 #AUTOINSERTED
  .half 0x400c #AUTOINSERTED
  vmax HX(0++,0), HX(0++,0), 0 REP r0 IFZ
  vmin HX(0++,0), HX(0++,0), HX(Vmaxvalue,0) REP r0 IFZ
.if BIT_DEPTH==8
# TODO is it faster to use flags on st, or to only modify c certain parts?
#:vstb H(0++,0), (r2 += r1) REP r0 IFZ
  .half 0xf887 #AUTOINSERTED
  .half 0xe000 #AUTOINSERTED
  .half 0x0380 #AUTOINSERTED
  .half 0x13e0 #AUTOINSERTED
  .half 0x4008 #AUTOINSERTED
.else
#:vsth HX(0++,0), (r2 += r1) REP r0 IFZ
  .half 0xf88f #AUTOINSERTED
  .half 0xe020 #AUTOINSERTED
  .half 0x0380 #AUTOINSERTED
  .half 0x13e0 #AUTOINSERTED
  .half 0x4008 #AUTOINSERTED
.endif
  # Now need to repeat for each 8 wide strip
  addcmpblt r2,r9,r8,residual_dc_loop
  b next_residual_cmd

# This function fetches the above data into HX(Vtop++,0)
fetch_above:
#:and r10, r9, 255 # r10 becomes numabove
  .half 0xc1ca #AUTOINSERTED
  .half 0x4f48 #AUTOINSERTED
  cmp r10,0
  beq no_samples_above
  sub r2,128
  mov r1, 32
.if BIT_DEPTH==8
  vmov HX(Vtop++,0), 0 REP 4
  vldb H(Vtop++,0), (r2+=r1) REP 4  # Slightly overfetching for small blocks here
.else
  vldh HX(Vtop++,0), (r2+=r1) REP 4  # Slightly overfetching for small blocks here
.endif
  # Note that we are not doing constrained, so we will either find all the top samples valid, or none of them
  # TODO what happens at right hand edge of image?
  # TODO could have an early out if extra context not required?
  cmp r10,r0
  bgt can_load_more
  # we need to edge extend from last samples
  addscale2 r5, r2, r0
.if BIT_DEPTH==8
  sub r5, 2
  ldb r4, (r5) # U
  ldb r5,1(r5) # V
.else
  sub r5, 4
  ldh r4, (r5) # U
  ldh r5,2(r5) # V
.endif
  vmov HX(Vtemp,0), r4
  valtl HX(Vtemp,0), HX(Vtemp,0), r5
  # Now figure out which rows to overwrite
  # Size offset num
  # 4    0      1 Special case this as needs flags
  # 8    1      1
  # 16   2      2
  # 32   4      4
  lsr r4,r0,3 # r4 is now the offset in y
  lsl r5,r4,6 # r5 convert to VRF offset
  add r2, 128 # Fix r2 back to pointing at top-left of block
  cmp r4,0
  bne multiplerows
  vmov HX(Vtop,0), HX(Vtemp,0) IFNZ
  b lr
multiplerows:
  mov r6,r0
  mov r0,r4
  vmov HX(Vtop++,0)+r5, HX(Vtemp,0) REP r0
  mov r0,r6
  b lr

can_load_more:
  add r5, r1, r0 # new x position
.if BIT_DEPTH==8
  cmp r5, 64
.else
  cmp r5, 32
.endif
  beq fetch_from_next_tile
  addscale2 r3, r2, r0
  lsr r4,r0,3 # r4 is now the offset in y
  lsl r5,r4,6 # r5 convert to VRF offset
  cmp r4,0
  beq already_loaded
  mov r6,r0
  mov r0,r4
.if BIT_DEPTH==8
  vmov HX(Vtop++,0), 0 REP r0
  vldb H(Vtop++,0)+r5, (r3+=r1) REP r0
.else
  vldh HX(Vtop++,0)+r5, (r3+=r1) REP r0
.endif
  mov r0,r6
already_loaded:
  add r2, 128
  b lr
fetch_from_next_tile:
  addscale2 r3, r2, r0  # Move to end of block
  add r2,128            # fixup r2
  sub r3,128            # wind back tile width
  add r3, r12           # move to next tile
  lsr r4,r0,3 # r4 is now the offset in y
  lsl r5,r4,6 # r5 convert to VRF offset
  cmp r4,0
  bne fetch_multiple_next_tile
  # Block size 4, want to load second 8 samples
.if BIT_DEPTH==8
  sub r3, 8
  vmov HX(Vtemp,0), 0
  vld H(Vtemp,0),(r3) IFNZ
.else
  sub r3, 16
  vldh HX(Vtemp,0),(r3) IFNZ
.endif

  b lr
fetch_multiple_next_tile:
  mov r6,r0
  mov r0,r4
.if BIT_DEPTH==8
  vmov HX(Vtop++,0),0 REP r0
  vldb H(Vtop++,0)+r5, (r3+=r1) REP r0  # TODO this may be overfetching
.else
  vldh HX(Vtop++,0)+r5, (r3+=r1) REP r0  # TODO this may be overfetching
.endif
  mov r0,r6
  b lr

no_samples_above:
  cmp r9,0
  beq no_samples_at_all
  # Need to fetch the left hand samples and replicate
  cmp r1,0
  beq different_tile2
  sub r5,r2,4
  b got_left_pointer
different_tile2:
.if BIT_DEPTH==8
  add r5,r2,128-2
.else
  add r5,r2,128-4
.endif
  sub r5, r5, r13
got_left_pointer:
.if BIT_DEPTH==8
  ldb r4, (r5) # U
  ldb r5, 1(r5) # V
.else
  ldh r4, (r5) # U
  ldh r5, 2(r5) # V
.endif
  vmov HX(Vtemp,0), r4
  valtl HX(Vtop++,0), HX(Vtemp,0), r5 REP 8  # TODO could reduce rep count here based on size
  b lr
no_samples_at_all:
  mov r4, 1<<(BIT_DEPTH-1)
  vmov HX(Vtop++,0), r4 REP 8
  b lr

# Straight down prediction
# First step is to prepare above samples in HX(Vtop++,0)  (note that this covers up to 4 rows for a 32x32 block)
# For use in planar, we also prepare the next 4 rows
straight_down_prediction:
  # Set flags according to size
  vsub HX(0,0),HX(Vxplus1_orig,0),r0
#:vmax -,HX(0,0),0 IFZ SETF  # Z means x values are important
  .half 0xf4c8 #AUTOINSERTED
  .half 0xe020 #AUTOINSERTED
  .half 0x0540 #AUTOINSERTED
  # Fetch top samples
  bl fetch_above
  mov r1, 128
  mov r9, 32
  lsl r4, r0, 1
  add r8, r2, r4
  mov r5, 0 # Offset in VRF
straight_down_loop:
.if BIT_DEPTH==8
  vst H(Vtop,0)+r5, (r2 += r1) REP r0 IFZ
.else
  vsth HX(Vtop,0)+r5, (r2 += r1) REP r0 IFZ
.endif
  add r5, 64
  addcmpblt r2,r9,r8,straight_down_loop
  b next_residual_cmd

# At entry we have:
# r2 address of block
# r1 x&63
# r0 size
# r4 code
#
# r9 numleft, numabove
#
# r12 img
# r13 stride
# r14 numcmds remaining
# r15 cmds
#
# fetch_left will fetch valid samples for -1..r0-1
# however, doesn't do extension if numleft <= size
fetch_left:
  cmp r9, 0xff
  bgt have_some_left
  cmp r9, 0
  beq use_default_value
.if BIT_DEPTH==8
  ldb r4, -128(r2) # U from above
  ldb r5, -127(r2) # V from above
.else
  ldh r4, -128(r2) # U from above
  ldh r5, -126(r2) # V from above
.endif
  vmov HX(0,0), r4
  valtl HX(0++,0), HX(0,0), r5 REP r0
  valtl HX(63,0), HX(0,0), r5
  b lr

use_default_value:
  mov r4, 1<<(BIT_DEPTH-1)
  vmov VX(0,14++), r4 REP 2
  vmov VX(16,14++), r4 REP 2
  vmov HX(63,0), r4
  b lr
have_some_left:
  cmp r1,0
  beq different_tile
  mov r1, 128
.if BIT_DEPTH==8
  sub r4, r2, 16 # In the same tile
  vmov H(0++,0), 0 REP r0
  vldb H(0++,0), (r4 += r1) REP r0
  sub r4, 128
  vmov HX(63,0), 0
  vldb H(63,0), (r4)
.else
  sub r4, r2, 32 # In the same tile
  vldh HX(0++,0), (r4 += r1) REP r0
  sub r4, 128
  vldh HX(63,0), (r4)
.endif
  b lr
different_tile:
.if BIT_DEPTH==8
  add r4, r2, 128-16
  sub r4, r13
  mov r1, 128
  vmov HX(0++,0), 0 REP r0
  vldb H(0++,0), (r4 += r1) REP r0
  sub r4, 128
  vmov HX(63,0), 0
  vldb H(63,0), (r4)
.else
  add r4, r2, 128-32
  sub r4, r13
  mov r1, 128
  vldh HX(0++,0), (r4 += r1) REP r0
  sub r4, 128
  vldh HX(63,0), (r4)
.endif
  b lr

straight_across_prediction:
# Need to look at numleft to decide how to get edge samples
# If 0, then extend from top if present, or default value otherwise

  # Set flags according to size
  vsub HX(0,0),HX(Vxplus1_orig,0),r0
#:vmax -,HX(0,0),0 IFZ SETF  # Z means x values are important
  .half 0xf4c8 #AUTOINSERTED
  .half 0xe020 #AUTOINSERTED
  .half 0x0540 #AUTOINSERTED
  # Fetch samples to left
  bl fetch_left

  # Copy the significant rows across
  vmov VX(0,32++), VX(0,14++) REP 16  # Could reduce this for smaller sizes
  cmp r0,16
  ble prepared_data_across
  vmov VX(16,32++), VX(16,14++) REP 16
prepared_data_across:
# This prediction can now be reused for all stores across
  mov r1, 128
.if BIT_DEPTH==8
  mov r9, 16
  addscale2 r8, r2, r0
.else
  mov r9, 32
  addscale4 r8, r2, r0
.endif
straight_across_prediction_loop:
.if BIT_DEPTH==8
  vstb H(0++,32), (r2 += r1) REP r0 IFZ
.else
  vsth HX(0++,32), (r2 += r1) REP r0 IFZ
.endif
  addcmpblt r2,r9,r8,straight_across_prediction_loop
  b next_residual_cmd


# 45 degree prediction
diagonal_prediction:
  vsub HX(0,0),HX(Vxplus1_orig,0),r0
#:vmax -,HX(0,0),0 IFZ SETF  # Z means x values are important
  .half 0xf4c8 #AUTOINSERTED
  .half 0xe020 #AUTOINSERTED
  .half 0x0540 #AUTOINSERTED
  bl fetch_above

  # Now can work down
  cmp r0, 4
  bne large_blocks
# For 4x4 we can unroll the loop
.if BIT_DEPTH==8
  vstb H(Vtop,0), (r2) IFZ
  add r2, 128
  vstb H(Vtop,2), (r2) IFZ
  add r2, 128
  vstb H(Vtop,4), (r2) IFZ
  add r2, 128
  vstb H(Vtop,6), (r2) IFZ
.else
  vsth HX(Vtop,0), (r2) IFZ
  add r2, 128
  vsth HX(Vtop,2), (r2) IFZ
  add r2, 128
  vsth HX(Vtop,4), (r2) IFZ
  add r2, 128
  vsth HX(Vtop,6), (r2) IFZ
.endif
  b next_residual_cmd

large_blocks:
  # TODO worried about change to r2 when move right?
  b next_residual_cmd # SKIP diagonal for the moment...

  # Work in units of 8 down
  mov r5,0
  lsl r10,r0,6
outer_diagonal_loop:
# r5,r10 counts on in 64s until reach size
  mov r4,r5
  lsl r9,r0,6
  add r9,r4
  mov r6,64
mid_diagonal_loop:
# r4,r9 used as 64*y offset for outer loop
# prepare HX(0,0) and HX(0,32) with appropriate samples for shifted access across top samples
#:vadd HX(0,0), HX(Vtop,0)+r4, 0
  .half 0xf504 #AUTOINSERTED
  .half 0x8026 #AUTOINSERTED
  .half 0x6400 #AUTOINSERTED
#:vadd HX(0,32), HX(Vtop1,0)+r4, 0
  .half 0xf504 #AUTOINSERTED
  .half 0xa026 #AUTOINSERTED
  .half 0x7400 #AUTOINSERTED
# r3,r8 used for inner loop
  mov r3,0
  mov r8,16
diagonal_loop:
.if BIT_DEPTH==8
  vstb H(0,0)+r3, (r2)
.else
  vsth HX(0,0)+r3, (r2)
.endif
  add r2, 128
  addcmpblt r3,2,r8,diagonal_loop
  addcmpblt r4,r6,r9,mid_diagonal_loop
  addcmpblt r5,r6,r10,outer_diagonal_loop
  b next_residual_cmd

# DC prediction
dc_prediction:
  vsub HX(0,0),HX(Vxplus1_orig,0),r0
#:vmax -,HX(0,0),0 IFZ SETF  # Z means x values are important
  .half 0xf4c8 #AUTOINSERTED
  .half 0xe020 #AUTOINSERTED
  .half 0x0540 #AUTOINSERTED
  bl fetch_above
  bl fetch_left
  valtl HX(Vleft,0),VX(0,14),VX(0,15)
  valtu HX(Vleft1,0),VX(0,14),VX(0,15)
  valtl HX(Vleft2,0),VX(16,14),VX(16,15)
  valtu HX(Vleft3,0),VX(16,14),VX(16,15)
  # Size 4 -> 1
  # Size 8 -> 1
  lsr r5,r0,3
  max r5,r5,1
  mov r6,r0
  mov r0,r4
#:vadd HX(0,0),HX(Vleft++,0),HX(Vtop++,0) CLRA UACC REP r0 # TODO make this unsigned somehow?
  .half 0xfd07 #AUTOINSERTED
  .half 0x8022 #AUTOINSERTED
  .half 0xe226 #AUTOINSERTED
  .half 0xf3e0 #AUTOINSERTED
  .half 0x09be #AUTOINSERTED
#:vand -,HX(0,0),HX(VselectU,0) SUM r3 IFZ
  .half 0xfc80 #AUTOINSERTED
  .half 0xe020 #AUTOINSERTED
  .half 0x0223 #AUTOINSERTED
  .half 0xf3c0 #AUTOINSERTED
  .half 0x52fc #AUTOINSERTED
#:vand -,HX(0,0),HX(VselectV,0) SUM r4 IFZ
  .half 0xfc80 #AUTOINSERTED
  .half 0xe020 #AUTOINSERTED
  .half 0x0224 #AUTOINSERTED
  .half 0xf3c0 #AUTOINSERTED
  .half 0x533c #AUTOINSERTED
  mov r0,r6
  lsr r3,r3,r0
  lsr r3,r3,1
  lsr r4,r4,r0
  lsr r4,r4,1
  vand HX(1,0), HX(VselectU,0), r3
  vand HX(2,0), HX(VselectV,0), r4
  vor HX(0,0), HX(1,0), HX(2,0)
  mov r1, 128
.if BIT_DEPTH==8
  mov r9, 16
  addscale2 r8, r2, r0
.else
  mov r9, 32
  addscale4 r8, r2, r0
.endif
dc_loop:
.if BIT_DEPTH==8
  vstb H(0,0), (r2 += r1) REP r0 IFZ
.else
  vsth HX(0,0), (r2 += r1) REP r0 IFZ
.endif
  addcmpblt r2,r9,r8,dc_loop
  b next_residual_cmd


# Planar prediction
# Can use 0x1000 offset to get replicating alias (less useful as we need pairs of values)
# Is there a way to get PPU number out easily?
# Could simply preload or precalculate?

# POS(x, y) = ((size - 1 - x) * left[y] + (x + 1) * top[size]  +
#              (size - 1 - y) * top[x]  + (y + 1) * left[size] + size) >> (trafo_size + 1);


# Replicate left UV pairs over the whole 32x16 block
# r0 size
# r5 y*64
# r6 y+1
planar_prediction:
  # fetch samples and prepare extensions
  vsub HX(0,0),HX(Vxplus1_orig,0),r0
#:vmax -,HX(0,0),0 IFZ SETF  # Z means x values are important
  .half 0xf4c8 #AUTOINSERTED
  .half 0xe020 #AUTOINSERTED
  .half 0x0540 #AUTOINSERTED
  add r0,1 # Need one more sample
  bl fetch_left
  sub r0,1
  bl fetch_above

  lsr r8,r9,8 # r8 is now numleft
  cmp r8,r0 # if greater than size we are already done
  lsl r4,r0,6 # r4 points to left sample
  bgt left_data_extended
  vmov HX(0,0)+r4, HX(63,0)+r4 # copy from samples above
left_data_extended:
  vmov HX(Vsize,0), r0
  # Duplicate out the extra samples using special behaviour
  add r4,0x1000 # Special replicating alias
  vmov HX(Vleftspecial,0),VX(0,14)+r4  # U sample
  valtl HX(Vleftspecial,0),HX(Vleftspecial,0),VX(0,15)+r4 # V sample
  # For top sample we need to split r0 into high,low where low has 3 bits
  # result offset is high*64+low*2
  and r4,r0,7 # r4=low
  lsr r5,r0,3 # r5=high
  lsl r5,6
  addscale2 r4,r5,r4
  add r4,r4,0x1000
  vmov HX(Vtopspecial,0),HX(Vtop,0)+r4  # U sample
  add r4,1
  valtl HX(Vtopspecial,0),HX(Vtopspecial,0),HX(Vtop,0)+r4 # V sample

#:msb r4,r0  # 1->0, 2->1, 4->2
  .half 0x5b04 #AUTOINSERTED
  add r4,1   # log2(trafosize) + 1
  vmov HX(Vtrafosizeplus1,0), r4

  # This loop goes all the way down (up to 32 samples down)
  mov r10,r2 # base address
  mov r4, 0 # shift for accessing different bits of vtop
.if BIT_DEPTH==8
  addscale2 r11,r10,r0 # final base address
.else
  addscale4 r11,r10,r0 # final base address
.endif
  vmov HX(Vxplus1,0), HX(Vxplus1_orig,0)
  vmov VX(0,32++), VX(0,14++)  REP 16
  cmp r0,16
  blt planar_loop_x
  vmov VX(16,32++), VX(16,14++) REP 16
planar_loop_x:
  mov r2,r10  # Restore base address
  mov r5,0
  mov r6,1
planar_loop:
  vsub HX(Vleftcoeff,0),HX(Vsize,0),HX(Vxplus1,0)
  vadd HX(Vtopcoeff2,0),HX(Vxplus1,0),1
  vsub HX(Vtopcoeff,0),HX(Vsize,0),r6
#:vmul -,HX(0,32)+r5,HX(Vleftcoeff,0) CLRA UACC
  .half 0xfd80 #AUTOINSERTED
  .half 0xe028 #AUTOINSERTED
  .half 0x0237 #AUTOINSERTED
  .half 0xf140 #AUTOINSERTED
  .half 0x09bc #AUTOINSERTED
#:vmul -,HX(Vleftspecial,0),r6 UACC
  .half 0xfd80 #AUTOINSERTED
  .half 0xe023 #AUTOINSERTED
  .half 0x5380 #AUTOINSERTED
  .half 0xf3c0 #AUTOINSERTED
  .half 0x0898 #AUTOINSERTED
#:vmul -,HX(Vtop,0)+r4,HX(Vtopcoeff,0) UACC
  .half 0xfd80 #AUTOINSERTED
  .half 0xe022 #AUTOINSERTED
  .half 0x6239 #AUTOINSERTED
  .half 0xf100 #AUTOINSERTED
  .half 0x08bc #AUTOINSERTED

#:vmul -,HX(Vtopspecial,0),HX(Vxplus1,0) UACC
  .half 0xfd80 #AUTOINSERTED
  .half 0xe023 #AUTOINSERTED
  .half 0x6222 #AUTOINSERTED
  .half 0xf3c0 #AUTOINSERTED
  .half 0x08bc #AUTOINSERTED
#:vmov HX(0,0), r0 UACC
  .half 0xfc00 #AUTOINSERTED
  .half 0x8038 #AUTOINSERTED
  .half 0x0380 #AUTOINSERTED
  .half 0xf3c0 #AUTOINSERTED
  .half 0x0880 #AUTOINSERTED
  vlsr HX(0,0),HX(0,0),HX(Vtrafosizeplus1,0)
.if BIT_DEPTH==8
  #vmov HX(0,0), HX(0,32)+r5 # HACK - looks okay
  #vmov HX(0,0), HX(Vtop,0)+r4 # HACK - looks okay
  #vmov HX(0,0), HX(Vtopspecial,0) # HACK - looks okay
  #vmov HX(0,0), HX(Vleftspecial,0) # HACK - looks okay
  #vmov HX(0,0), HX(Vleftcoeff,0) # HACK - looks okay
  #vmov HX(0,0), HX(Vtopcoeff,0) # HACK - looks okay
  #vmov HX(0,0), HX(Vxplus1,0) # HACK - looks okay

  #vmov H(0,0), H(Vtopspecial,16) # BAD!
  vstb H(0,0),(r2) IFZ
.else
  vsth HX(0,0),(r2) IFZ
.endif
  add r2,128
  add r5,64
  addcmpblt r6,1,r0,planar_loop
  # Now move on by 8 pixels
  vadd HX(Vxplus1,0),HX(Vxplus1,0),8
.if BIT_DEPTH==8
  add r10,16
.else
  add r10,32
.endif
  add r4,64
  cmp r10,r11
  blt planar_loop_x
  b next_residual_cmd

#int16 table8dot5[35] = { 0,0,0,0,0, 0,0,0,0,0, // 0->9
#                        0,-4096,-1638,-910,-630,-482,-390,-315,-256,-315,  // 10->19
#                        -390,-482,-630,-910,-1638,-4096,0,0,0,0,
#                        0,0,0,0,0};
# COmputes inv angle
table8dot5:
  .short 0,0,0,0,0, 0,0,0,0,0,0,-4096,-1638,-910,-630,-482,-390,-315,-256,-315,-390,-482,-630,-910,-1638,-4096,0,0,0,0,0,0,0,0,0

# int8 table8dot4[35] = {0,0,32,26,21,17,13,9,5,2,0,-2,-5,-9,-13,-17,-21,-26, -32,-26,-21,-17,-13,-9,-5,-2,0,2,5,9,13,17,21,26,32};
# Computes intra pred angle
table8dot4:
  .byte 0,0,32,26,21,17,13,9,5,2,0,-2,-5,-9,-13,-17,-21,-26, -32,-26,-21,-17,-13,-9,-5,-2,0,2,5,9,13,17,21,26,32
  .balign 4


# r0 size   (setup on entry)
# r2 destination for predicted block (setup on entry)
# r3 direction
# r4 direction (setup on entry destroyed by fetch_above)
# r5 intraPredAngle (from table 8.4)
# r6 invAngle (from table 8.5)
# r9 numleft, numabove
#
# r12 img               (keep)
# r13 stride            (keep)
# r14 numcmds remaining (keep)
# r15 cmds              (keep)
#
# HX(Vtop++,0) contains the above samples
# HX(0++,14:15) contains left samples
# Note that we can never have more than 32 left valid samples
#
# After fetch_above and fetch_left we can bump up the number of available samples to the size
general_prediction:
  mov r3,r4
  # fetch samples and prepare extensions
  vsub HX(0,0),HX(Vxplus1_orig,0),r0
#:vmax -,HX(0,0),0 IFZ SETF  # Z means x values are important
  .half 0xf4c8 #AUTOINSERTED
  .half 0xe020 #AUTOINSERTED
  .half 0x0540 #AUTOINSERTED
  mov r10,r0 # Save original verion of size
  add r0,r0   # Need bottom-left samples as well
#:min r0,32   # There will never be more than 32 that are valid
  .half 0xb220 #AUTOINSERTED
  .half 0x0020 #AUTOINSERTED
  bl fetch_left
  mov r0, r10 # Restore r0=size
  bl fetch_above

  add r10,pc,table8dot4-$     # LIVE(r10)
  add r11,pc,table8dot5-$     # LIVE(r11)
  ldb r5,(r10,r3)             # LIVE(r5) = intraPredAngle   DEAD(r10)
  ldhs r6,(r11,r3)            # LIVE(r6) = invAngle         DEAD(r11)
# Need to prepare a 1d lookup ref
# Or may simply want to do all with calculations - always ends up referencing one of the edge pixels
#
# 1d ref could be a lot more efficient, followed by using shifted pixel access and either V or H destinations
# However, need more than 64 pixels wide so doesn't easily fit.
# Perhaps could fetch 1d ref per 8 pixel chunks?
  cmp r5, 18
  blt angle_less_than_18
  #
  # iIdx = ( ( y + 1 )*intraPredAngle ) >> 5
  # iFact = ( ( y + 1 )*intraPredAngle ) & 31
  # predSamplesIntra[ x ][ y ] = ( ( 32 - iFact )*ref[ x+iIdx+1 ] + iFact*ref[ x+iIdx+2] + 16 ) >> 5
  #
  # ref[x] is p[-1+x][-1] x>=0  (top edge)
  #        or p[-1][ -1+( ( x*invAngle+128 )>>8 ) ] otherwise (left edge)

  vmov HX(VintraPredAngle,0), r5   # DEAD(r5)
  vmov HX(VinvAngle,0), r6         # DEAD(r6)

  asr r5,r9,8  # LIVE(r5) = numleft
  max r5,r0,r5 # fetch_left has ensured we have at least r0 pixels on the left
  sub r5,r5,1  # convert to last valid pixel
  vmov HX(Vlastleft,0), r5 # DEAD(r5)
#:and r5,r9,255
  .half 0xc1c5 #AUTOINSERTED
  .half 0x4f48 #AUTOINSERTED
  max r5,r0,r5
  sub r5,r5,1
  vmov HX(Vlastabove,0), r5

  # Save flags for later use
  vmov HX(Vflags,0), 0 IFZ
  vmov HX(Vflags,0), 1 IFNZ

  # Loop over vertical tiles 8 wide
  vmov HX(Vxplus1,0), HX(Vxplus1_orig,0)
  mov r10,0 # LIVE(r10) = x

xloop_over_18:
  # loop over y
  mov r4,1                         # LIVE(r4) = y+1

yloop_over_18:
  vmul HX(Vidx,0), HX(VintraPredAngle,0),r4   # abs(intraPredAngle)<=32
  vand HX(Vfact,0), HX(Vidx,0), 31
  vasr HX(Vidx,0), HX(Vidx,0), 5
  vadd HX(Vref,0), HX(Vxplus1,0),HX(Vidx,0)
  vadd HX(Vref2,0), HX(Vref,0), 1

  # To convert to reference we need to do different things for x>0 or not
  vsub HX(Vacross,0),HX(Vref,0),1 SETF # IFN means we need to access the left edge pixels
  vmul HX(Vtemp,0),HX(Vref,0),HX(VinvAngle,0)
#:vsub HX(Vtemp,0), HX(Vtemp,0), 128  # Can get >>8 for free by accessing top byte, subtraction of 128 instead of add because we want to be relative to top-left sample
  .half 0xfd20 #AUTOINSERTED
  .half 0x8962 #AUTOINSERTED
  .half 0x5480 #AUTOINSERTED
  .half 0xf3c0 #AUTOINSERTED
  .half 0x0000 #AUTOINSERTED
  vmin HX(Vtemp,0), HX(Vlastleft,0), H(Vtemp,16)  # tmp can equal -1
  # Need to access location (tmp&63)*64 + 14/15 depending on U/V
  vand HX(Vtemp,0), HX(Vtemp,0), 63
  vshl HX(Vtemp,0), HX(Vtemp,0), 6
#:vadd -,HX(Vtemp,0), HX(VuvOffset_14_15,0) CLRA ACC IFN
  .half 0xfd00 #AUTOINSERTED
  .half 0xe022 #AUTOINSERTED
  .half 0x523d #AUTOINSERTED
  .half 0xf3c0 #AUTOINSERTED
  .half 0x8fbc #AUTOINSERTED
  # Now access location (across&7)*2+uvTop+(across>>3)*64
  vmin HX(Vacross,0), HX(Vacross,0), HX(Vlastabove,0)
  vand HX(Vtemp,0), HX(Vacross,7), 7
  vshl HX(Vtemp,0), HX(Vtemp,0), 1
#:vadd -, HX(Vtemp,0), HX(VuvOffsetTop_0_1,0) CLRA ACC IFNN
  .half 0xfd00 #AUTOINSERTED
  .half 0xe022 #AUTOINSERTED
  .half 0x523e #AUTOINSERTED
  .half 0xf3c0 #AUTOINSERTED
  .half 0xafbc #AUTOINSERTED
#:vlsr HX(Vtemp,0), HX(Vtemp,0), 3
  .half 0xf450 #AUTOINSERTED
  .half 0x8962 #AUTOINSERTED
  .half 0x5403 #AUTOINSERTED
#:vshl -, HX(Vtemp,0), 6 ACC IFNN
  .half 0xfc40 #AUTOINSERTED
  .half 0xe022 #AUTOINSERTED
  .half 0x5406 #AUTOINSERTED
  .half 0xf3c0 #AUTOINSERTED
  .half 0xae80 #AUTOINSERTED
# vlookup HX(Vref,0) sadly vlookup was removed after vc02 so this cannot assemble

# Now repeat for ref2
  vsub HX(Vacross,0),HX(Vref2,0),1 SETF # IFN means we need to access the left edge pixels
  vmul HX(Vtemp,0),HX(Vref2,0),HX(VinvAngle,0)
#:vsub HX(Vtemp,0), HX(Vtemp,0), 128  # Can get >>8 for free by accessing top byte, subtraction of 128 instead of add because we want to be relative to top-left sample
  .half 0xfd20 #AUTOINSERTED
  .half 0x8962 #AUTOINSERTED
  .half 0x5480 #AUTOINSERTED
  .half 0xf3c0 #AUTOINSERTED
  .half 0x0000 #AUTOINSERTED
  vmin HX(Vtemp,0), HX(Vlastleft,0), H(Vtemp,16)  # tmp can equal -1
  # Need to access location (tmp&63)*64 + 14/15 depending on U/V
  vand HX(Vtemp,0), HX(Vtemp,0), 63
  vshl HX(Vtemp,0), HX(Vtemp,0), 6
#:vadd -,HX(Vtemp,0), HX(VuvOffset_14_15,0) CLRA ACC IFN
  .half 0xfd00 #AUTOINSERTED
  .half 0xe022 #AUTOINSERTED
  .half 0x523d #AUTOINSERTED
  .half 0xf3c0 #AUTOINSERTED
  .half 0x8fbc #AUTOINSERTED
  # Now access location (across&7)*2+uvTop+(across>>3)*64
  vmin HX(Vacross,0), HX(Vacross,0), HX(Vlastabove,0)
  vand HX(Vtemp,0), HX(Vacross,7), 7
  vshl HX(Vtemp,0), HX(Vtemp,0), 1
#:vadd -, HX(Vtemp,0), HX(VuvOffsetTop_0_1,0) CLRA ACC IFNN
  .half 0xfd00 #AUTOINSERTED
  .half 0xe022 #AUTOINSERTED
  .half 0x523e #AUTOINSERTED
  .half 0xf3c0 #AUTOINSERTED
  .half 0xafbc #AUTOINSERTED
  vlsr HX(Vtemp,0), HX(Vtemp,0), 3
#:vshl -, HX(Vtemp,0), 6 ACC IFNN
  .half 0xfc40 #AUTOINSERTED
  .half 0xe022 #AUTOINSERTED
  .half 0x5406 #AUTOINSERTED
  .half 0xf3c0 #AUTOINSERTED
  .half 0xae80 #AUTOINSERTED
# vlookup HX(Vref2,0)

# Combine
#:vmul -,HX(Vfact,0), HX(Vref2,0) CLRA ACC
  .half 0xfd80 #AUTOINSERTED
  .half 0xe023 #AUTOINSERTED
  .half 0xb23d #AUTOINSERTED
  .half 0xf3c0 #AUTOINSERTED
  .half 0x0fbc #AUTOINSERTED
  vrsub HX(Vfact,0), HX(Vfact,0), 32
#:vmul -,HX(Vfact,0), HX(Vref,0) ACC
  .half 0xfd80 #AUTOINSERTED
  .half 0xe023 #AUTOINSERTED
  .half 0xb23c #AUTOINSERTED
  .half 0xf3c0 #AUTOINSERTED
  .half 0x0ebc #AUTOINSERTED
#:vmov HX(Vtemp,0), 16 ACC
  .half 0xfc00 #AUTOINSERTED
  .half 0x8978 #AUTOINSERTED
  .half 0x0410 #AUTOINSERTED
  .half 0xf3c0 #AUTOINSERTED
  .half 0x0e80 #AUTOINSERTED
  vlsr HX(Vtemp,0), HX(Vtemp,0), 5

  vmov -,HX(Vflags,0) SETF
.if BIT_DEPTH == 8
  vstb H(Vtemp,0), (r2) IFZ
.else
  vsth HX(Vtemp,0), (r2) IFZ
.endif
  add r2, 128
  add r4, 1
  cmp r4, r0
  ble yloop_over_18

  subscale128 r2, r0
.if BIT_DEPTH == 8
  add r2, 16
.else
  add r2, 32
.endif
  vadd HX(Vxplus1,0), HX(Vxplus1,0), 8
  add r10, 8
  cmp r10, r0
  blt xloop_over_18

  b next_residual_cmd

angle_less_than_18:
  # TODO
  b next_residual_cmd
