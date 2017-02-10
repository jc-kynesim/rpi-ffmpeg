#!/usr/bin/env python

#    add.ifz.setf  -, r0, ra0 ; fmul  rb1, rany2, 0 ; thrend # comment
#    add  r0, r0, 1                    # implicit mul nop
#    nop                               # explicit add nop, implicit mul nop
#    bkpt                              # implicit add/mul nop
#    mov  r0, 0x1234                   # hex immediate
#    mov  r0, 20 * 40                  # expressions...
#    mov  r0, f(sqrt(2.0) * 3.0)       # f() converts float to bits
#    mov  r0, a:label                  # put address of label in r0
# :label
#    bra.allnn  ra2, a:1f              # branch to label 1 (searching forward), using absolute address
# :1
#    brr.anyz  -, r:1b                 # branch to label 1 (searching backward), using relative address
# :1                                   # multiple definitions of numeric labels (differentiated using f/b)
# .set my_val, 3                       # introduce alias for 3
# .set my_reg, r0                      # and for r0
#    mov  my_reg, my_val               # then use them
# .set my_reg2, my_reg + my_val        # r0 plus 3 is r3
# .macro my_add, a, b, c               # a, b, c act as if .set on entry
# .set my_val, 10
#    add  a, b, c
#    mov  r0, my_val                   # 10
# .endm                                # forget all .sets since .macro (including arg .sets)
#    mov  r0, my_val                   # 3
#    my_add  my_reg2, my_reg, ra0 << 4 # << rotates left (>> rotates right)

import math
import optparse
import os
import random
import re
import struct
import sys
import time

###############################################################################
# constants
###############################################################################

# ops
######

# negatives are internal qasm ops

AOP_MOV     = -3   # two operands
AOP_BRA     = -2   # two operands
AOP_BRR     = -1   # two operands
AOP_NOP     = 0x00 # no operands
AOP_FADD    = 0x01
AOP_FSUB    = 0x02
AOP_FMIN    = 0x03
AOP_FMAX    = 0x04
AOP_FMINABS = 0x05
AOP_FMAXABS = 0x06
AOP_FTOI    = 0x07 # two operands
AOP_ITOF    = 0x08 # two operands
AOP_ADD     = 0x0c
AOP_SUB     = 0x0d
AOP_SHR     = 0x0e
AOP_ASR     = 0x0f
AOP_ROR     = 0x10
AOP_SHL     = 0x11
AOP_MIN     = 0x12
AOP_MAX     = 0x13
AOP_AND     = 0x14
AOP_OR      = 0x15
AOP_XOR     = 0x16
AOP_NOT     = 0x17 # two operands
AOP_CLZ     = 0x18 # two operands
AOP_V8ADDS  = 0x1e
AOP_V8SUBS  = 0x1f

MOP_MOV    = -1  # two operands
MOP_NOP    = 0x0 # no operands
MOP_FMUL   = 0x1
MOP_MUL24  = 0x2
MOP_V8MULD = 0x3
MOP_V8MIN  = 0x4
MOP_V8MAX  = 0x5
MOP_V8ADDS = 0x6
MOP_V8SUBS = 0x7

# ldi modes
############

LDI_32          = 0
LDI_EL_SIGNED   = 1
LDI_EL_UNSIGNED = 3
LDI_SEMA        = 4

# conds
########

COND_NEVER  = 0
COND_ALWAYS = 1
COND_IFZ    = 2
COND_IFNZ   = 3
COND_IFN    = 4
COND_IFNN   = 5
COND_IFC    = 6
COND_IFNC   = 7

BCOND_ALLZ   = 0
BCOND_ALLNZ  = 1
BCOND_ANYZ   = 2
BCOND_ANYNZ  = 3
BCOND_ALLN   = 4
BCOND_ALLNN  = 5
BCOND_ANYN   = 6
BCOND_ANYNN  = 7
BCOND_ALLC   = 8
BCOND_ALLNC  = 9
BCOND_ANYC   = 10
BCOND_ANYNC  = 11
BCOND_ALWAYS = 15

# packing/unpacking
####################

# regfile a pack modes
PACK_A_NOP   = 0
PACK_A_16A   = 1
PACK_A_16B   = 2
PACK_A_8888  = 3
PACK_A_8A    = 4
PACK_A_8B    = 5
PACK_A_8C    = 6
PACK_A_8D    = 7
PACK_A_32S   = 8
PACK_A_16AS  = 9
PACK_A_16BS  = 10
PACK_A_8888S = 11
PACK_A_8AS   = 12
PACK_A_8BS   = 13
PACK_A_8CS   = 14
PACK_A_8DS   = 15

# mul unit pack modes
PACK_MUL_NOP  = 0
PACK_MUL_8888 = 3
PACK_MUL_8A   = 4
PACK_MUL_8B   = 5
PACK_MUL_8C   = 6
PACK_MUL_8D   = 7

# regfile a unpack modes
UNPACK_A_NOP = 0
UNPACK_A_16A = 1
UNPACK_A_16B = 2
UNPACK_A_8R  = 3
UNPACK_A_8A  = 4
UNPACK_A_8B  = 5
UNPACK_A_8C  = 6
UNPACK_A_8D  = 7

# r4 unpack modes
UNPACK_R4_NOP = 0
UNPACK_R4_16A = 1
UNPACK_R4_16B = 2
UNPACK_R4_8R  = 3
UNPACK_R4_8A  = 4
UNPACK_R4_8B  = 5
UNPACK_R4_8C  = 6
UNPACK_R4_8D  = 7

PACK_TYPE_INT    = 0
PACK_TYPE_FLOAT  = 1
PACK_TYPE_EITHER = -1

PACK_MODE_A      = 0 # regfile a
PACK_MODE_M      = 1 # mul unit
PACK_MODE_EITHER = -1

UNPACK_LOC_A     = 0 # regfile a
UNPACK_LOC_R4    = 1 # r4
UNPACK_LOC_AB    = 2 # either regfile a or regfile b
UNPACK_LOC_OTHER = 3 # somewhere else

# args
#######

# loc_t, ie internal
MUX_AC  = 0
MUX_ANY = 1
MUX_A   = 2
MUX_B   = 3
RW_EITHER = 0
RW_READ   = 1
RW_WRITE  = 2

RADDR_NOP = 39

# negatives are for internal use
RMUX_SEMA  = -6
RMUX_LABEL = -5
RMUX_IMMV  = -4
RMUX_IMM   = -3
RMUX_AC    = -2
RMUX_ANY   = -1
RMUX_A0    = 0 # followed by A1, A2, A3, A4, A5
RMUX_A     = 6
RMUX_B     = 7

WADDR_R0  = 32 # followed by R1, R2, R3
WADDR_NOP = 39

WMUX_ANY = 0
WMUX_A   = 1
WMUX_B   = 2

# signals
##########

SIG_BKPT       = 0
SIG_NORMAL     = 1
SIG_THRSW      = 2
SIG_THREND     = 3
SIG_SBWAIT     = 4
SIG_SBDONE     = 5
SIG_INT        = 6 # on a0
SIG_LTHRSW     = 6 # on b0
SIG_LOADCV     = 7
SIG_LOADC      = 8
SIG_LDCEND     = 9
SIG_LDTMU0     = 10
SIG_LDTMU1     = 11
SIG_ROTATE     = 12 # on a0
SIG_LOADAM     = 12 # on b0
SIG_SMALLIMMED = 13
SIG_IMMED      = 14
SIG_BRANCH     = 15

# multi-line assembler constructs
##################################

CONSTRUCT_MACRO = 0x1
CONSTRUCT_IF    = 0x2
CONSTRUCT_ELSE  = 0x4
CONSTRUCT_REP   = 0x8

###############################################################################
# helpers
###############################################################################

def asm_error(message, location = None):
   if location is None:
      location = current_location
   if location == '':
      sys.stderr.write('qasm ERROR: %s\n' % message)
   else:
      sys.stderr.write('qasm ERROR: %s: %s\n' % (location, message))
   sys.exit(-1)

def asm_warning(message, location = None):
   if disable_warnings or (nwarn_level != 0):
      return
   if location is None:
      location = current_location
   if location == '':
      sys.stderr.write('qasm WARNING: %s\n' % message)
   else:
      sys.stderr.write('qasm WARNING: %s: %s\n' % (location, message))
   if warnings_are_errors:
      asm_error('warnings are errors!', location)

# smart_split('') = []
# smart_split('a') = ['a']
# smart_split('a(1, 2),[3, 4, 5],6') = ['a(1, 2)', '[3, 4, 5]', '6']
def smart_split(s, delim = ',', count = 0):
   if len(s) == 0:
      return []
   parts = []
   depth = 0
   i = 0
   for j in xrange(len(s)):
      if s[j] in '([{':
         depth += 1
      elif s[j] in ')]}':
         depth -= 1
      elif (s[j] == delim) and (depth == 0):
         parts.append(s[i:j])
         i = j + 1
         if len(parts) == count:
            break
   if depth != 0:
      asm_error('bracket nesting fail')
   parts.append(s[i:])
   return parts

def is_int(x):
   return isinstance(x, int) or isinstance(x, long)

###############################################################################
# "parsing" stuff
###############################################################################

re_macro = re.compile('\\.macro\\s+(?P<name>\\w+)(?P<params>(\\s*,\\s*\\w+)*)$')
re_if = re.compile('\\.if((?P<set>n?set)\\s+(?P<name>\\w+)|\\s(?P<condition>.+))$')
re_elif = re.compile('\\.elif((?P<set>n?set)\\s+(?P<name>\\w+)|\\s(?P<condition>.+))$')
re_rep = re.compile('\\.rep\\s+(?P<name>\\w+)\\s*,(?P<count>.+)$')
re_include = re.compile('\\.include\\s(?P<filename>.+)$')
re_set = re.compile('\\.set\\s+(?P<name>\\w+)\\s*,(?P<val>.+)$')
re_unset = re.compile('\\.unset\\s+(?P<name>\\w+)$')
re_eval = re.compile('\\.eval\\s(?P<expr>.+)$')
re_print_info_warn_error = re.compile('\\.(?P<print_info_warn_error>print|info|warn|error)\\s(?P<message>.+)$')
re_assert = re.compile('\\.assert\\s(?P<condition>.+)$')
re_data = re.compile('\\.d(?P<size>[124])\\s(?P<data>.+)$')
re_macro_inst = re.compile('(?P<name>\\w+)(?P<args>\\s.+|)$')
re_label = re.compile(':(?P<name>:?[a-zA-Z_]\\w*|\\d+)$')
re_op = re.compile('(?P<op>\\w+)(\\.(?P<cond>\\w+))??(\\.(?P<sf>setf))?(?P<args>\\s.+|)$')
re_label_ref_left = re.compile('\\b([ar]):')
re_label_ref_right = re.compile('[a-zA-Z_]\\w*|\\d+[bf]$')
re_pack = re.compile('\\.([0-9]\\w*[a-df-zA-DF-Z_])') # a bit weird because we don't want to pick up float literals...

# ops
######

aops = {
   'mov': (AOP_MOV, 2),
   'bra': (AOP_BRA, 2),
   'brr': (AOP_BRR, 2),
   'nop': (AOP_NOP, 0),
   'fadd': (AOP_FADD, 3),
   'fsub': (AOP_FSUB, 3),
   'fmin': (AOP_FMIN, 3),
   'fmax': (AOP_FMAX, 3),
   'fminabs': (AOP_FMINABS, 3),
   'fmaxabs': (AOP_FMAXABS, 3),
   'ftoi': (AOP_FTOI, 2),
   'itof': (AOP_ITOF, 2),
   'add': (AOP_ADD, 3),
   'sub': (AOP_SUB, 3),
   'shr': (AOP_SHR, 3),
   'asr': (AOP_ASR, 3),
   'ror': (AOP_ROR, 3),
   'shl': (AOP_SHL, 3),
   'min': (AOP_MIN, 3),
   'max': (AOP_MAX, 3),
   'and': (AOP_AND, 3),
   'or': (AOP_OR, 3),
   'xor': (AOP_XOR, 3),
   'not': (AOP_NOT, 2),
   'clz': (AOP_CLZ, 2),
   'v8adds': (AOP_V8ADDS, 3),
   'v8subs': (AOP_V8SUBS, 3)}

