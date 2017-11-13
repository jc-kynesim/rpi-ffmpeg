# ******************************************************************************
# Argon Design Ltd.
# (c) Copyright 2015 Argon Design Ltd. All rights reserved.
#
# Module : HEVC
# Author : Peter de Rivaz
# ******************************************************************************

# HEVC VPU Transform
#             fe
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
  vldh HX(0++,0)+r0,(r1 += r3) REP 16
  mov r4,64 # Constant used for rounding first pass
  mov r5,TRANS_RND2 # Constant used for rounding second pass

  # At start of block r0,r1 point to the current block (that has already been loaded)
block_loop:
  eor r0,r8
  add r1,r7
  # Prefetch the next block
  vldh HX(0++,0)+r0,(r1 += r3) REP 16
  eor r0,r8
  sub r1,r7

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

  # Now go and do any 32x32 transforms
  b hevc_trans_32x32

  pop r6-r15, pc

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

# hevc_trans_32x32(short *transMatrix2, short *coeffs, int num)
# transMatrix2: address of the constant matrix (must be at 32 byte aligned address in Videocore memory) Even followed by odd
# coeffs: address of the transform coefficients (must be at 32 byte aligned address in Videocore memory)
# num: number of 16x16 transforms to be done
#
hevc_trans_32x32:
  mov r1,r14 # coeffs
  mov r2,r15 # num

  # Fetch odd transform matrix
  #mov r3, 16*2 # Stride of transMatrix2 in bytes (and of coefficients)
  #vldh HX(32++,0),(r0 += r3) REP 16 # This is the even 16x16 matrix
  #add r0, 16*16*2
  #vldh HX(32++,32),(r0 += r3) REP 16 # This is the odd 16x16 matrix

  mov r3, 32*2*2 # Stride used to fetch alternate rows of our input coefficient buffer
  mov r7, 16*16*2 # Total block size
  sub sp,sp,32*32*2+32 # Allocate some space on the stack for us to store 32*32 shorts as temporary results (needs to be aligned)
  # set r8 to 32byte aligned stack pointer
  add r8,sp,31
  lsr r8,5
  lsl r8,5
  mov r9,r8  # Backup of the temporary storage
  mov r10,r1 # Backup of the coefficient buffer
block_loop32:

  # COLUMN TRANSFORM
  mov r4, 64 # Constant used for rounding first pass
  mov r5, 9 # left shift used for rounding first pass

  # Transform the first 16 columns
  mov r1,r10  # Input Coefficient buffer
  mov r8,r9   # Output temporary storage
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

  add r10, 32*32*2 # move onto next block of coefficients
  addcmpbgt r2,-1,0,block_loop32

  add sp,sp,32*32*2+32 # Restore stack

  pop r6-r15, pc

trans32:
  push lr
  # We can no longer afford the VRF space to do prefetching when doing 32x32
  # Fetch the even rows
  vldh HX(0++,0),(r1 += r3) REP 16
  # Fetch the odd rows
  vldh HX(16++,0),64(r1 += r3) REP 16 # First odd row is 32 shorts ahead of r1

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
  vldb H(12++,16)+r3,(r0 += r1) REP 4    # Load the current block
  vldb H(16++,16)+r3,(r2 += r1) REP 16
  vldb H(setup_input,0), (r4)  # We may wish to prefetch these
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
  vldb H(12++,16)+r3,(r0 += r1) REP 4  # load the current block
  vldb H(16++,16)+r3,(r2 += r1) REP 16
  vldb H(setup_input,0), (r4)
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
  vldb H(12++,16)+r3,(r0 += r1) REP 4    # Load the current block
  vldb H(16++,16)+r3,(r2 += r1) REP 16
  vldb H(setup_input,0), (r4)  # We may wish to prefetch these
  vstb H(zeros,0),(r4)
  # Filter the left edge
  bl uv_vert_filter_striped
  b uv_start_deblock_loop_striped
