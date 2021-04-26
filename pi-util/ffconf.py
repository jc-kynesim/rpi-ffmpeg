#!/usr/bin/env python3

import string
import os
import subprocess
import re
import argparse
import sys
import csv
from stat import *

class DecodeType:
    def __init__(self, textname, hwaccel):
        self.textname = textname
        self.hwaccel = hwaccel

hwaccel_rpi = DecodeType("RPI Test/Legacy", "rpi")
hwaccel_sw = DecodeType("Software", None)
hwaccel_drm = DecodeType("DRM Prime", "drm")
hwaccel_vaapi = DecodeType("VAAPI", "vaapi")

def testone(fileroot, srcname, es_file, md5_file, pix, dectype, vcodec, args):
    ffmpeg_exec = args.ffmpeg
    gen_yuv = args.gen_yuv
    valgrind = args.valgrind
    rv = 0

    pix_fmt = []
    if pix == "8":
        pix_fmt = ["-pix_fmt", "yuv420p"]
    elif pix == "10":
        pix_fmt = ["-pix_fmt", "yuv420p10le"]
    elif pix == "12":
        pix_fmt = ["-pix_fmt", "yuv420p12le"]

    tmp_root = "/tmp"

    names = srcname.split('/')
    while len(names) > 1:
        tmp_root = os.path.join(tmp_root, names[0])
        del names[0]
    name = names[0]

    if not os.path.exists(tmp_root):
        os.makedirs(tmp_root)

    dec_file = os.path.join(tmp_root, name + ".dec.md5")
    try:
        os.remove(dec_file)
    except:
        pass

    yuv_file = os.path.join(tmp_root, name + ".dec.yuv")
    try:
        os.remove(yuv_file)
    except:
        pass

    flog = open(os.path.join(tmp_root, name + ".log"), "w+t")

    ffargs = [ffmpeg_exec, "-flags", "unaligned"] +\
        (["-hwaccel", dectype.hwaccel] if dectype.hwaccel else []) +\
        ["-vcodec", "hevc", "-i", os.path.join(fileroot, es_file)] +\
        pix_fmt +\
        ([yuv_file] if gen_yuv else ["-f", "md5", dec_file])

    if valgrind:
        ffargs = ['valgrind', '--leak-check=full'] + ffargs

    # Unaligned needed for cropping conformance
    rstr = subprocess.call(ffargs, stdout=flog, stderr=subprocess.STDOUT)

    if gen_yuv:
        with open(dec_file, 'wt') as f:
            subprocess.call(["md5sum", yuv_file], stdout=f, stderr=subprocess.STDOUT)

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

    if valgrind:
        flog.seek(0)
        leak = True
        valerr = True

        for line in flog:
            if re.search("^==[0-9]+== All heap blocks were freed", line):
                leak = False
            if re.search("^==[0-9]+== ERROR SUMMARY: 0 errors", line):
                valerr = False
        if leak or valerr:
            rv = 4

    if  m1 and m2 and m1.group() == m2.group():
        print("Match: " + m1.group(), file=flog)
    elif not m1:
        print("****** Cannot find m1", file=flog)
        rv = 3
    elif not m2:
        print("****** Cannot find m2", file=flog)
        rv = 2
    else:
        print("****** Mismatch: " + m1.group() + " != " + m2.group(), file=flog)
        rv = 1
    flog.close()
    return rv

def scandir(root):
    aconf = []
    ents = os.listdir(root)
    ents.sort(key=str.lower)
    for name in ents:
        test_path = os.path.join(root, name)
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
                elif ext == ".md5" or (ext == ".txt" and (base[-4:] == "_md5" or base[-6:] == "md5sum")):
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
        if name[0:len(t)] == t or name.find("/" + t) != -1:
            return True
    return False