def get_aop(aop):
   if aop not in aops:
      asm_error('invalid aop')
   return aops[aop]

mops = {
   'mov': (MOP_MOV, 2),
   'nop': (MOP_NOP, 0),
   'fmul': (MOP_FMUL, 3),
   'mul24': (MOP_MUL24, 3),
   'v8muld': (MOP_V8MULD, 3),
   'v8min': (MOP_V8MIN, 3),
   'v8max': (MOP_V8MAX, 3),
   'v8adds': (MOP_V8ADDS, 3),
   'v8subs': (MOP_V8SUBS, 3)}

def get_mop(mop):
   if mop not in mops:
      asm_error('invalid mop')
   return mops[mop]

# conds
########

conds = {
   'ifz': COND_IFZ,
   'ifnz': COND_IFNZ,
   'ifn': COND_IFN,
   'ifnn': COND_IFNN,
   'ifc': COND_IFC,
   'ifnc': COND_IFNC}

def get_cond(cond):
   if not cond:
      return COND_ALWAYS
   if cond not in conds:
      asm_error('invalid cond')
   return conds[cond]

bconds = {
   'allz': BCOND_ALLZ,
   'allnz': BCOND_ALLNZ,
   'anyz': BCOND_ANYZ,
   'anynz': BCOND_ANYNZ,
   'alln': BCOND_ALLN,
   'allnn': BCOND_ALLNN,
   'anyn': BCOND_ANYN,
   'anynn': BCOND_ANYNN,
   'allc': BCOND_ALLC,
   'allnc': BCOND_ALLNC,
   'anyc': BCOND_ANYC,
   'anync': BCOND_ANYNC}

def get_bcond(bcond):
   if not bcond:
      return BCOND_ALWAYS
   if bcond not in bconds:
      asm_error('invalid bcond')
   return bconds[bcond]

def get_setf(setf):
   if not setf:
      return False
   return True

# packing/unpacking
####################

packs = {
   '16a':    (PACK_A_16A,    PACK_TYPE_INT,    PACK_MODE_A),
   '16b':    (PACK_A_16B,    PACK_TYPE_INT,    PACK_MODE_A),
   '16af':   (PACK_A_16A,    PACK_TYPE_FLOAT,  PACK_MODE_A),
   '16bf':   (PACK_A_16B,    PACK_TYPE_FLOAT,  PACK_MODE_A),
   '8abcd':  (PACK_A_8888,   PACK_TYPE_EITHER, PACK_MODE_A),
   '8a':     (PACK_A_8A,     PACK_TYPE_EITHER, PACK_MODE_A),
   '8b':     (PACK_A_8B,     PACK_TYPE_EITHER, PACK_MODE_A),
   '8c':     (PACK_A_8C,     PACK_TYPE_EITHER, PACK_MODE_A),
   '8d':     (PACK_A_8D,     PACK_TYPE_EITHER, PACK_MODE_A),
   's':      (PACK_A_32S,    PACK_TYPE_EITHER, PACK_MODE_A),
   '16as':   (PACK_A_16AS,   PACK_TYPE_EITHER, PACK_MODE_A),
   '16bs':   (PACK_A_16BS,   PACK_TYPE_EITHER, PACK_MODE_A),
   '8abcds': (PACK_A_8888S,  PACK_TYPE_EITHER, PACK_MODE_A),
   '8as':    (PACK_A_8AS,    PACK_TYPE_EITHER, PACK_MODE_A),
   '8bs':    (PACK_A_8BS,    PACK_TYPE_EITHER, PACK_MODE_A),
   '8cs':    (PACK_A_8CS,    PACK_TYPE_EITHER, PACK_MODE_A),
   '8ds':    (PACK_A_8DS,    PACK_TYPE_EITHER, PACK_MODE_A),
   '8abcdc': (PACK_MUL_8888, PACK_TYPE_EITHER, PACK_MODE_M),
   '8ac':    (PACK_MUL_8A,   PACK_TYPE_EITHER, PACK_MODE_M),
   '8bc':    (PACK_MUL_8B,   PACK_TYPE_EITHER, PACK_MODE_M),
   '8cc':    (PACK_MUL_8C,   PACK_TYPE_EITHER, PACK_MODE_M),
   '8dc':    (PACK_MUL_8D,   PACK_TYPE_EITHER, PACK_MODE_M)}

def get_pack(pack):
   if not pack:
      return (0, PACK_TYPE_EITHER, PACK_MODE_EITHER)
   if pack not in packs:
      asm_error('invalid pack')
   return packs[pack]

a_unpacks = {
   '16a':  (UNPACK_A_16A, PACK_TYPE_INT),
   '16b':  (UNPACK_A_16B, PACK_TYPE_INT),
   '16af': (UNPACK_A_16A, PACK_TYPE_FLOAT),
   '16bf': (UNPACK_A_16B, PACK_TYPE_FLOAT),
   '8dr':  (UNPACK_A_8R,  PACK_TYPE_EITHER),
   '8a':   (UNPACK_A_8A,  PACK_TYPE_INT),
   '8b':   (UNPACK_A_8B,  PACK_TYPE_INT),
   '8c':   (UNPACK_A_8C,  PACK_TYPE_INT),
   '8d':   (UNPACK_A_8D,  PACK_TYPE_INT),
   '8ac':  (UNPACK_A_8A,  PACK_TYPE_FLOAT),
   '8bc':  (UNPACK_A_8B,  PACK_TYPE_FLOAT),
   '8cc':  (UNPACK_A_8C,  PACK_TYPE_FLOAT),
   '8dc':  (UNPACK_A_8D,  PACK_TYPE_FLOAT)}

def get_a_unpack(unpack):
   if not unpack:
      return (UNPACK_A_NOP, PACK_TYPE_EITHER, UNPACK_LOC_A)
   if unpack not in a_unpacks:
      asm_error('invalid ra unpack')
   return a_unpacks[unpack] + (UNPACK_LOC_A,)

r4_unpacks = {
   '16af': UNPACK_R4_16A,
   '16bf': UNPACK_R4_16B,
   '8dr':  UNPACK_R4_8R,
   '8ac':  UNPACK_R4_8A,
   '8bc':  UNPACK_R4_8B,
   '8cc':  UNPACK_R4_8C,
   '8dc':  UNPACK_R4_8D}

def get_r4_unpack(unpack):
   if not unpack:
      return (UNPACK_R4_NOP, PACK_TYPE_EITHER, UNPACK_LOC_R4)
   if unpack not in r4_unpacks:
      asm_error('invalid r4 unpack')
   return (r4_unpacks[unpack], PACK_TYPE_EITHER, UNPACK_LOC_R4)

# args
#######

class loc_t:
   def __init__(self, mux, i, rot, r5_rot, pack, rw):
      self.mux = mux
      self.i = i
      self.rot = rot % 16
      self.r5_rot = r5_rot % 16
      self.pack = pack
      self.rw = rw

   def copy(self):
      return loc_t(self.mux, self.i, self.rot, self.r5_rot, self.pack, self.rw)

   def __add__(self, i):
      if not is_int(i):
         raise Exception('can only add integer to loc')
      return loc_t(self.mux, self.i + i, self.rot, self.r5_rot, self.pack, self.rw)

   def __sub__(self, i):
      if not is_int(i):
         raise Exception('can only subtract integer from loc')
      return loc_t(self.mux, self.i - i, self.rot, self.r5_rot, self.pack, self.rw)

   def __cmp__(self, other):
      if is_int(other):
         return cmp(self.i, other)
      if not isinstance(other, loc_t):
         raise Exception('can only compare loc to integer or other loc')
      if self.mux != other.mux:
         return cmp(self.mux, other.mux)
      if self.i != other.i:
         return cmp(self.i, other.i)
      if self.rot != other.rot:
         return cmp(self.rot, other.rot)
      if self.r5_rot != other.r5_rot:
         return cmp(self.r5_rot, other.r5_rot)
      return cmp(self.pack, other.pack)

   def is_r5(self):
      return (self.mux == MUX_AC) and (self.i == 5)

   def shift(self, rot, left):
      if isinstance(rot, loc_t) and rot.is_r5():
         if (rot.rot != 0) or (rot.r5_rot != 0) or rot.pack:
            raise Exception('can\'t rotate by rotated/unpacked r5')
         return loc_t(self.mux, self.i, self.rot, self.r5_rot + (-1 if left else 1), self.pack, self.rw)
      if not is_int(rot):
         raise Exception('can only rotate by integer or r5')
      return loc_t(self.mux, self.i, self.rot + (-rot if left else rot), self.r5_rot, self.pack, self.rw)

   def __lshift__(self, rot):
      return self.shift(rot, True)

   def __rshift__(self, rot):
      return self.shift(rot, False)

   def __getattr__(self, name):
      # discard the first character if it is an underscore. this is a total hack
      # to allow packs starting with a digit to work
      if name[0] == '_':
         name = name[1:]
      if (name in packs) or (name in a_unpacks) or (name in r4_unpacks):
         if self.pack:
            raise Exception('can\'t specify two packs')
         return loc_t(self.mux, self.i, self.rot, self.r5_rot, name, self.rw)
      raise AttributeError()

   def __str__(self):
      if self.mux == MUX_AC:
         return 'r%d' % self.i
      if self.mux == MUX_ANY:
         return 'rany%d' % self.i
      if self.mux == MUX_A:
         return 'ra%d' % self.i
      if self.mux == MUX_B:
         return 'rb%d' % self.i
      assert 0

class sema_t:
   def __init__(self, acq, i):
      if not is_int(i):
         raise Exception('semaphore index must be integer')
      self.acq = acq
      self.i = i

class label_t:
   def __init__(self, rel, name, offset):
      self.rel = rel
      self.name = name
      self.offset = offset

   def __add__(self, offset):
      return label_t(self.rel, self.name, self.offset + offset)

   def __sub__(self, offset):
      return label_t(self.rel, self.name, self.offset - offset)

class label_maker_t:
   def __init__(self, rel):
      self.rel = rel

   def __getattr__(self, name):
      # we discard the first character. this is a total hack to allow numeric labels to work
      if not re_label_ref_right.match(name[1:]):
         raise Exception('invalid label reference')
      return label_t(self.rel, name[1:], 0)

def bits(x, n):
   if (x >> n) != 0:
      raise Exception('%d doesn\'t fit in %d bits' % (x, n))
   return x

def bitsw(x, n):
   if x == (1 << n):
      x = 0
   return bits(x, n)

def bitsws(x, n):
   if x == (1 << (n - 1)):
      x = 0
   if -(1 << (n - 1)) <= x < 0:
      x += 1 << n
   return bits(x, n)

def vpm_setup(n, stride, addr, v2 = False):
   horiz, laned, size, y, x, p = addr
   if size not in (0, 1, 2):
      raise Exception('addr size should be 0, 1, or 2')
   if horiz:
      if x != 0:
         raise Exception('horizontal accesses must have x of 0')
   else:
      if (y & 0xf) != 0:
         raise Exception('vertical accesses must be 16 row aligned')
   hls = (bits(horiz, 1) << 3) | (bits(laned, 1) << 2) | (2 - size)
   if v2:
      return ((1 << 29) | (bitsw(n, 5) << 24) | (bitsws(stride, 7) << 16) |
         (hls << 12) | ((bits(y, 8) | bits(x, 4)) << size) | bits(p, size))
   return ((bitsw(n, 4) << 20) | (bitsw(stride, 6) << 12) |
      (hls << 8) | ((bits(y, 6) | bits(x, 4)) << size) | bits(p, size))

def vdw_setup_0(n, m, addr):
   horiz, size, y, x, p = addr
   if size not in (0, 1, 2):
      raise Exception('addr size should be 0, 1, or 2')
   return ((2 << 30) | (bitsw(n, 7) << 23) | (bitsw(m, 7) << 16) |
      (bits(horiz, 1) << 14) | (bits(y, 7) << 7) | (bits(x, 4) << 3) | (size << 1) | bits(p, size))

def vdr_setup_0(n, m, addr, vpm_stride, stride):
   horiz, size, y, x, p = addr
   if size not in (0, 1, 2):
      raise Exception('addr size should be 0, 1, or 2')
   if (stride < 8) or (stride & (stride - 1)):
      raise Exception('stride must be power of 2 >= 8, 8 meaning use extended stride')
   log2_stride = 3
   while (1 << log2_stride) != stride:
      log2_stride += 1
   return ((1 << 31) | (size << 29) | (bits(p, size) << 28) | (bits(log2_stride - 3, 4) << 24) |
      (bitsw(m, 4) << 20) | (bitsw(n, 4) << 16) | (bitsw(vpm_stride, 4) << 12) |
      (bits(1 - horiz, 1) << 11) | (bits(y, 7) << 4) | bits(x, 4))

