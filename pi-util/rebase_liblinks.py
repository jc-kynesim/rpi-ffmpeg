#!/usr/bin/env python

import os, sys
from stat import *

def walktree(top, callback, n, prefix):
    '''recursively descend the directory tree rooted at top,
       calling the callback function for each regular file'''

    for f in os.listdir(top):
        pathname = os.path.join(top, f)
        mode = os.lstat(pathname).st_mode
        if S_ISDIR(mode):
            # It's a directory, recurse into it
            walktree(pathname, callback, n+1, prefix)
        elif S_ISLNK(mode):
            # It's a file, call the callback function
            callback(pathname, os.readlink(pathname), n, prefix)

def visitfile(file, linkname, n, prefix):
    if (linkname.startswith(prefix + 'lib/')):
        newlink = "../" * n + linkname[len(prefix):]
        print 'relinking', file, "->", newlink
        os.remove(file)
        os.symlink(newlink, file)

if __name__ == '__main__':
    argc = len(sys.argv)
    if argc == 2:
        walktree(sys.argv[1], visitfile, 0, "/")
    elif argc == 3:
        walktree(sys.argv[1], visitfile, 0, sys.argv[2])
    else:
        print "rebase_liblinks.py <local root> [<old sysroot>]"