uv_deblock_loop_striped:
  # Middle iterations do vertical on current block and horizontal on preceding
  vldb H(12++,16)+r3,(r0 += r1) REP 4  # load the current block
  vldb H(16++,16)+r3,(r2 += r1) REP 16
  vldb H(setup_input,0), (r4)
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
  vstb H(12++,0)+r3,(r0 += r1) REP 4  # Save the deblocked pixels for the previous block
uv_skip_save_top_striped:
  vstb H(16++,0)+r3,(r2 += r1) REP 16
  add r0,r7
  add r2,r7
uv_start_deblock_loop_striped:
  vadd H(setup_old,0), H(setup_input,8),0 # Save setup for use for horizontal edges
  # move onto next 16x16 (could do this with circular buffer support instead)
  add r3,16
  and r3,r8
  add r4,16
  # Perform loop counter operations (may work with an addcmpbgt as well?)
  add r15,1 # When we reach 8 we need to switch r7
  mov r7, 16 # Amount to move on
  and r5,r15,7
  cmp r5,0
  bne normal_case
  add r7,r14,16-128 # move back to left edge, then move on one stripe
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
  vstb H(12++,0)+r3,(r0 += r1) REP 4  # Save the deblocked pixels for the last block
uv_skip_save_top2_striped:
  vstb H(16++,0)+r3,(r2 += r1) REP 16

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
  vmov HX(P1,0), V(16,12)+r3
  vmov HX(P0,0), V(16,14)+r3
  vmov HX(Q0,0), V(16,16)+r3
  vmov HX(Q1,0), V(16,18)+r3

  valtl H(setup,0),H(setup_input,0),H(setup_input,0) # tc*8
  valtl HX(tc,0),H(setup,0),H(setup,0)
  bl do_chroma_filter_with_tc

  vadds V(16,14)+r3, HX(P0,0), 0
  vadds V(16,16)+r3, HX(Q0,0), 0
  
  # V filtering
  vmov HX(P1,0), V(16,13)+r3
  vmov HX(P0,0), V(16,15)+r3
  vmov HX(Q0,0), V(16,17)+r3
  vmov HX(Q1,0), V(16,19)+r3

  vadd  H(setup,0),H(setup_input,4),0 # Shift to V samples
  valtl H(setup,0),H(setup,0),H(setup,0)
  valtl HX(tc,0),H(setup,0),H(setup,0)
  bl do_chroma_filter_with_tc

  vadds V(16,15)+r3, HX(P0,0), 0
  vadds V(16,17)+r3, HX(Q0,0), 0

  pop pc

# Filter edge at H(16,0)+r3
# weights in setup_old: u0u1v0v1
uv_horz_filter_striped:
  push lr

  vmov HX(P1,0), H(14,0)+r3
  vmov HX(P0,0), H(15,0)+r3
  vmov HX(Q0,0), H(16,0)+r3
  vmov HX(Q1,0), H(17,0)+r3

  valtl H(setup,0),H(setup_old,0),H(setup_old,0) # tc*8
  valtl H(setup,0),H(setup,0),H(setup,0) # u0u0u0u0u1u1u1u1v0v0v0v0v1v1v1v1
  vadd H(tc,0),H(setup,8),0 # Could remove this if we store u/v in alternate order (issue is only ra port has shifted access)
  valtl HX(tc,0),H(setup,0),H(tc,0) # u0v0u1v1...
  bl do_chroma_filter_with_tc

  vadds H(15,0)+r3, HX(P0,0), 0
  # P3 and Q3 never change so don't bother saving back
  vadds H(16,0)+r3, HX(Q0,0), 0
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
# r6 sao data [typeidx(2) spare(4) eoclass(2) spare(3) leftclass(5) saooffsetval(16)] for typeidx=BAND (eoclass unused)
#             [typeidx(2) spare(4) eoclass(2) flags(8) saooffsetval(16)] for typeidx=EDGE
#
# Followed by another word for V.
#
# sao values packed 4 bits each, LSBits for offset[4].
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
.set Vbottomrightcontext, 32
.set Vk, 48
.set Voffset, 49
.set VsaoOffsetVal, 50
.set Vdiff0, 51
.set Vdiff1, 52
.set Vband, 53
.set Vzeros, 54
.set Vtemp, 55
.set VLeftClass, 56