class allocator_t:
   def __init__(self, *available):
      self.available = list(available)
      self.allocated = {}
      self.reserved = []

   def copy(self):
      a = allocator_t()
      a.available = self.available[:]
      a.allocated = self.allocated.copy()
      a.reserved = self.reserved[:]
      return a

   def forget(self):
      self.__init__(self.available + self.allocated.values() + self.reserved)

   def reserve(self, *rs):
      for r in rs:
         self.available.remove(r)
         self.reserved.append(r)

   def retire(self, name):
      r = self.allocated.pop(name)
      del r.__invert__
      del r.retire
      self.available.append(r)
      return r

   def __getattr__(self, name):
      if name not in self.allocated:
         r = self.available.pop()
         r.retire = lambda: self.retire(name) # this is an ugly hack to get nicer retire syntax
         r.__invert__ = r.retire
         self.allocated[name] = r
      return self.allocated[name]

def pragma_allow_xor_0(x):
   global allow_xor_0

   if not isinstance(x, bool):
      raise Exception('allow_xor_0 must be bool')
   x, allow_xor_0 = allow_xor_0, x
   return x

def pragma_dont_warn_when_mul_rot_inp_r5(x):
   global dont_warn_when_mul_rot_inp_r5

   if not isinstance(x, bool):
      raise Exception('dont_warn_when_mul_rot_inp_r5 must be bool')
   x, dont_warn_when_mul_rot_inp_r5 = dont_warn_when_mul_rot_inp_r5, x
   return x

arg_defs = {
   # special reg names (these alias the regular names, but also have appropriate read/write restrictions)
   'w':             loc_t(MUX_A,   15, 0, 0, None, RW_EITHER),
   'z':             loc_t(MUX_B,   15, 0, 0, None, RW_EITHER),
   'unif':          loc_t(MUX_ANY, 32, 0, 0, None, RW_READ),
   'vary':          loc_t(MUX_ANY, 35, 0, 0, None, RW_READ),
   'tmurs':         loc_t(MUX_ANY, 36, 0, 0, None, RW_WRITE),
   'r5quad':        loc_t(MUX_A,   37, 0, 0, None, RW_WRITE),
   'r5rep':         loc_t(MUX_B,   37, 0, 0, None, RW_WRITE),
   'elem_num':      loc_t(MUX_A,   38, 0, 0, None, RW_READ),
   'qpu_num':       loc_t(MUX_B,   38, 0, 0, None, RW_READ),
   'unif_addr':     loc_t(MUX_A,   40, 0, 0, None, RW_WRITE),
   'unif_addr_rel': loc_t(MUX_B,   40, 0, 0, None, RW_WRITE),
   'x_coord':       loc_t(MUX_A,   41, 0, 0, None, RW_EITHER),
   'y_coord':       loc_t(MUX_B,   41, 0, 0, None, RW_EITHER),
   'ms_mask':       loc_t(MUX_A,   42, 0, 0, None, RW_EITHER),
   'rev_flag':      loc_t(MUX_B,   42, 0, 0, None, RW_EITHER),
   'stencil':       loc_t(MUX_ANY, 43, 0, 0, None, RW_WRITE),
   'tlbz':          loc_t(MUX_ANY, 44, 0, 0, None, RW_WRITE),
   'tlbm':          loc_t(MUX_ANY, 45, 0, 0, None, RW_WRITE),
   'tlbc':          loc_t(MUX_ANY, 46, 0, 0, None, RW_WRITE),
   'vpm':           loc_t(MUX_ANY, 48, 0, 0, None, RW_EITHER),
   'vr_busy':       loc_t(MUX_A,   49, 0, 0, None, RW_READ),
   'vw_busy':       loc_t(MUX_B,   49, 0, 0, None, RW_READ),
   'vr_setup':      loc_t(MUX_A,   49, 0, 0, None, RW_WRITE),
   'vw_setup':      loc_t(MUX_B,   49, 0, 0, None, RW_WRITE),
   'vr_wait':       loc_t(MUX_A,   50, 0, 0, None, RW_READ),
   'vw_wait':       loc_t(MUX_B,   50, 0, 0, None, RW_READ),
   'vr_addr':       loc_t(MUX_A,   50, 0, 0, None, RW_WRITE),
   'vw_addr':       loc_t(MUX_B,   50, 0, 0, None, RW_WRITE),
   'mutex':         loc_t(MUX_ANY, 51, 0, 0, None, RW_EITHER),
   'recip':         loc_t(MUX_ANY, 52, 0, 0, None, RW_WRITE),
   'recipsqrt':     loc_t(MUX_ANY, 53, 0, 0, None, RW_WRITE),
   'rsqrt':         loc_t(MUX_ANY, 53, 0, 0, None, RW_WRITE),
   'exp':           loc_t(MUX_ANY, 54, 0, 0, None, RW_WRITE),
   'log':           loc_t(MUX_ANY, 55, 0, 0, None, RW_WRITE),
   't0s':           loc_t(MUX_ANY, 56, 0, 0, None, RW_WRITE),
   't0t':           loc_t(MUX_ANY, 57, 0, 0, None, RW_WRITE),
   't0r':           loc_t(MUX_ANY, 58, 0, 0, None, RW_WRITE),
   't0b':           loc_t(MUX_ANY, 59, 0, 0, None, RW_WRITE),
   't1s':           loc_t(MUX_ANY, 60, 0, 0, None, RW_WRITE),
   't1t':           loc_t(MUX_ANY, 61, 0, 0, None, RW_WRITE),
   't1r':           loc_t(MUX_ANY, 62, 0, 0, None, RW_WRITE),
   't1b':           loc_t(MUX_ANY, 63, 0, 0, None, RW_WRITE),

   # semaphore acq/rel
   'sacq': lambda i: sema_t(True, i),
   'srel': lambda i: sema_t(False, i),

   # label makers (before evaluating, the syntax x:label gets transformed to x_label_maker._label)
   'r_label_maker': label_maker_t(True),
   'a_label_maker': label_maker_t(False),

   # handy functions
   'f':     lambda x: struct.unpack('I', struct.pack('f', x))[0],
   'sqrt':  math.sqrt,
   'sin':   math.sin,
   'cos':   math.cos,
   'atan2': math.atan2,
   'pi':    math.pi,
   'rseed': random.seed,
   'rand':  lambda: int(random.getrandbits(32)),
   'bits':  bits,
   'bitsw': bitsw,
   'bitsws': bitsws,

   # handy vpm/vdw/vdr stuff
   'h32':  lambda y:       (1, 0, 0, y, 0, 0),
   'h16l': lambda y, p:    (1, 1, 1, y, 0, p),
   'h16p': lambda y, p:    (1, 0, 1, y, 0, p),
   'h8l':  lambda y, p:    (1, 1, 2, y, 0, p),
   'h8p':  lambda y, p:    (1, 0, 2, y, 0, p),
   'v32':  lambda y, x:    (0, 0, 0, y, x, 0),
   'v16l': lambda y, x, p: (0, 1, 1, y, x, p),
   'v16p': lambda y, x, p: (0, 0, 1, y, x, p),
   'v8l':  lambda y, x, p: (0, 1, 2, y, x, p),
   'v8p':  lambda y, x, p: (0, 0, 2, y, x, p),
   'dma_h32':  lambda y, x:    (1, 0, y, x, 0),
   'dma_h16p': lambda y, x, p: (1, 1, y, x, p),
   'dma_h8p':  lambda y, x, p: (1, 2, y, x, p),
   'dma_v32':  lambda y, x:    (0, 0, y, x, 0),
   'dma_v16p': lambda y, x, p: (0, 1, y, x, p),
   'dma_v8p':  lambda y, x, p: (0, 2, y, x, p),
   'vpm_setup': vpm_setup,
   'vpm_setup_v2': lambda n, stride, addr: vpm_setup(n, stride, addr, True),
   'vdw_setup_0': vdw_setup_0,
   'vdw_setup_1': lambda stride: (3 << 30) | bits(stride, 13),
   'vdr_setup_0': vdr_setup_0,
   'vdr_setup_ext_stride': 8, # stride of 8 means use extended stride
   'vdr_setup_1': lambda stride: (9 << 28) | bits(stride, 13),

   # annotations
   'mul_used': lambda *is_: ('mul_used', sum(1 << i for i in is_)),
   'mul_unused': lambda *is_: ('mul_used', sum(1 << i for i in is_) ^ 0xffff),
   'preserve_cond': ('preserve_cond', 1),

   # somewhat experimental register allocator
   'allocator_t': allocator_t,

   # pragmas
   'pragma_allow_xor_0': pragma_allow_xor_0,
   'pragma_dont_warn_when_mul_rot_inp_r5': pragma_dont_warn_when_mul_rot_inp_r5}

# accumulators and regs (regular names -- r0, ra0, etc)
arg_defs.update(('r%d' % i, loc_t(MUX_AC, i, 0, 0, None, RW_EITHER)) for i in xrange(6))
arg_defs.update(('rany%d' % i, loc_t(MUX_ANY, i, 0, 0, None, RW_EITHER)) for i in xrange(64))
arg_defs.update(('ra%d' % i, loc_t(MUX_A, i, 0, 0, None, RW_EITHER)) for i in xrange(64))
arg_defs.update(('rb%d' % i, loc_t(MUX_B, i, 0, 0, None, RW_EITHER)) for i in xrange(64))

def arg_eval(arg, sets):
   s = (arg.strip().split('.', 1) + [None])[:2]
   if s[0] == '-':
      return loc_t(MUX_ANY, WADDR_NOP, 0, 0, s[1], RW_WRITE)
   arg = re_label_ref_left.sub('\\1_label_maker._', arg) # todo: we probably don't want to replace in strings...
   arg = re_pack.sub('._\\1', arg)
   try:
      # todo: i would like to be able to pass both arg_defs and sets in here
      # (with sets hiding arg_defs in the case of conflicts), but the obvious
      # dict(arg_defs, **sets) won't permit things such as:
      # .set f, lambda x: y
      # .set y, 4
      # (the y in the lambda will be looked up in the temporary dict we created
      # when evaluating the f .set, which doesn't contain y)
      #
      # instead, sets is initially set to (a copy of) arg_defs. to simulate the
      # hiding behaviour, on an unset, we restore any hidden arg_defs value.
      # also, before dumping sets at the end, we strip out the arg_defs stuff
      # (this isn't entirely correct as we want to dump sets that are hiding
      # arg_defs)
      return eval(arg, sets)
   except Exception, e:
      asm_error(e)
   except:
      asm_error('unknown error while evaluating argument')

# doesn't check/fixup pack
def check_and_fixup_loc(loc, read):
   if (not read) and (loc.rw == RW_READ):
      asm_error('writing to read-only hardware register')
   if read and (loc.rw == RW_WRITE):
      asm_error('reading from write-only hardware register')
   if not read:
      # conceptually, we are writing to a location rotated right by
      # loc.rot/loc.r5_rot. but we are actually rotating the output right by
      # -loc.rot/-loc.r5_rot then writing it to the unrotated location
      loc.rot = -loc.rot % 16
      loc.r5_rot = -loc.r5_rot % 16
   if (loc.rot != 0) and (loc.r5_rot != 0):
      asm_error('can\'t rotate by both r5 and immediate')
   if (loc.r5_rot != 0) and (loc.r5_rot != 1):
      asm_error('only supported rotation by r5 is once to the %s' % ('left', 'right')[read])
   if (not mulw_rotate) and ((loc.rot != 0) or loc.r5_rot): # mulw_rotate source checking is done later
      if not read:
         asm_error('target doesn\'t support write rotation')
      if loc.mux == MUX_ANY:
         loc.mux = MUX_A # can't do rotated read from regfile b
      if loc.mux != MUX_A:
         asm_error('rotation on read only allowed from regfile a')
      if loc.i >= 32:
         asm_warning('rotation only works from physical regfile')
   if loc.mux == MUX_AC:
      if (loc.i < 0) or (loc.i >= 6):
         asm_error('reg out of range')
      if not read:
         if loc.i == 4:
            asm_error('not allowed to write to r4')
         if loc.i == 5:

            asm_error('not allowed to write to r5 -- please specify r5quad or r5rep')
   elif (loc.mux == MUX_ANY) or (loc.mux == MUX_A) or (loc.mux == MUX_B):
      if (loc.i < 0) or (loc.i >= 64):
         asm_error('reg out of range')
   else:
      assert 0

