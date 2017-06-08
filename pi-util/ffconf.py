#!/usr/bin/env python

import os
import subprocess
import re
import argparse
import sys
import csv
from stat import *

conf_root = "/opt/conform/h265"
ffmpeg_exec = "./ffmpeg"

def testone(fileroot, name, es_file, md5_file):
    tmp_root = "/tmp"

    dec_file = os.path.join(tmp_root, name + ".dec.md5")
    try:
        os.remove(dec_file)
    except:
        pass

    flog = open(os.path.join(tmp_root, name + ".log"), "wt")

    # Unaligned needed for cropping conformance
    rstr = subprocess.call(
        [ffmpeg_exec, "-flags", "unaligned", "-vcodec", "hevc", "-i", os.path.join(fileroot, es_file), "-f", "md5", dec_file],
        stdout=flog, stderr=subprocess.STDOUT)

    try:
        m1 = None
        m2 = None
        with open(os.path.join(fileroot, md5_file)) as f:
            for line in f:
                m1 = re.search("[0-9a-f]{32}", line.lower())
                if m1:
                    break

        with open(dec_file) as f:
            m2 = re.search("[0-9a-f]{32}", f.readline())
    except:
        pass

    if  m1 and m2 and m1.group() == m2.group():
        print >> flog, "Match: " + m1.group()
        rv = 0
    elif not m1:
        print >> flog, "****** Cannot find m1"
        rv = 3
    elif not m2:
        print >> flog, "****** Cannot find m2"
        rv = 2
    else:
        print >> flog, "****** Mismatch: " + m1.group() + " != " + m2.group()
        rv = 1
    flog.close()
    return rv

def scandir(root):
    aconf = []
    ents = os.listdir(conf_root)
    ents.sort(key=str.lower)
    for name in ents:
        test_path = os.path.join(conf_root, name)
        if S_ISDIR(os.stat(test_path).st_mode):
            files = os.listdir(test_path)
            es_file = "?"
            md5_file = "?"
            for f in files:
                (base, ext) = os.path.splitext(f)
                if base[0] == '.':
                    pass
                elif ext == ".bit" or ext == ".bin":
                    es_file = f
                elif ext == ".md5":
                    if md5_file == "?":
                        md5_file = f
                    elif base[-3:] == "yuv":
                        md5_file = f
            aconf.append((1, name, es_file, md5_file))
    return aconf

def runtest(name, tests):
    if not tests:
        return True
    for t in tests:
        if name[0:len(t)] == t:
            return True
        return False

def doconf(csva, tests):
    failures = []
    unx_success = []
    for a in csva:
        exp_test = int(a[0])
        if (exp_test and runtest(a[1], tests)):
            name = a[1]
            print "==== ", name,
            sys.stdout.flush()

            rv = testone(os.path.join(conf_root, name), name, a[2], a[3])
            if (rv == 0):
                if exp_test == 2:
                    print ": * OK *"
                    unx_success.append(name)
                else:
                    print ": ok"
            elif exp_test > 1 and rv == 1:
                print ": fail"
            else:
                failures.append(name)
                if rv == 1:
                    print ": * FAIL *"
                elif (rv == 2) :
                    print ": * CRASH *"
                elif (rv == 3) :
                    print ": * MD5 MISSING *"
                else :
                    print ": * BANG *"

    if failures or unx_success:
        print "Unexpected Failures:", failures
        print "Unexpected Success: ", unx_success
    else:
        print "All tests normal"


class ConfCSVDialect(csv.Dialect):
    delimiter = ','
    doublequote = True
    lineterminator = '\n'
    quotechar='"'
    quoting = csv.QUOTE_MINIMAL
    skipinitialspace = True
    strict = True

if __name__ == '__main__':

    argp = argparse.ArgumentParser(description="FFmpeg h265 conformance tester")
    argp.add_argument("tests", nargs='*')
    argp.add_argument("--csvgen", action='store_true', help="Generate CSV file for dir")
    argp.add_argument("--csv", default="pi-util/conf_h265.csv", help="CSV filename")
    args = argp.parse_args()

    if args.csvgen:
        csv.writer(sys.stdout).writerows(scandir(conf_root))
        exit(0)

    with open(args.csv, 'rt') as csvfile:
        csva = [a for a in csv.reader(csvfile, ConfCSVDialect())]


    doconf(csva, args.tests)