def doconf(csva, tests, test_root, vcodec, dectype, args):
    unx_failures = []
    unx_success = []
    failures = 0
    successes = 0
    for a in csva:
        exp_test = int(a[0])
        if (exp_test and runtest(a[1], tests)):
            name = a[1]
            print ("==== ", name, end="")
            sys.stdout.flush()

            rv = testone(os.path.join(test_root, name), name, a[2], a[3], a[4], dectype=dectype, vcodec=vcodec, args=args)
            if (rv == 0):
                successes += 1
            else:
                failures += 1

            if (rv == 0):
                if exp_test == 2:
                    print(": * OK *")
                    unx_success.append(name)
                else:
                    print(": ok")
            elif exp_test == 2 and rv == 1:
                print(": fail")
            elif exp_test == 3 and rv == 2:
                # Call an expected "crash" an abort
                print(": abort")
            else:
                unx_failures.append(name)
                if rv == 1:
                    print(": * FAIL *")
                elif (rv == 2) :
                    print(": * CRASH *")
                elif (rv == 3) :
                    print(": * MD5 MISSING *")
                elif (rv == 4) :
                    print(": * VALGRIND *")
                else :
                    print(": * BANG *")

    print()
    print("Tested using decode type:", dectype.textname)
    if unx_failures or unx_success:
        print("Unexpected Failures:", unx_failures)
        print("Unexpected Success: ", unx_success)
    else:
        print("All tests normal:", successes, "ok,", failures, "failed")

    return unx_failures + unx_success


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
    argp.add_argument("--pi4", action='store_true', help="Force pi4 cmd line")
    argp.add_argument("--drm", action='store_true', help="Force v4l2 drm cmd line")
    argp.add_argument("--sw", action='store_true', help="Use software decode")
    argp.add_argument("--vaapi", action='store_true', help="Force vaapi cmd line")
    argp.add_argument("--test_root", default="/opt/conform/h265.2016", help="Root dir for test")
    argp.add_argument("--csvgen", action='store_true', help="Generate CSV file for dir")
    argp.add_argument("--csv", default="pi-util/conf_h265.2016.csv", help="CSV filename")
    argp.add_argument("--vcodec", default="hevc_rpi", help="vcodec name to use")
    argp.add_argument("--ffmpeg", default="./ffmpeg", help="ffmpeg exec name; if directory given use <dir>/ffmpeg")
    argp.add_argument("--valgrind", action='store_true', help="Run valgrind on tests")
    argp.add_argument("--gen_yuv", action='store_true', help="Create yuv file (stored with log under /tmp)")
    argp.add_argument("--loop", default=0, type=int, help="Loop n times, or until unexpected result")
    args = argp.parse_args()

    if not os.path.isdir(args.test_root):
        print("Test root dir '%s' not found" % args.test_root)
        exit(1)

    if args.csvgen:
        csv.writer(sys.stdout).writerows(scandir(args.test_root))
        exit(0)

    with open(args.csv, 'rt') as csvfile:
        csva = [a for a in csv.reader(csvfile, ConfCSVDialect())]

    dectype = None
    if os.path.exists("/dev/rpivid-hevcmem"):
        dectype = hwaccel_rpi
    if args.drm or os.path.exists("/sys/module/rpivid_hevc") or os.path.exists("/sys/module/rpi_hevc_dec"):
        dectype = hwaccel_drm

    if args.pi4:
        dectype = hwaccel_rpi
    elif args.drm:
        dectype = hwaccel_drm
    elif args.vaapi:
        dectype = hwaccel_vaapi
    elif args.sw:
        dectype = hwaccel_sw

    if os.path.isdir(args.ffmpeg):
        args.ffmpeg = os.path.join(args.ffmpeg, "ffmpeg")
    if not os.path.isfile(args.ffmpeg):
        print("FFmpeg file '%s' not found" % args.ffmpeg)
        exit(1)

    if not dectype:
        print("No decode type selected and no h/w detected")
        exit(1)
    print("Running test using decode:", dectype.textname)

    i = 0
    while True:
        i = i + 1
        if args.loop:
            print("== Loop ", i)
        if doconf(csva, args.tests, args.test_root, args.vcodec, dectype, args) or (args.loop >= 0 and i > args.loop):
            break