def get_dst(dst, sets):
   if not dst:
      return None, None, (0, PACK_TYPE_EITHER, PACK_MODE_EITHER), 0, 0
   dst = arg_eval(dst, sets)
   if not isinstance(dst, loc_t):
      asm_error('invalid dst')
   dst = dst.copy()
   check_and_fixup_loc(dst, False)
   pack = get_pack(dst.pack)
   if dst.mux == MUX_AC:
      if pack[2] == PACK_MODE_A:
         asm_warning('ra packing only works when writing to physical regfile')
         return WADDR_R0 + dst.i, WMUX_A, pack, dst.rot, dst.r5_rot
      return WADDR_R0 + dst.i, WMUX_ANY, pack, dst.rot, dst.r5_rot
   if (dst.mux == MUX_A) or ((dst.mux == MUX_ANY) and (pack[2] == PACK_MODE_A)): # can't pack to regfile b with this operation
      if (pack[2] == PACK_MODE_A) and (dst.i >= 32):
         asm_warning('ra packing only works when writing to physical regfile')
      return dst.i, WMUX_A, pack, dst.rot, dst.r5_rot
   if dst.mux == MUX_ANY:
      return dst.i, WMUX_ANY, pack, dst.rot, dst.r5_rot
   if dst.mux == MUX_B:
      if pack[2] == PACK_MODE_A:
         asm_error('this packing operation can only be used for regfile a')
      return dst.i, WMUX_B, pack, dst.rot, dst.r5_rot
   assert 0

def get_src(src, sets):
   if not src:
      return None, None, (0, PACK_TYPE_EITHER, UNPACK_LOC_OTHER), None, None
   src = arg_eval(src, sets)
   if isinstance(src, sema_t):
      if not have_sema:
         asm_error('target does not support semaphores')
      if (src.i < 0) or (src.i >= 16):
         asm_error('semaphore number must be in [0, 16)')
      return src.i | (src.acq << 4), RMUX_SEMA, (0, PACK_TYPE_EITHER, UNPACK_LOC_OTHER), 0, 0
   if isinstance(src, label_t):
      return (src.name, src.rel, src.offset), RMUX_LABEL, (0, PACK_TYPE_EITHER, UNPACK_LOC_OTHER), 0, 0
   if isinstance(src, list):
      if len(src) != 16:
         asm_error('vector immediate must have length 16')
      src = src[:]
      for i in xrange(16):
         if not is_int(src[i]):
            asm_error('all elements of vector immediate must be integers')
         src[i] &= (1 << 32) - 1
      return src, RMUX_IMMV, (0, PACK_TYPE_EITHER, UNPACK_LOC_OTHER), 0, 0
   if is_int(src):
      return src & ((1 << 32) - 1), RMUX_IMM, (0, PACK_TYPE_EITHER, UNPACK_LOC_OTHER), 0, 0
   if not isinstance(src, loc_t):
      asm_error('invalid src')
   src = src.copy()
   check_and_fixup_loc(src, True)
   if mulw_rotate:
      srot, sr5rot = 0, 0
      drot, dr5rot = src.rot, src.r5_rot
   else:
      srot, sr5rot = src.rot, src.r5_rot
      drot, dr5rot = 0, 0
   if src.mux == MUX_AC:
      if src.i == 4:
         return 4, RMUX_AC, get_r4_unpack(src.pack), drot, dr5rot
      if src.pack:
         asm_error('unpack only allowed for regfile a or r4')
      return src.i, RMUX_AC, (0, PACK_TYPE_EITHER, UNPACK_LOC_OTHER), drot, dr5rot
   if (src.mux == MUX_A) or ((src.mux == MUX_ANY) and src.pack): # can't unpack from regfile b
      return (src.i, srot, sr5rot), RMUX_A, get_a_unpack(src.pack), drot, dr5rot
   if src.mux == MUX_ANY:
      return src.i, RMUX_ANY, (0, PACK_TYPE_EITHER, UNPACK_LOC_AB), drot, dr5rot
   if src.mux == MUX_B:
      if src.pack:
         asm_error('unpack only allowed for regfile a or r4')
      return src.i, RMUX_B, (0, PACK_TYPE_EITHER, UNPACK_LOC_OTHER), drot, dr5rot
   assert 0

# signals
##########

sigs = {
   'bkpt': SIG_BKPT,
   'thrsw': SIG_THRSW,
   'thrend': SIG_THREND,
   'sbwait': SIG_SBWAIT,
   'sbdone': SIG_SBDONE,
   'int': SIG_INT,
   'loadcv': SIG_LOADCV,
   'loadc': SIG_LOADC,
   'ldcend': SIG_LDCEND,
   'ldtmu0': SIG_LDTMU0,
   'ldtmu1': SIG_LDTMU1}

def get_sig(sig):
   if sig not in sigs:
      return SIG_NORMAL
   return sigs[sig]

# annotations
##############

def get_annots(annot, sets):
   annots = arg_eval(annot, sets)
   if isinstance(annots, list):
      annots = annots[:]
   else:
      annots = [annots]
   for i, annot in enumerate(annots):
      if ((not isinstance(annot, tuple)) or (len(annot) != 2) or (not isinstance(annot[0], str)) or
         (not is_int(annot[1]))):
         asm_error('annotation must be (string, integer) pair, or a list of such pairs')
      annots[i] = (annot[0], annot[1] & ((1 << 32) - 1))
   return annots

###############################################################################
# core
###############################################################################

def calculate_pack_modes(rpacks, rfloats, couldrfloat, wpacks, wfloats):
   needfloat = PACK_TYPE_EITHER
   havefloata = False
   havefloatr4 = False
   unpacka = None
   unpackr4 = None
   forcebs = [False, False, False, False]
   forcerafloat = False

   pm = PACK_MODE_EITHER
   for i in (0, 1, 2, 3):
      if (rpacks[i][2] == UNPACK_LOC_OTHER) or (rpacks[i][2] == UNPACK_LOC_AB):
         assert rpacks[i][0] == 0
      else:
         if rpacks[i][2] == UNPACK_LOC_A:
            if unpacka is None:
               unpacka = rpacks[i][0]
            elif unpacka != rpacks[i][0]:
               asm_error('conflicting unpack operations on regfile a')
            havefloata = havefloata or rfloats[i]
         elif rpacks[i][2] == UNPACK_LOC_R4:
            if unpackr4 is None:
               unpackr4 = rpacks[i][0]
            elif unpackr4 != rpacks[i][0]:
               asm_error('conflicting unpack operations on r4')
            havefloatr4 = havefloatr4 or rfloats[i]
         else:
            assert 0

         if rpacks[i][1] != PACK_TYPE_EITHER:
            if (needfloat != PACK_TYPE_EITHER) and (needfloat != rpacks[i][1]):
               asm_error('conflicting unpack float requirements')
            needfloat = rpacks[i][1]
   for i in (0, 1, 2, 3):
      if rpacks[i][2] == UNPACK_LOC_AB:
         if (unpacka is not None) and (unpacka != UNPACK_A_NOP):
            forcebs[i] = True # non-nop unpack from regfile a. must use b

   if unpacka:
      if (needfloat == PACK_TYPE_FLOAT) and (not havefloata) and couldrfloat:
         havefloata = True
         forcerafloat = True
      havefloat = havefloata
   else:
      havefloat = havefloatr4

   if (needfloat == PACK_TYPE_FLOAT) and (not havefloat):
      asm_error('float unpack operation used in integer alu operations')
   if (needfloat == PACK_TYPE_INT) and havefloat:
      asm_error('integer unpack operation used in float alu operation')

   unpack = 0
   if unpacka and unpackr4:
      asm_error('cannot specify pack operation for both regfile a and r4')
   if unpacka:
      pm = PACK_MODE_A
      unpack = unpacka
   elif unpackr4:
      pm = PACK_MODE_M
      unpack = unpackr4

   pack = 0
   if wpacks[0][2] == PACK_MODE_M:
      asm_error('mul-unit pack operation used on add result')
   for i in (0, 1):
      if wpacks[i][2] == PACK_MODE_A:
         if (pm != PACK_MODE_EITHER) and (pm != PACK_MODE_A):
            asm_error('conflicting pack modes')
         pm = PACK_MODE_A
         pack = wpacks[i][0]
      elif wpacks[i][2] == PACK_MODE_M:
         if (pm != PACK_MODE_EITHER) and (pm != PACK_MODE_M):
            asm_error('conflicting pack modes')
         pm = PACK_MODE_M
         pack = wpacks[i][0]

      if (wpacks[i][1] == PACK_TYPE_FLOAT) and (not wfloats[i]):
         asm_error('float pack operation used with integer alu result')
      if (wpacks[i][1] == PACK_TYPE_INT) and wfloats[i]:
         asm_error('integer pack operation used with float alu result')

   if pm == PACK_MODE_EITHER:
      pm = PACK_MODE_A
   return pm, pack, unpack, forcebs, forcerafloat

# immediates that can be encoded with SIG_SMALLIMMED
bimms = {}
bimms.update((i, i) for i in xrange(16))
bimms.update(((i - 32) + (1 << 32), i) for i in xrange(16, 32))
bimms.update(((127 + (i - 32)) << 23, i) for i in xrange(32, 40))
bimms.update(((127 + (i - 48)) << 23, i) for i in xrange(40, 48))

def merge_rmux(raddr_a, raddr_b, immb, arot_r5, raddr, rmux):
   if rmux == RMUX_SEMA:
      asm_error('semaphore op can only be used with mov')
   if rmux == RMUX_LABEL:
      asm_error('label not allowed here')
   if rmux == RMUX_IMMV:
      asm_error('vector immediate can only be used with mov')
   if rmux == RMUX_IMM:
      if raddr not in bimms:
         asm_error('can\'t encode immediate 0x%08x' % raddr)
      raddr = bimms[raddr]
      if not immb:
         if raddr_b is not None:
            asm_error('regfile b and immediates don\'t mix')
         raddr_b = raddr
         immb = True
      elif raddr_b != raddr:
         asm_error('can only encode one rotation/immediate')
      return raddr_a, raddr_b, immb, arot_r5, RMUX_B
   if rmux == RMUX_AC:
      return raddr_a, raddr_b, immb, arot_r5, RMUX_A0 + raddr
   if rmux == RMUX_ANY:
      if (mulw_rotate or (((not immb) or (raddr_b < 48)) and (not arot_r5))) and (raddr_a == raddr):
         return raddr_a, raddr_b, immb, arot_r5, RMUX_A
      if (not immb) and (raddr_b == raddr):
         return raddr_a, raddr_b, immb, arot_r5, RMUX_B
      if raddr_a is None:
         assert mulw_rotate or (((not immb) or (raddr_b < 48)) and (not arot_r5))
         raddr_a = raddr
         return raddr_a, raddr_b, immb, arot_r5, RMUX_A
      if raddr_b is None:
         assert not immb
         raddr_b = raddr
         return raddr_a, raddr_b, immb, arot_r5, RMUX_B
      asm_error('no free read slots')
   if rmux == RMUX_A:
      if (not mulw_rotate) and (raddr_a is not None) and (
         ((raddr[1] != 0) | ((raddr[2] != 0) << 1)) != ((immb and (raddr_b >= 48)) | (arot_r5 << 1))):
         asm_error('conflicting rotations from regfile a')
      if raddr_a is None:
         raddr_a = raddr[0]
      elif raddr_a != raddr[0]:
         asm_error('can only read from one location in each regfile')
      arot_r5 = raddr[2]
      if raddr[1] == 0:
         return raddr_a, raddr_b, immb, arot_r5, RMUX_A
      raddr = 48 + raddr[1]
      if not immb:
         if raddr_b is not None:
            asm_error('regfile b and rotation don\'t mix')
         raddr_b = raddr
         immb = True
      elif raddr_b != raddr:
         asm_error('can only encode one rotation/immediate')
      return raddr_a, raddr_b, immb, arot_r5, RMUX_A
   if rmux == RMUX_B:
      if immb:
         asm_error('regfile b and rotation/immediates don\'t mix')
      if raddr_b is None:
         raddr_b = raddr
      elif raddr_b != raddr:
         asm_error('can only read from one location in each regfile')
      return raddr_a, raddr_b, immb, arot_r5, RMUX_B
   assert 0

