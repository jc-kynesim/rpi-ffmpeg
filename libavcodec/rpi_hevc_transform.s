# ******************************************************************************
# Argon Design Ltd.
# (c) Copyright 2015 Argon Design Ltd. All rights reserved.
#
# Module : HEVC
# Author : Peter de Rivaz
# ******************************************************************************

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
#
hevc_trans_16x16:
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
  mov r5,1<<11 # Constant used for rounding second pass

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
  vasl HY(0++,0)+r0,HY(0++,0)+r0,4 REP 16    # This should be saturating, but the instruction above does not assemble?  (Probably because it ends with ls which is interpreted as a condition flag)

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
  mov r4, 1<<11 # Constant used for rounding second pass
  mov r5, 4 # left shift used for rounding second pass

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