.set LeftFlag, 16
.set TopFlag, 17
.set RightFlag, 18
.set BottomFlag, 19

.set TopLeftFlag, 20
.set TopRightFlag, 21
.set BottomLeftFlag, 22
.set BottomRightFlag, 23

.set EoClass0, 24
.set EoClass1, 25
.set LeftClass, 16

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
  mov r14, 16 # Amount to move onto next block
  mov r1, 128
  vmov HX(Vzeros,0),0
  
  # Prepare by loading the first block
  vldb H(15,32), (r2)
  addscale16 r6, r0, r1
  vldb H(16++,32), (r0 += r1) REP 16
  vldb H(32,32), (r6)
  
  # We have no need to store horizontal context as we always do an entire 16 high row each time.
  
  # However, we may need diagonals and right pixels so fetch the block data one ahead
  
sao_block_loop:
  # At start of loop:
  #  r0 points to current block (that we will need to save)
  #  r5 gives offset to next block
  #  r8 gives index (in units of 8x16) of current block

  # Load data into H(16,16) -> transform into H(16,48)

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

  # Load data: 
  ld r6, (r11)
  ld r3, 4(r11)
  
  # Move the offset values into the VRF
  vmov HX(VsaoOffsetVal,0), r6
  valtl HX(VsaoOffsetVal,0),HX(VsaoOffsetVal,0), r3 # Alternate u with v offsets
  
  # Process and save block
  bl sao_block
  
  # Save upper context
  vstb H(31,16),(r2)
  
  # Move onto next block
  add r0, r14
  
  # Prepare r5 to move to the following block
  add r8,1 # When we reach 8 we need to switch r7
  mov r14,16 # Amount to move on
  and r1,r8,7
  cmp r1,7
  bne normal_case_sao
  add r14,r9,16-128 # move back to left edge, then move on one stripe
normal_case_sao:
  
  # Update loop variables
  add r2, 16

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
  vmov H(16,48)+r3, H(16,16)+r3  # Set up default values
  vasr HX(Vband,0),H(16,16)+r3,3
  # bandIdx = bandTable[band]
  # bandTable[(k+leftClass)&31] = k+1 . [0<=k<4]
  # (k+leftClass)&31 = band
  # (k+leftClass) = band (mod 32)
  # k = band - leftClass (mod 32)
  vsub HX(Vk,0), HX(Vband,0), H(VLeftClass,0)
  vand HX(Vk,0), HX(Vk,0), 31 SETF
  # WARNING the assembler seems to ignore SETF if we use an immediate instead of HX(Vtemp,0)
  vsub -, HX(Vk,0), HX(Vtemp,0) SETF  # Flags are N if k in legal range
  vshl HX(Vk,0), HX(Vk,0), 2 # Amount to shift
  vshl HX(Voffset,0),HX(VsaoOffsetVal,0), HX(Vk,0)
  vasr HX(Voffset,0),HX(Voffset,0),12
  vadds H(16,48)+r3, H(16,16)+r3, HX(Voffset,0) IFN
  addcmpblt r3,r1,r15,sao_band_loop
  subscale16 r3,r15,r1 # Put r3 back again
  
  # Store processed block
  mov r1, 128
  vstb H(16++,48), (r0 += r1) REP 16
  b lr
  
  
sao_copy:

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
  
  vshl HX(Vk,0), HX(Vk,0), 2 # Amount to shift
  vshl HX(Voffset,0),HX(VsaoOffsetVal,0), HX(Vk,0)
  vasr HX(Voffset,0),HX(Voffset,0),12
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