# ok if:
# - accumulator (r0-r3)
# - uniform (ie all elements identical). this is true of unif, qpu_num, vr_busy,
#   and vw_busy. it's also true of r5 if it was written by r5rep, but not if it
#   was written by r5quad. so, by default, r5 isn't considered uniform. todo:
#   what about vr_wait/vw_wait/mutex?
def read_rot_ok(rmux, raddr_a, raddr_b):
   return ((rmux < 4) or ((rmux == 5) and dont_warn_when_mul_rot_inp_r5) or
      ((rmux == 6) and (raddr_a in (32, 49))) or # unif/vr_busy
      ((rmux == 7) and (raddr_b in (32, 38, 49)))) # unif/qpu_num/vw_busy

def asm_flush_prog_data():
   global prog_data

   while len(prog_data) & 7:
      prog_data.append(0)
   for i in xrange(0, len(prog_data), 8):
      prog.append(((prog_data[i + 3] << 24) | (prog_data[i + 2] << 16) | (prog_data[i + 1] << 8) | (prog_data[i + 0] << 0),
         (prog_data[i + 7] << 24) | (prog_data[i + 6] << 16) | (prog_data[i + 5] << 8) | (prog_data[i + 4] << 0), 'data', {}))
   prog_data = []

def asm_line(sets, location, line):
   global current_location, construct, nwarn_level

   prev_location = current_location
   current_location = location

   try:
      if construct != None:
         if re_macro.match(line):
            construct_stack.append(CONSTRUCT_MACRO)
         elif re_if.match(line):
            construct_stack.append(CONSTRUCT_IF)
         elif re_rep.match(line):
            construct_stack.append(CONSTRUCT_REP)
         else:
            else_m = line == '.else'
            elif_m = re_elif.match(line)
            if elif_m:
               end_construct = CONSTRUCT_IF
            else:
               end_construct = {
                  '.endm':  CONSTRUCT_MACRO,
                  '.else':  CONSTRUCT_IF,
                  '.endif': CONSTRUCT_IF | CONSTRUCT_ELSE,
                  '.endr':  CONSTRUCT_REP}.get(line)
            if end_construct is not None:
               end_construct &= construct_stack.pop()
               if end_construct == 0:
                  if elif_m:
                     asm_error('unexpected .elif')
                  asm_error('unexpected %s' % line)
               if len(construct_stack) == 0:
                  lines = construct
                  construct = None
                  if end_construct == CONSTRUCT_MACRO:
                     return
                  if (end_construct == CONSTRUCT_IF) or (end_construct == CONSTRUCT_ELSE):
                     condition_if, condition_else = lines[0]
                     lines = lines[1:]
                     if condition_if:
                        for location, line in lines:
                           asm_line(sets, location, line)
                     if else_m:
                        construct = [(condition_else, False)]
                        construct_stack.append(CONSTRUCT_ELSE)
                     elif elif_m:
                        if elif_m.group('set'):
                           condition_if = condition_else and ((elif_m.group('set') == 'nset') ^ (elif_m.group('name') in sets))
                        else:
                           condition_if = condition_else and arg_eval(elif_m.group('condition'), sets)
                        condition_else = condition_else and (not condition_if)
                        construct = [(condition_if, condition_else)]
                        construct_stack.append(CONSTRUCT_IF)
                     return
                  if end_construct == CONSTRUCT_REP:
                     name, count = lines[0]
                     lines = lines[1:]
                     for i in xrange(count):
                        sets[name] = i
                        for location, line in lines:
                           asm_line(sets, location, line)
                     return
                  assert 0
               if else_m:
                  construct_stack.append(CONSTRUCT_ELSE)
               elif elif_m:
                  construct_stack.append(CONSTRUCT_IF)
         construct.append((current_location, line))
         return

      if line in ('.endm', '.else', '.endif', '.endr'):
         asm_error('unexpected %s' % line)
      if re_elif.match(line):
         asm_error('unexpected .elif')

      m = re_macro.match(line)
      if m:
         construct = []
         construct_stack.append(CONSTRUCT_MACRO)
         macros[m.group('name')] = ([param.strip() for param in m.group('params').split(',')[1:]], construct)
         return

      m = re_if.match(line)
      if m:
         if m.group('set'):
            condition = (m.group('set') == 'nset') ^ (m.group('name') in sets)
         else:
            # not not forces condition to a bool (this matters if condition is
            # something mutable like a list)
            condition = not not arg_eval(m.group('condition'), sets)
         construct = [(condition, not condition)]
         construct_stack.append(CONSTRUCT_IF)
         return

      m = re_rep.match(line)
      if m:
         count = arg_eval(m.group('count'), sets)
         if not is_int(count):
            asm_error('.rep count must be integer')
         construct = [(m.group('name'), count)]
         construct_stack.append(CONSTRUCT_REP)
         return

      m = re_include.match(line)
      if m:
         filename = arg_eval(m.group('filename'), sets)
         if not isinstance(filename, str):
            asm_error('expected string')
         asm_file(sets, '%s: %s' % (current_location, filename), filename)
         return

      m = re_set.match(line)
      if m:
         sets[m.group('name')] = arg_eval(m.group('val'), sets)
         return

      m = re_unset.match(line)
      if m:
         name = m.group('name')
         if name not in sets:
            asm_error('%s not set' % name)
         if name in arg_defs: # todo: see arg_eval
            sets[name] = arg_defs[name]
         else:
            del sets[name]
         return

      m = re_eval.match(line)
      if m:
         arg_eval(m.group('expr'), sets)
         return

      m = re_print_info_warn_error.match(line)
      if m:
         def print_fn(message):
            print message
         def info_fn(message):
            sys.stderr.write('%s\n' % message)
         {'print': print_fn, 'info': info_fn, 'warn': asm_warning, 'error': asm_error}[
            m.group('print_info_warn_error')](arg_eval(m.group('message'), sets))
         return

      m = re_assert.match(line)
      if m:
         if not arg_eval(m.group('condition'), sets):
            asm_error('assertion failure: \'%s\'' % m.group('condition'))
         return

      m = re_data.match(line)
      if m:
         size = int(m.group('size'))
         for datum in smart_split(m.group('data')):
            datum = arg_eval(datum, sets)
            if not is_int(datum):
               asm_error('datum must be integer')
            prog_data.extend(((datum >> (i * 8)) & 0xff) for i in xrange(size))
         return

      m = re_macro_inst.match(line)
      if m:
         name = m.group('name')
         if name in macros:
            params, lines = macros[name]
            args = smart_split(m.group('args'))
            if len(args) > len(params):
               asm_error('too many arguments to macro')
            sets = sets.copy()
            sets.update(zip(params, (arg_eval(arg, sets) for arg in args)))
            for param in params[len(args):]:
               if param in sets:
                  if param in arg_defs: # todo: see arg_eval
                     sets[param] = arg_defs[param]
                  else:
                     del sets[param]
            for location, line in lines:
               asm_line(sets, '%s: %s' % (current_location, location), line)
            return

      if line == '.pushnwarn':
         nwarn_level += 1
         return
      if line == '.popnwarn':
         if nwarn_level == 0:
            asm_error('.popnwarn without .pushnwarn')
         nwarn_level -= 1
         return

      # everything below assumes prog is up to date
      asm_flush_prog_data()

      m = re_label.match(line)
      if m:
         name = m.group('name')
         if name[0].isdigit():
            labels.setdefault(name, []).append(len(prog))
         else:
            if name[0] == ':':
               undecorated_name = name[1:]
            else:
               undecorated_name = name
            if (undecorated_name in labels) or ((':' + undecorated_name) in labels):
               asm_error('named label defined twice')
            labels[name] = len(prog)
         return

      annots = line.split('@')
      ops = [op.strip() for op in annots[0].split(';')]
      annots = sum((get_annots(annot, sets) for annot in annots[1:]), [])
      sig = get_sig(ops[-1])
      if sig != SIG_NORMAL:
         ops = ops[:-1]
      if len(ops) > 2:
         asm_error('too many ops')
      elif (len(ops) == 1) and (ops[0] == ''):
         ops = []
      ops = (ops + ['nop', 'nop'])[:2]
      m = re_op.match(ops[0])
      if not m:
         asm_error('invalid syntax')
      aop, aargs_n = get_aop(m.group('op'))
      if (aop == AOP_BRA) or (aop == AOP_BRR):
         acond = get_bcond(m.group('cond'))
      else:
         acond = get_cond(m.group('cond'))
      asf = get_setf(m.group('sf'))
      aargs = smart_split(m.group('args'))
      if len(aargs) != aargs_n:
         asm_error('wrong operand count')
      ard, ara, arb = (aargs + [None, None, None])[:3]
      m = re_op.match(ops[1])
      if not m:
         asm_error('invalid syntax')
      mop, margs_n = get_mop(m.group('op'))
      mcond = get_cond(m.group('cond'))
      msf = get_setf(m.group('sf'))
      margs = smart_split(m.group('args'))
      if len(margs) != margs_n:
         asm_error('wrong operand count')
      mrd, mra, mrb = (margs + [None, None, None])[:3]
      # eval srcs first so allocator can retire and reuse registers for dst
      aaraddr, aarmux, aarpack, aadrot, aadrot_r5 = get_src(ara, sets)
      abraddr, abrmux, abrpack, abdrot, abdrot_r5 = get_src(arb, sets)
      maraddr, marmux, marpack, madrot, madrot_r5 = get_src(mra, sets)
      mbraddr, mbrmux, mbrpack, mbdrot, mbdrot_r5 = get_src(mrb, sets)
      awaddr, awmux, awpack, awrot, awrot_r5 = get_dst(ard, sets)
      mwaddr, mwmux, mwpack, mwrot, mwrot_r5 = get_dst(mrd, sets)
      if (((abrmux is not None) and ((aadrot != abdrot) or (aadrot_r5 != abdrot_r5))) or
         ((mbrmux is not None) and ((madrot != mbdrot) or (madrot_r5 != mbdrot_r5)))):
         asm_error('cannot have 2 arguments with different rotations')
      if aarmux is not None:
         awrot = (awrot + aadrot) % 16
         awrot_r5 = (awrot_r5 + aadrot_r5) % 16
      if (awrot != 0) or awrot_r5:
         asm_error('rotate not allowed on add write')
      if marmux is not None:
         mwrot = (mwrot + madrot) % 16
         mwrot_r5 = (mwrot_r5 + madrot_r5) % 16

      afloatr = aop in (AOP_FADD, AOP_FSUB, AOP_FMIN, AOP_FMAX, AOP_FMINABS, AOP_FMAXABS, AOP_FTOI)
      afloatw = aop in (AOP_FADD, AOP_FSUB, AOP_FMIN, AOP_FMAX, AOP_FMINABS, AOP_FMAXABS, AOP_ITOF)
      pm, pack, unpack, forcebs, forcerafloat = calculate_pack_modes(
         [aarpack, abrpack, marpack, mbrpack],
         [afloatr, afloatr, mop == MOP_FMUL, mop == MOP_FMUL],
         aop == AOP_FTOI,
         [awpack, mwpack],
         [afloatw, mop == MOP_FMUL])
      if forcebs[0]:
         aarmux = RMUX_B
      if forcebs[1]:
         abrmux = RMUX_B
      if forcebs[2]:
         marmux = RMUX_B
      if forcebs[3]:
         mbrmux = RMUX_B

      # extend nops to 3 operands
      if aop == AOP_NOP:
         awaddr, awmux, aaraddr, aarmux, abraddr, abrmux = WADDR_NOP, WMUX_ANY, 0, RMUX_AC, 0, RMUX_AC
      if mop == MOP_NOP:
         mwaddr, mwmux, maraddr, marmux, mbraddr, mbrmux = WADDR_NOP, WMUX_ANY, 0, RMUX_AC, 0, RMUX_AC

      # extend 2 operand alu ops to 3 operands (by duplicating the 2nd operand)
      if (aop == AOP_FTOI) or (aop == AOP_ITOF) or (aop == AOP_NOT) or (aop == AOP_CLZ):
         if forcerafloat:
            assert aop == AOP_FTOI # can only forcerafloat if we have an unused float operand
            # instead of duplicating the 2nd operand, take the ra operand from
            # the mul op thus forcing the ra value to be considered a float for
            # the purposes of unpacking
            if marmux == RMUX_A:
               abraddr, abrmux = maraddr, marmux
            else:
               assert mbrmux == RMUX_A
               abraddr, abrmux = mbraddr, mbrmux
         else:
            abraddr, abrmux = aaraddr, aarmux
      else:
         assert not forcerafloat # can only forcerafloat if we have an unused operand

      # handle write addrs
      if (awmux == mwmux) and (awmux != WMUX_ANY):
         asm_error('add/mul ops not allowed to write to same regfile')
      ws = (awmux == WMUX_B) or (mwmux == WMUX_A)

      # handle branch
      if (aop == AOP_BRA) or (aop == AOP_BRR):
         # check setf
         if asf:
            asm_error('setf not allowed on bra/brr')

         # check pack/unpack
         if (pack != 0) or (unpack != 0):
            asm_error('pack/unpack not allowed with bra/brr')

         # handle read address
         if aarmux == RMUX_LABEL:
            if (aop == AOP_BRA) and aaraddr[1]:
               asm_warning('bra with rel label')
            if (aop == AOP_BRR) and (not aaraddr[1]):
               asm_warning('brr with abs label')
            aaraddr, aarmux = (current_location,) + aaraddr, RMUX_IMM
         if aarmux == RMUX_ANY:
            aaraddr, aarmux = (aaraddr, 0, 0), RMUX_A
         if (aarmux != RMUX_IMM) and (aarmux != RMUX_A):
            asm_error('branch destination must be either label, immediate, or from regfile a')
         if aarmux == RMUX_IMM:
            imm = aaraddr
            raddr = 0 # can't use RADDR_NOP
         elif aarmux == RMUX_A:
            if (aaraddr[1] != 0) or (aaraddr[2] != 0):
               asm_error('rotation of read from regfile a not allowed with branch')
            if aop == AOP_BRR:
               asm_warning('brr with ra')
            imm = 0
            raddr = aaraddr[0]
         else:
            assert 0

         # check mul op is nop
         if mop != MOP_NOP:
            asm_error('mul op not allowed with branch')

         # check sig
         if sig != SIG_NORMAL:
            asm_error('no signal allowed with branch')

         if raddr >= 32:
            asm_error('can only branch to register locations in physical regfile')
         if raddr & 1:
            asm_warning('branch instruction will destroy flags (see hw-2780)')

         # construct branch instruction
         prog.append((imm,
            (mwaddr << 0) | (awaddr << 6) | (ws << 12) | (raddr << 13) | ((aarmux == RMUX_A) << 18) | ((aop == AOP_BRR) << 19) | (acond << 20) | (SIG_BRANCH << 28),
            line, annots))

         return

      # use COND_NEVER when possible (might save power / allow mul setf)
      if not dict(annots).get('preserve_cond', 0):
          if (awaddr == WADDR_NOP) and (not asf):
             acond = COND_NEVER
          if (mwaddr == WADDR_NOP) and (not msf):
             mcond = COND_NEVER

      # attempt to convert movs to ldi
      if (# no mul setf
         (not msf) and
         # ops must either be nop or mov of sema/label/imm/immv
         ((aop == AOP_NOP) or ((aop == AOP_MOV) and (aarmux in (RMUX_SEMA, RMUX_LABEL, RMUX_IMMV, RMUX_IMM)))) and
         ((mop == MOP_NOP) or ((mop == MOP_MOV) and (marmux in (RMUX_SEMA, RMUX_LABEL, RMUX_IMMV, RMUX_IMM)))) and
         # but we don't want 2 nops
         ((aop != AOP_NOP) or (mop != MOP_NOP)) and
         # if both ops are movs, srcs must be identical
         ((aop != AOP_MOV) or (mop != MOP_MOV) or ((aarmux == marmux) and (aaraddr == maraddr))) and
         # no signal
         (sig == SIG_NORMAL)):
         # make sure aarmux/aaraddr contains the value
         if aop != AOP_MOV:
            aarmux = marmux
            aaraddr = maraddr

         # convert immediate
         if aarmux == RMUX_SEMA:
            ldi_mode = LDI_SEMA
         elif aarmux == RMUX_LABEL:
            ldi_mode = LDI_32
            aaraddr, aarmux = (current_location,) + aaraddr, RMUX_IMM
         elif aarmux == RMUX_IMMV:
            signed, unsigned = True, True
            imm = 0
            for i, elem in enumerate(aaraddr):
               if elem not in (-2 + (1 << 32), -1 + (1 << 32), 0, 1):
                  signed = False
               if elem not in (0, 1, 2, 3):
                  unsigned = False
               imm |= ((elem & 0x1) << i) | ((elem & 0x2) << (15 + i))
            if not (signed or unsigned):
               asm_error('can\'t encode vector immediate')
            if signed:
               ldi_mode = LDI_EL_SIGNED
            else:
               ldi_mode = LDI_EL_UNSIGNED
            aaraddr, aarmux = imm, RMUX_IMM
         elif aarmux == RMUX_IMM:
            ldi_mode = LDI_32
         else:
            assert 0

         # construct ldi instruction
         prog.append((aaraddr,
            (mwaddr << 0) | (awaddr << 6) | (ws << 12) | (asf << 13) | (mcond << 14) | (acond << 17) | (pack << 20) | (pm << 24) | (ldi_mode << 25) | (SIG_IMMED << 28),
            line, annots))

         return

      # convert movs to alu ops
      if aop == AOP_MOV:
         if allow_xor_0 and (aarmux == RMUX_IMM) and (aaraddr == 0):
            aop = AOP_XOR
            aaraddr, aarmux = 0, RMUX_AC
            abraddr, abrmux = 0, RMUX_AC
         else:
            aop = AOP_OR
            abraddr, abrmux = aaraddr, aarmux
      if mop == MOP_MOV:
         if allow_xor_0 and (marmux == RMUX_IMM) and (maraddr == 0):
            mop = MOP_V8SUBS
            maraddr, marmux = 0, RMUX_AC
            mbraddr, mbrmux = 0, RMUX_AC
         else:
            mop = MOP_V8MIN
            mbraddr, mbrmux = maraddr, marmux

      # normal alu instruction...

      # handle setf
      if asf and (aop == AOP_NOP):
         asm_error('nop.setf is not allowed in add pipe')
      if msf and (mop == MOP_NOP):
         asm_warning('nop.setf, really?')
      if (aop == AOP_NOP) or (acond == COND_NEVER):
         sf = msf
      else:
         if msf:
            asm_error('setf only allowed on mul op if add op is nop or add condition is never')
         sf = asf

      # handle read addrs
      raddr_a = None
      raddr_b = None
      immb = False
      arot_r5 = False
      muxes = [0, 0, 0, 0]
      if mwrot != 0:
         raddr_b = 48 + mwrot
         immb = True
      if mwrot_r5 and have_am:
         raddr_b = 48
         immb = True
      for f in lambda rmux: rmux != RMUX_ANY, lambda rmux: rmux == RMUX_ANY: # do RMUX_ANY last
         for i, raddr, rmux in (0, aaraddr, aarmux), (1, abraddr, abrmux), (2, maraddr, marmux), (3, mbraddr, mbrmux):
            if f(rmux):
               raddr_a, raddr_b, immb, arot_r5, muxes[i] = merge_rmux(raddr_a, raddr_b, immb, arot_r5, raddr, rmux)
      add_a, add_b, mul_a, mul_b = muxes
      if (not read_rot_ok(mul_a, raddr_a, raddr_b)) or (not read_rot_ok(mul_b, raddr_a, raddr_b)):
         # some output elements might not be as expected
         if mwrot_r5 or ((mwrot >= 4) and (mwrot <= 12)):
            bad_elems = 0xffff
         else:
            bad_elems = ((1 << (mwrot & 0x3)) - 1) * 0x1111
            if mwrot > 12:
               bad_elems ^= 0xffff
         bad_elems &= dict(annots).get('mul_used', 0xffff)
         if not msf:
            if mwaddr == WADDR_NOP:
               # not writing anywhere and not setting flags. no elements used
               bad_elems = 0
            elif ((mwaddr in (36, 40, 43, 49, 50, 51)) or
               ((not ws) and (mwaddr == 37))):
               # writing to tmurs/r5rep/unif_addr/unif_addr_rel/stencil/
               # vr_setup/vw_setup/vr_addr/vw_addr/mutex and not setting flags.
               # only use element 0
               bad_elems &= 0x0001
            elif ((mwaddr == 41) or (ws and (mwaddr == 37)) or
               ((not ws) and (mwaddr == 42))):
               # writing to r5quad/x_coord/y_coord/rev_flag and not setting
               # flags. only use elements 0, 4, 8, and 12
               bad_elems &= 0x1111
         if bad_elems:
            asm_warning('mul inputs don\'t come from accumulators (r0-r3). output may not be as expected')
      if raddr_a is None:
         raddr_a = RADDR_NOP
      if raddr_b is None:
         raddr_b = RADDR_NOP
      if immb:
         if sig != SIG_NORMAL:
            asm_error('rotation/immediates and signal don\'t mix')
         sig = SIG_SMALLIMMED
      if arot_r5 or (mwrot_r5 and (not have_am)):
         if sig != SIG_NORMAL:
            asm_error('rotation/immediates/signal don\'t mix')
         sig = SIG_ROTATE

      # construct instruction
      prog.append(((mul_b << 0) | (mul_a << 3) | (add_b << 6) | (add_a << 9) | (raddr_b << 12) | (raddr_a << 18) | (aop << 24) | (mop << 29),
         (mwaddr << 0) | (awaddr << 6) | (ws << 12) | (sf << 13) | (mcond << 14) | (acond << 17) | (pack << 20) | (pm << 24) | (unpack << 25) | (sig << 28),
         line, annots))
   finally:
      current_location = prev_location

