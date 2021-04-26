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
    def __init__(self, textname, hwaccel, nameprefix):
        self.textname = textname
        self.hwaccel = hwaccel
        self.prefix = nameprefix

    def checkname(self, name):
        for x in self.prefix:
            if name.startswith(x):
                return True;
        return False

hwaccel_rpi = DecodeType("RPI Test/Legacy", "rpi", [])
hwaccel_sw = DecodeType("Software", None, ["yuv", "gray"])
hwaccel_drm = DecodeType("DRM Prime", "drm", ["drm"])
hwaccel_vaapi = DecodeType("VAAPI", "vaapi", [])

allaccel = [
    hwaccel_rpi, hwaccel_sw, hwaccel_drm, hwaccel_vaapi
]

def testone(fileroot, srcname, es_file, md5_file, pix, dectype, vcodec, args):
    ffmpeg_exec = args.ffmpeg
    gen_yuv = args.gen_yuv
    valgrind = args.valgrind
    rv = 0

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
        ["-no_cvt_hw", "-flags", "output_corrupt"] +\
        (["-init_hw_device", f"drm:,v4l2fmts={"/".join(args.v4l2fmts)}"] if args.v4l2fmts else []) +\
        (["-hwaccel", dectype.hwaccel] if dectype.hwaccel else []) +\
        ["-vcodec", "hevc", "-i", os.path.join(fileroot, es_file)] +\
        ["-conform_corrupt", "1"] +\
        (["-conform_out", "file", "-f", "conform", yuv_file] if gen_yuv else ["-conform_out", "md5", "-f", "conform", dec_file])

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

    flog.seek(0)
    leak = True
    valerr = True
    frametype = None
    v4l2fmt = "????"

    for line in flog:
        sv = re.search(r'^ *Stream #[0-9]+:[0-9]+: Video: wrapped.+, ([A-Za-z0-9-_]+)\(', line)
        if sv:
            for a in allaccel:
                if a.checkname(sv.group(1)):
                    frametype = a
                    break

        sv = re.search(r'^\[hevc .*Hwaccel V4L2.*V4L2fmt (.+)$', line)
        if sv:
            v4l2fmt = sv.group(1)

        if re.search("^==[0-9]+== All heap blocks were freed", line):
            leak = False
        if re.search("^==[0-9]+== ERROR SUMMARY: 0 errors", line):
            valerr = False
    if valgrind and (leak or valerr):
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
    return (rv, frametype, v4l2fmt)

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
    unx_match = []
    unx_nomatch = []

    failures = 0
    successes = 0
    for a in csva:
        exp_test = int(a[0])
        if (exp_test and runtest(a[1], tests)):
            name = a[1]
            print ("==== ", name, end="")
            sys.stdout.flush()

            (rv, frametype, v4l2fmt) = testone(os.path.join(test_root, name), name, a[2], a[3], a[4], dectype=dectype, vcodec=vcodec, args=args)

            if (rv == 0):
                successes += 1
            else:
                failures += 1

            comments = []
            is_unx_nomatch = False

            if args.v4l2fmts and frametype == hwaccel_drm and v4l2fmt not in args.v4l2fmts:
                comments.append(v4l2fmt)
                is_unx_nomatch = True

            sw_expected = int(a[5])
            if frametype != dectype:
                if frametype:
                    if sw_expected and frametype == hwaccel_sw:
                        comments.append(frametype.textname.lower())
                    else:
                        comments.append(frametype.textname.upper())
                        is_unx_nomatch = True
                else:
                    comments.append("????")
                    if exp_test == 0:
                        is_unx_nomatch = True

            elif sw_expected and dectype != hwaccel_sw:
                comments.append(frametype.textname.upper())
                unx_match.append(name)

            if is_unx_nomatch:
                unx_nomatch.append(name)

            if comments:
                print(f" ({",".join(comments)})", end="")

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
    print(f"Tested using decode: {dectype.textname}, Frame type: {args.hwfmt}")
    if unx_failures or unx_success or unx_match or unx_nomatch:
        print("Unexpected Failures:", unx_failures)
        print("Unexpected Success: ", unx_success)
        print("Unexpected Format Success: ", unx_match)
        print("Unexpected Format Fail: ", unx_nomatch)
    else:
        print("All tests normal:", successes, "ok,", failures, "failed")

    return len(unx_failures) + len(unx_success) + len(unx_nomatch)


class ConfCSVDialect(csv.Dialect):
    delimiter = ','
    doublequote = True
    lineterminator = '\n'
    quotechar='"'
    quoting = csv.QUOTE_MINIMAL
    skipinitialspace = True
    strict = True



def main():
    argp = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="FFmpeg H.265 conformance tester",
        epilog="""\
Return values:
   0   Tests passed
   1   Python crashed
   2   Setup issue
   3   Tests failed
""")
    argp.add_argument("tests", nargs='*')
    argp.add_argument("--pi4", action='store_true', help="Force pi4 cmd line")
    argp.add_argument("--drm", action='store_true', help="Force v4l2 drm cmd line")
    argp.add_argument("--sw", action='store_true', help="Use software decode")
    argp.add_argument("--hwfmt", default="default", help="Force h/w format (sand, oldsand, nv12), default is to use 1st offered")
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
        return 2

    if args.csvgen:
        csv.writer(sys.stdout).writerows(scandir(args.test_root))
        return 0

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

    if args.hwfmt == "default":
        args.v4l2fmts = None
    elif args.hwfmt == "sand":
        args.v4l2fmts = ["Nc12","Nc30"]
    elif args.hwfmt == "oldsand":
        args.v4l2fmts = ["NC12","NC30"]
    elif args.hwfmt == "nv":
        args.v4l2fmts = ["NV12","P010"]
    else:
        print("Unexpected hwfmt: sand, oldsand, nv expected")
        exit(1)

    if os.path.isdir(args.ffmpeg):
        args.ffmpeg = os.path.join(args.ffmpeg, "ffmpeg")
    if not os.path.isfile(args.ffmpeg):
        print("FFmpeg file '%s' not found" % args.ffmpeg)
        return 2

    if not dectype:
        print("No decode type selected and no h/w detected")
        return 2
    print(f"Running test using decode: {dectype.textname}, Frame type: {args.hwfmt}")

    errs = 0
    i = 0
    while not errs:
        i = i + 1
        if args.loop:
            print("== Loop ", i)
        errs = doconf(csva, args.tests, args.test_root, args.vcodec, dectype, args)
        if (args.loop >= 0 and i >= args.loop):
            break

    if errs:
        return 3
    return 0

if __name__ == '__main__':
    exit(main())

