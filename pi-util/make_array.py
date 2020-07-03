#!/usr/bin/env python

# Usage
#   make_array file.bin
#   Produces file.h with array of bytes.
#
import sys
for file in sys.argv[1:]:
  prefix,suffix = file.split('.')
  assert suffix=='bin'
  name=prefix.split('/')[-1]
  print 'Converting',file
  with open(prefix+'.h','wb') as out:
    print >>out, 'static const unsigned char',name,'[] = {'
    with open(file,'rb') as fd:
      i = 0
      for byte in fd.read():
        print >>out, '0x%02x, ' % ord(byte),
        i = i + 1
        if i % 8 == 0:
          print >>out, ' // %04x' % (i - 8)
    print >>out,'};'