def preprocess_passthrough(file):
   line_number = 0
   for line in file:
      line_number += 1
      yield line_number, line

def asm_file(sets, location, filename, preprocess = None):
   global current_dir, current_location

   if filename is None:
      location = '<stdin>'
      file = sys.stdin

      prev_dir = current_dir
   else:
      filename = os.path.normpath(os.path.join(current_dir, filename))

      try:
         file = open(filename)
      except Exception, e:
         asm_error(e)
      except:
         asm_error('unknown error while opening file %s' % filename)

      prev_dir = current_dir
      current_dir = os.path.dirname(filename)

   prev_location = current_location
   current_location = location

   if preprocess is None:
      preprocess = preprocess_passthrough

   try:
      for line_number, line in preprocess(file):
         # strip off comments and whitespace
         line = line.split('#')[0].strip()
         if line == '':
            continue

         asm_line(sets, '%s: %d' % (current_location, line_number), line)
   finally:
      current_dir = prev_dir
      current_location = prev_location

def asm_end_prog():
   # check we aren't in a multi-line construct (eg .macro or .rep)
   if construct != None:
      asm_error({
         CONSTRUCT_MACRO: '.macro without .endm',
         CONSTRUCT_IF:    '.if/.elif without .endif',
         CONSTRUCT_ELSE:  '.else without .endif',
         CONSTRUCT_REP:   '.rep without .endr'}[construct_stack[-1]])

   # check no warnings level back to 0
   if nwarn_level != 0:
      asm_error('.pushnwarn without .popnwarn')

   # flush queued up data
   asm_flush_prog_data()

   # fixup all the label references we can
   for pc in xrange(len(prog)):
      if isinstance(prog[pc][0], tuple):
         location, label, rel, offset = prog[pc][0]
         if label[0].isdigit():
            label_pcs = labels.get(label[:-1], [])
            if label[-1] == 'b':
               label_pcs = filter(lambda label_pc: label_pc <= pc, label_pcs)[-1:]
            else:
               label_pcs = filter(lambda label_pc: label_pc > pc, label_pcs)[:1]
            if label_pcs == []:
               asm_error('search for label reached begin/end of file', location = location)
            imm = label_pcs[0]
         elif label in labels:
            imm = labels[label]
         elif (':' + label) in labels:
            imm = labels[':' + label]
         elif external_link:
            continue # let the external linker deal with it
         else:
            asm_error('undefined label', location = location)
         imm = (imm * 8) + offset
         if rel:
            imm -= (pc + 4) * 8 # relative to instruction after delay slots
            imm &= (1 << 32) - 1
         else:
            if not external_link:
               asm_error('can\'t get absolute address without using an external linker. this mode doesn\'t have an external linker', location = location)
            imm = (location, label, rel, offset, imm)
         prog[pc] = (imm,) + prog[pc][1:]

