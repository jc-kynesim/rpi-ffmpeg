# ******************************************************************************
# Argon Design Ltd.
# (c) Copyright 2015 Argon Design Ltd. All rights reserved.
#
# Module : HEVC
# Author : Peter de Rivaz
# ******************************************************************************

# USE_STACK = 1 means temporary data stored on the stack (requires build with larger stack)
# USE_STACK = 0 means temporary data stored in fixed per-VPU data buffers (requires modifications to vasm to handle instruction encoding for PC relative instructions)
.set USE_STACK, 0

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
  extu r2,16
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
  cmp r2,0
  beq done16x16s
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
done16x16s:

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
  extu r4,8
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
  extu r4,10
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
  extu r2,16 # Total number

  # Fetch odd transform matrix
  #mov r3, 16*2 # Stride of transMatrix2 in bytes (and of coefficients)
  #vldh HX(32++,0),(r0 += r3) REP 16 # This is the even 16x16 matrix
  #add r0, 16*16*2
  #vldh HX(32++,32),(r0 += r3) REP 16 # This is the odd 16x16 matrix

  mov r3, 32*2*2 # Stride used to fetch alternate rows of our input coefficient buffer
  mov r7, 16*16*2 # Total block size

.if USE_STACK
  # Stack base allocation
  sub sp,sp,32*32*4+64 # Allocate some space on the stack for us to store 32*32 shorts as temporary results (needs to be aligned) and another 32*32 for unpacking
  # set r8 to 32byte aligned stack pointer with 32 bytes of space before it
  add r8,sp,63
  lsr r8,5
  lsl r8,5
.else
#:version r8
  .half 0x00e8 #AUTOINSERTED
  btst r8,16
#:add r8,pc,intermediate_results-$
  .half 0xbfe8
  .half intermediate_results-($-2)
  beq on_vpu1
  add r8,r8,32*32*2*2+16*2 # Move to secondary storage
on_vpu1:
.endif
  mov r9,r8  # Backup of the temporary storage
  mov r10,r1 # Backup of the coefficient buffer

  cmp r2,0
  beq done32x32s
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

  add r10, 32*32*2 # move onto next block of coefficients
  addcmpbgt r2,-1,0,block_loop32
done32x32s:

.if USE_STACK
  add sp,sp,32*32*4+64# Restore stack
.endif

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

.if USE_STACK == 0
  .balign 32

# .space directives generate 0's in the bin so avoid unnecessary padding by
# just setting to appropriate value
.equ intermediate_results, $+16*2

# Layout goes:
#
#packed_buffer:
#  .space 16*2
#intermediate_results:
#  .space 32*32*2
#unpacked_buffer:
#  .space 32*32*2
#
#packed_buffer2:
#  .space 16*2
#intermediate_results2:
#  .space 32*32*2
#unpacked_buffer2:
#  .space 32*32*2
.endif


