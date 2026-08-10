#!/usr/bin/env python3

import string
import os
import subprocess
import re
import argparse
import sys
import csv
from stat import *

class validator:
    def __init__(self, ok=False):
        self.ok = ok
        self.fail_msg = "Failed"

    def isok(self):
        return self.ok

    def setok(self):
        self.ok = True

    def setfail(self):
        self.ok = False

    def setmsg(self, msg):
        self.fail_msg = msg[:]

    def failmsg(self):
        return self.fail_msg

class valid_regex(validator):
    def __init__(self, regex):
        super().__init__()
        self.regex = re.compile(regex)

    def scanline(self, line):
        if self.isok() or self.regex.search(line):
            self.setok()

class invalid_regex(validator):
    def __init__(self, regex, fail_msg):
        super().__init__(True)
        self.setmsg(fail_msg)
        self.regex = re.compile(regex)

    def scanline(self, line):
        if not self.isok():
            return
        if self.regex.search(line):
            self.setfail()

def validate(validators, flog):
    for line in flog:
        for v in validators:
            v.scanline(line)

    ok = True
    for v in validators:
        if not v.isok():
            ok = False
            # complain
            print(v.failmsg())
            break

    if ok:
        print("OK")
    return ok

def runtest(name, ffmpeg, args, suffix, validators):

    print(f'==== {name}: ', end="", flush=True)

    log_root = os.path.join("/tmp", "testfilt", name)
    ofilename = os.path.join(log_root, name + suffix)

    if not os.path.exists(log_root):
        os.makedirs(log_root)

    try:
        os.remove(ofilename)
    except:
        pass

    flog = open(os.path.join(log_root, name + ".log"), "wb")
    ffargs = [ffmpeg] + args + [ofilename]

    subprocess.call(ffargs, stdout=flog, stderr=subprocess.STDOUT, text=False)
    flog.close

    flog = open(os.path.join(log_root, name + ".log"), "rt")
    return validate(validators, flog)

def main():
    argp = argparse.ArgumentParser(description="FFmpeg filter tester")
    argp.add_argument("--ffmpeg", default="./ffmpeg", help="ffmpeg exec name")
    argp.add_argument("--test_root", default="../streams", help="Root dir for test")
    args = argp.parse_args()

    if not os.path.isdir(args.test_root):
        print("Test root dir '%s' not found" % args.test_root)
        return 2

    if os.path.isdir(args.ffmpeg):
        args.ffmpeg = os.path.join(args.ffmpeg, "ffmpeg")
    if not os.path.isfile(args.ffmpeg):
        print("FFmpeg file '%s' not found" % args.ffmpeg)
        return 2

    src_file = os.path.join(args.test_root, "jellyfish-3-mbps-hd-h264.mkv")
    if not os.path.isfile(src_file):
        print("Test source file '%s' not found" % src_file)
        return 2

    runtest("ATest", args.ffmpeg, ["-v", "verbose", "-no_cvt_hw", "-an", "-c:v", "h264_v4l2m2m", "-i",
                                   src_file, "-c:v", "h264_v4l2m2m", "-b:v", "2M"], ".mkv",
            [invalid_regex(r'Could not find a valid device', "Device not found (not Pi4?)"),
             valid_regex(r'Output stream #0:0 \(video\): 900 frames encoded; 900 packets muxed')])
    return 0

if __name__ == '__main__':
    exit(main())