def asm_init():
   global current_dir, current_location, prog, prog_data, macros, labels, construct, construct_stack, nwarn_level

   current_dir = os.getcwd()
   current_location = ''
   prog = []
   prog_data = []
   macros = {
      'sacq': (['dst', 'i'], [('candyland', 'mov  dst, sacq(i)')]),
      'srel': (['dst', 'i'], [('candyland', 'mov  dst, srel(i)')])}
   labels = {}
   construct = None
   construct_stack = []
   nwarn_level = 0

def asm_reset_prog():
   global prog, labels

   prog = []
   labels = {}

###############################################################################
# dumping
###############################################################################

def print_lines(lines):
   for line in lines:
      print line

class dumper_t:
   def external_link(self): return False
   def begin(self): pass
   def label(self, pc, name): pass
   def line(self, pc, ls, ms, line, annots, first): pass
   def end(self): pass
   def sets(self, sets): pass
   def direct(self, line): pass

class clif_dumper_t(dumper_t):
   def __init__(self):
      self.annot_mode = 0

   def external_link(self):
      return True

   def parse_annot_mode(self, line):
      l = line.split(',')
      self.annot_mode = int(l[0])
      if self.annot_mode not in (0, 1, 2):
         asm_error('bad annot mode')
      if self.annot_mode == 2:
         if len(l) != 2:
            asm_error('expected buffer name')
         self.annot_name = l[1].strip()
         self.annot_offset = 0
      elif len(l) != 1:
         asm_error('unexpected comma')

   def label(self, pc, name):
      if (self.annot_mode != 1) and (name[0] == ':'):
         if self.annot_mode == 2:
            name = name + '_annotations'
         print '@label %s' % name[1:]
      else:
         print '// :%s' % name

   def line(self, pc, ls, ms, line, annots, first):
      if self.annot_mode == 0:
         if isinstance(ls, tuple):
            if len(ls) == 5:
               location, label, rel, offset, offset_from_prog = ls
               assert not rel
               ls = '[. - %d + %d]' % (pc * 8, offset_from_prog)
            else:
               location, label, rel, offset = ls
               if rel:
                  asm_error('relative external label references not allowed in this mode', location = location)
               ls = '[%s + %d]' % (label, offset)
         else:
            ls = '0x%08x' % ls
         print '%s 0x%08x // %s' % (ls, ms, line)
      elif self.annot_mode == 1:
         print '// %s' % line
         for annot in annots:
            print '0x%08x 0x%08x // %s' % ({
               # todo: would rather not have these hard coded
               'mul_used':              1,
               'preserve_cond':         2,
               'geomd_open':            3,
               'geomd_i':               4,
               'geomd_tris_clear':      5,
               'geomd_verts':           6,
               'geomd_tris_add':        7,
               'geomd_tris_set_center': 8,
               'geomd_region_clear':    9,
               'geomd_region_set':      10,
               'geomd_images_clear':    11,
               'geomd_images_l':        12,
               'geomd_images_b':        13,
               'geomd_images_r':        14,
               'geomd_images_t':        15,
               'geomd_images_add_vpm':  16,
               'trace_4c':              17,
               'geomd_images_add_tex':  18,}[annot[0]], annot[1], annot[0])
         if len(annots) != 0:
            print '0x00000000 // end'
      else:
         assert self.annot_mode == 2
         if len(annots) == 0:
            print '0x00000000 // %s' % line
         else:
            print '[%s + %d] // %s' % (self.annot_name, self.annot_offset, line)
            self.annot_offset += (len(annots) * 8) + 4

   def direct(self, line):
      print line

class plain_dumper_t(dumper_t):
   def line(self, pc, ls, ms, line, annots, first):
      print '0x%08x, 0x%08x, // %s' % (ls, ms, line)

class c_c_dumper_t(dumper_t):
   def __init__(self, header_name, full_header_name, array_name):
      self.header_name = header_name
      self.array_name = array_name

   def external_link(self):
      return True

   def begin(self):
      self.external_labels = set()
      self.lines = []

      print '#include "%s.h"' % self.header_name
      print ''
      print '#ifdef _MSC_VER'
      print '   #include <stdint.h>'
      print '   /* cast through uintptr_t to avoid warnings */'
      print '   #define POINTER_TO_UINT(X) ((unsigned int)(uintptr_t)(X))'
      print '#else'
      print '   #define POINTER_TO_UINT(X) ((unsigned int)(X))'
      print '#endif'
      print ''
      print '#ifdef __cplusplus'
      print 'extern "C" { /* the types are probably wrong... */'
      print '#endif'

   def label(self, pc, name):
      self.lines.append('// :%s' % name)

   def line(self, pc, ls, ms, line, annots, first):
      if isinstance(ls, tuple):
         if len(ls) == 5:
            location, label, rel, offset, offset_from_prog = ls
            assert not rel
            ls = 'POINTER_TO_UINT(%s) + %d' % (self.array_name, offset_from_prog)
         else:
            location, label, rel, offset = ls
            if rel:
               asm_error('relative external label references not allowed in this mode', location = location)
            if label not in self.external_labels:
               self.external_labels.add(label)
               print 'extern uint8_t %s[];' % label
            ls = 'POINTER_TO_UINT(%s) + %d' % (label, offset)
      else:
         ls = '0x%08x' % ls
      self.lines.append('/* [0x%08x] */ %s, 0x%08x, // %s' % (pc * 8, ls, ms, line))

   def end(self):
      print '#ifdef __cplusplus'
      print '}'
      print '#endif'
      print ''
      print '#ifdef _MSC_VER'
      print '__declspec(align(8))'
      print '#elif defined(__GNUC__)'
      print '__attribute__((aligned(8)))'
      print '#endif'
      print 'unsigned int %s[] = {' % self.array_name
      print_lines(self.lines)
      print '};'
      print '#ifdef __HIGHC__'
      print '#pragma Align_to(8, %s)' % self.array_name
      print '#endif'

class c_h_dumper_t(dumper_t):
   def __init__(self, header_name, full_header_name, array_name):
      self.full_header_name = full_header_name
      self.array_name = array_name

   def external_link(self):
      return True

   def begin(self):
      print '#ifndef %s_H' % self.full_header_name
      print '#define %s_H' % self.full_header_name
      print ''
      print 'extern unsigned int %s[];' % self.array_name
      print ''

   def label(self, pc, name):
      if name[0] == ':':
         print '#define %s (%s + %d)' % (name[1:], self.array_name, pc * 2)

   def end(self):
      print ''
      print '#endif'

class ml_c_dumper_t(dumper_t):
   def __init__(self, header_name, full_header_name, name, annots):
      self.header_name = header_name
      self.name = name
      self.annots = annots

   def external_link(self):
      return True

   def begin(self):
      if self.annots:
         self.annot_lines = []
      self.lines = []
      self.external_labels = set()
      self.link_lines = []

      print '#include "%s.h"' % self.header_name
      print '#include <assert.h>'
      if self.annots:
         print '#ifdef SIMPENROSE'
         print '#include <stddef.h>'
         print '#include "v3d/verification/tools/2760sim/simpenrose.h"'
      print ''

   def label(self, pc, name):
      self.lines.append('// :%s' % name)

   def line(self, pc, ls, ms, line, annots, first):
      if self.annots:
         if len(annots) == 0:
            self.annot_lines.append('NULL,')
         else:
            print 'static unsigned int const annotations_%d[] = {' % pc
            for annot in annots:
               print '   SIMPENROSE_SHADER_ANNOTATION_%s, 0x%08x,' % (annot[0].upper(), annot[1])
            print '   SIMPENROSE_SHADER_ANNOTATION_END};'
            print ''
            self.annot_lines.append('annotations_%d,' % pc)
      if isinstance(ls, tuple):
         self.link_lines.append('   assert(p[%d] == 0xdeadbeef);' % (pc * 2))
         if len(ls) == 5:
            location, label, rel, offset, offset_from_prog = ls
            assert not rel
            self.link_lines.append('   p[%d] = base + %d;' % (pc * 2, offset_from_prog))
         else:
            location, label, rel, offset = ls
            self.external_labels.add(label)
            if rel:
               self.link_lines.append('   p[%d] = (%s + %d) - (base + %d);' % (pc * 2, label, offset, (pc + 4) * 8))
            else:
               self.link_lines.append('   p[%d] = %s + %d;' % (pc * 2, label, offset))
         ls = '0xdeadbeef'
      else:
         ls = '0x%08x' % ls
      self.lines.append('/* [0x%08x] */ %s, 0x%08x, // %s' % (pc * 8, ls, ms, line))

   def end(self):
      if self.annots:
         print 'unsigned int const *const %s_annotations_array[] = {' % self.name
         print_lines(self.annot_lines)
         print '};'
         print '#endif'
         print ''
      print 'static unsigned int const array[] = {'
      print_lines(self.lines)
      print '};'
      print ''
      print 'void %s_link(void *p_in, unsigned int base' % self.name
      for label in sorted(self.external_labels):
         print '   , unsigned int %s' % label
      print '   )'
      print '{'
      print '   unsigned int *p = (unsigned int *)p_in;'
      print '   unsigned int i;'
      print '   for (i = 0; i != (%s_SIZE / 4); ++i) {' % self.name.upper()
      print '      p[i] = array[i];'
      print '   }'
      print_lines(self.link_lines)
      print '}'

class ml_h_dumper_t(dumper_t):
   def __init__(self, header_name, full_header_name, name, annots):
      self.full_header_name = full_header_name
      self.name = name
      self.annots = annots

   def external_link(self):
      return True

   def begin(self):
      self.external_labels = set()
      self.lines_n = 0

      print '#ifndef %s_H' % self.full_header_name
      print '#define %s_H' % self.full_header_name
      print ''
      if self.annots:
         print '#ifdef SIMPENROSE'
         print '   extern unsigned int const *const %s_annotations_array[];' % self.name
         print '#endif'
         print ''

   def label(self, pc, name):
      if name[0] == ':':
         print '#define %s_OFFSET %d' % (name[1:].upper(), pc * 8)
         if self.annots:
            print '#ifdef SIMPENROSE'
            print '   #define %s_annotations (%s_annotations_array + %d)' % (name[1:], self.name, pc)
            print '#endif'

   def line(self, pc, ls, ms, line, annots, first):
      if isinstance(ls, tuple) and (len(ls) != 5):
         self.external_labels.add(ls[1])
      self.lines_n += 1

   def end(self):
      print ''
      print 'extern void %s_link(void *p, unsigned int base' % self.name
      for label in sorted(self.external_labels):
         print '   , unsigned int %s' % label
      print '   );'
      print ''
      print '#define %s_SIZE %d' % (self.name.upper(), (self.lines_n * 8))
      print ''
      print '#endif'

def print_lines_lc(lines):
   for line in lines:
      print '%s \\' % line

def print_groups_lc(groups):
   first = True
   for group in groups:
      if first:
         print '{ \\'
      else:
         print ', { \\'
      print_lines_lc(group)
      print '} \\'
      first = False

class inline_c_dumper_t(dumper_t):
   def __init__(self, annots):
      self.annots = annots
      self.iteration = False

   def begin_iteration(self):
      assert not self.iteration
      self.iteration = True
      self.iteration_lines = []
      if self.annots:
         self.iteration_annot_lines = []
         self.annot_arrs = []

   def end_iteration(self):
      assert self.iteration
      self.iteration = False
      print '%d, \\' % self.iteration_n
      if self.annots:
         print '( \\'
      print_groups_lc(self.iteration_lines)
      if self.annots:
         print '), ( \\'
         print_groups_lc(self.iteration_annot_lines)
         print '), ( \\'
         for annot_arr in self.annot_arrs:
            print_lines_lc(annot_arr)
         print ') \\'

   def begin(self):
      self.n = 0
      self.lines = []
      if self.annots:
         self.annot_lines = []
         if not self.iteration:
            self.annot_arrs = []

   def label(self, pc, name):
      self.lines.append('/* :%s */' % name)
      if self.annots:
         self.annot_lines.append('/* :%s */' % name)

   def line(self, pc, ls, ms, line, annots, first):
      self.n += 1
      if first:
         prefix = ''
      else:
         prefix = ', '
      self.lines.append('%s0x%08x, 0x%08x /* %s */' % (prefix, ls, ms, line))
      if self.annots:
         if len(annots) == 0:
            a = 'NULL'
         else:
            a = 'annotations_%d' % len(self.annot_arrs)
            annot_arr = ['static unsigned int const annotations_%d[] = {' % len(self.annot_arrs)]
            for annot in annots:
               annot_arr.append('   SIMPENROSE_SHADER_ANNOTATION_%s, 0x%08x,' % (annot[0].upper(), annot[1]))
            annot_arr.append('   SIMPENROSE_SHADER_ANNOTATION_END};')
            self.annot_arrs.append(annot_arr)
         self.annot_lines.append('%s%s /* %s */' % (prefix, a, line))

   def end(self):
      if self.iteration:
         if len(self.iteration_lines) == 0:
            self.iteration_n = self.n
         elif self.iteration_n != self.n:
            asm_error('number of instructions differs between iterations')
         self.iteration_lines.append(self.lines)
         if self.annots:
            self.iteration_annot_lines.append(self.annot_lines)
      else:
         if self.annots:
            print '( \\'
         print_lines_lc(self.lines)
         if self.annots:
            print '), ( \\'
            print_lines_lc(self.annot_lines)
            print '), ( \\'
            for annot_arr in self.annot_arrs:
               print_lines_lc(annot_arr)
            print ') \\'

   def direct(self, line):
      print line

class asvc_dumper_t(dumper_t):
   def external_link(self):
      return True

   def begin(self):
      print '.align 8'

   def label(self, pc, name):
      if name[0] == ':':
         print '%s::' % name[1:]
      else:
         print '%s:' % name

   def line(self, pc, ls, ms, line, annots, first):
      if isinstance(ls, tuple):
         location, label, rel, offset = ls[:4]
         if rel:
            ls = '%s + %d - (. + 32)' % (label, offset)
         else:
            ls = '%s + %d' % (label, offset)
      else:
         ls = '0x%08x' % ls
      print '.word %s, 0x%08x ; %s' % (ls, ms, line)

def is_ra_or_rb(val):
   return isinstance(val, loc_t) and ((val.mux == MUX_A) or (val.mux == MUX_B))

class aliases_dumper_t(dumper_t):
   def external_link(self):
      return True

   def begin(self):
      print '#ifndef JUST_DQASM_ARGS'

   def label(self, pc, name):
      if not name[0].isdigit():
         if name[0] == ':':
            name = name[1:]
         print '"bs%s", "bs%x",' % (name, pc * 8)
         print '"bu%s", "bu%x",' % (name, pc * 8)

   def end(self):
      print '#endif'

   # todo: handle things other than ra and rb? dqasm only allows ra and rb atm
   def sets(self, sets):
      dqasm_args = []
      print '#ifndef JUST_DQASM_ARGS'
      for name in sets:
         if is_ra_or_rb(sets[name]):
            dqasm_args.append('-r%s=%s' % (sets[name], name))
            print '"%s", "%s",' % (name, sets[name])
         elif isinstance(sets[name], list):
            for i, val in enumerate(sets[name]):
               if is_ra_or_rb(val):
                  dqasm_args.append('-r%s=%s[%d]' % (val, name, i))
                  print '"%s[%d]", "%s",' % (name, i, val)
      print '#endif'
      print '#define DQASM_ARGS "%s"' % ' '.join(dqasm_args)

def dump(dumper):
   if (len(prog) != 0) or (len(labels) != 0):
      dumper.begin()

      sorted_labels = []
      for name in labels:
         if name[0].isdigit():
            for pc in labels[name]:
               sorted_labels.append((pc, name))
         else:
            sorted_labels.append((labels[name], name))
      sorted_labels.sort(reverse = True)

      first = True
      for pc in xrange(len(prog)):
         ls, ms, line, annots = prog[pc]
         while (len(sorted_labels) != 0) and (sorted_labels[-1][0] == pc):
            dumper.label(*sorted_labels.pop())
         dumper.line(pc, ls, ms, line, annots, first)
         first = False
      for sorted_label in sorted_labels:
         assert sorted_label[0] == len(prog)
         dumper.label(*sorted_label)

      dumper.end()

###############################################################################
# preprocessing
###############################################################################

def preprocess_inline_c(dumper):
   def preprocess(file):
      ls = None
      line_number = 0
      for line in file:
         line_number += 1
         while True:
            if ls is None:
               l = line.split('%[', 1)
               if len(l) == 1:
                  dumper.direct(l[0].rstrip())
                  break
               dumper.direct('%s \\' % l[0].rstrip())
               line = l[1]
               ls = []
            else:
               l = line.split('%]', 1)
               ls.append((line_number, l[0]))
               if len(l) == 1:
                  break
               line = l[1]
               l = ls[-1][1].split('%|', 1)
               if len(l) == 1:
                  for l_number, l in ls:
                     yield l_number, l
                  asm_end_prog()
                  dump(dumper)
                  asm_reset_prog()
               else:
                  ls[-1] = (ls[-1][0], l[0])
                  if hasattr(dumper, 'begin_iteration'):
                     dumper.begin_iteration()
                  for repls in l[1].split('%,'):
                     repls = [repl.strip() for repl in repls.split('%/')]
                     for l_number, l in ls:
                        for i, repl in enumerate(repls):
                           l = l.replace('%' + str(i), repl)
                        yield l_number, l
                     asm_end_prog()
                     dump(dumper)
                     asm_reset_prog()
                  if hasattr(dumper, 'end_iteration'):
                     dumper.end_iteration()
               ls = None
   return preprocess

def preprocess_clif(dumper):
   def preprocess(file):
      in_asm = False
      line_number = 0
      for line in file:
         line_number += 1
         if in_asm:
            if line.strip() == '%]':
               asm_end_prog()
               dump(dumper)
               asm_reset_prog()
               in_asm = False
            else:
               yield line_number, line
         else:
            if line.strip() == '%[':
               in_asm = True
            elif (line[:1] == '%') and (line[:2] != '%@'):
               yield line_number, line[1:]
            else:
               asm_end_prog()
               dump(dumper)
               asm_reset_prog()
               if line[:2] == '%@':
                  if hasattr(dumper, 'parse_annot_mode'):
                     dumper.parse_annot_mode(line[2:])
               else:
                  dumper.direct(line.rstrip())
   return preprocess

###############################################################################
# main
###############################################################################

def main():
   global external_link, allow_xor_0, dont_warn_when_mul_rot_inp_r5
   global warnings_are_errors, disable_warnings, have_sema, have_am, mulw_rotate

   asm_init() # do this first so we can use asm_error without having to pass a location and so asm_warning will work

   # parse command line
   parser = optparse.OptionParser(usage = 'usage: %prog [options] <filename>')
   parser.add_option('-m', '--mode', dest = 'mode',
      help = '<mode> should be clif, plain, ' +
      'c_c:<header_name>,<full_header_name>,<array_name>, ' +
      'c_h:<header_name>,<full_header_name>,<array_name>, ' +
      'ml_c:<header_name>,<full_header_name>,<name>[,annots], ' +
      'ml_h:<header_name>,<full_header_name>,<name>[,annots], ' +
      'inline_c[:annots], asvc, or aliases[:<preprocess_mode>]', metavar = '<mode>')
   parser.add_option('-t', '--target', dest = 'target',
      help = '<target> should be a0, b0, or hera', metavar = '<target>')
   parser.add_option('-x', '--allow_xor_0', dest = 'allow_xor_0', action = 'store_true', default = False)
   parser.add_option('-r', '--dont_warn_when_mul_rot_inp_r5', dest = 'dont_warn_when_mul_rot_inp_r5', action = 'store_true', default = False)
   parser.add_option('-w', '--warnings_are_errors', dest = 'warnings_are_errors', action = 'store_true', default = False)
   parser.add_option('-d', '--disable_warnings', dest = 'disable_warnings', action = 'store_true', default = False)
   parser.add_option('-s', '--set', dest = 'sets', action = 'append', default = [], metavar = '<name>=<val>')
   options, args = parser.parse_args()
   if len(args) == 0:
      filename = None
   elif len(args) == 1:
      filename = args[0]
   else:
      parser.print_help()
      sys.exit(-1)

   # handle mode
   mode = options.mode or 'clif' # assume clif if no mode specified
   if mode == 'clif':
      dumper = clif_dumper_t()
      preprocess = preprocess_clif(dumper)
   elif mode == 'plain':
      dumper = plain_dumper_t()
      preprocess = None
   elif (mode[:4] == 'c_c:') or (mode[:4] == 'c_h:'):
      mode_options = mode[4:].split(',')
      if len(mode_options) != 3:
         asm_error('badly formatted mode on command line')
      dumper = {'c_c': c_c_dumper_t, 'c_h': c_h_dumper_t}[mode[:3]](*mode_options)
      preprocess = None
   elif (mode[:5] == 'ml_c:') or (mode[:5] == 'ml_h:'):
      mode_options = mode[5:].split(',')
      if (len(mode_options) != 3) and ((len(mode_options) != 4) or (mode_options[3] != 'annots')):
         asm_error('badly formatted mode on command line')
      dumper = {'ml_c': ml_c_dumper_t, 'ml_h': ml_h_dumper_t
         }[mode[:4]](*(mode_options[:3] + [len(mode_options) == 4]))
      preprocess = None
   elif mode == 'inline_c':
      dumper = inline_c_dumper_t(False)
      preprocess = preprocess_inline_c(dumper)
   elif mode == 'inline_c:annots':
      dumper = inline_c_dumper_t(True)
      preprocess = preprocess_inline_c(dumper)
   elif mode == 'asvc':
      dumper = asvc_dumper_t()
      preprocess = None
   elif mode == 'aliases':
      dumper = aliases_dumper_t()
      preprocess = None
   elif mode == 'aliases:inline_c':
      dumper = aliases_dumper_t()
      preprocess = preprocess_inline_c(dumper)
   else:
      asm_error('invalid mode')
   external_link = dumper.external_link()

   # handle target
   target = options.target or 'b0' # assume b0 if no target specified
   if target == 'a0':
      have_sema = False
      have_am = False
      mulw_rotate = False
      have_lthrsw = False
   elif target == 'b0':
      have_sema = True
      have_am = True
      mulw_rotate = True
      have_lthrsw = True
   elif target == 'hera':
      have_sema = True
      have_am = False
      mulw_rotate = True
      have_lthrsw = True
   else:
      asm_error('invalid target')
   if have_am:
      sigs['loadam'] = SIG_LOADAM
      arg_defs['tlbam'] = loc_t(MUX_ANY, 47, 0, 0, None, RW_WRITE)
   if have_lthrsw:
      sigs['lthrsw'] = SIG_LTHRSW
      del sigs['int']
      arg_defs['interrupt'] = loc_t(MUX_ANY, 38, 0, 0, None, RW_WRITE)

   # handle misc options
   allow_xor_0 = options.allow_xor_0
   dont_warn_when_mul_rot_inp_r5 = options.dont_warn_when_mul_rot_inp_r5
   warnings_are_errors = options.warnings_are_errors
   disable_warnings = options.disable_warnings

   # make options visible to asm
   arg_defs['mode'] = mode
   arg_defs['target'] = target

   # arg_defs all setup at this point
   sets = arg_defs.copy() # todo: see arg_eval

   # handle command line sets
   re_options_set = re.compile('(?P<name>\\w+)=(?P<val>.+)$')
   for options_set in options.sets:
      m = re_options_set.match(options_set)
      if not m:
         asm_error('badly formatted set on command line')
      sets[m.group('name')] = arg_eval(m.group('val'), sets)

   # assemble input file and dump
   asm_file(sets, filename, filename, preprocess)
   asm_end_prog()
   dump(dumper)
   for name in arg_defs: # todo: see arg_eval
      del sets[name]
   dumper.sets(sets)

if __name__ == '__main__':
   main()
