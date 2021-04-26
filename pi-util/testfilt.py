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
    def __init__(self):
        self.ok = False

    def isok(self):
        return self.ok

    def setok(self):
        self.ok = True

class valid_regex(validator):
    def __init__(self, regex):
        super().__init__()
        self.regex = re.compile(regex)

    def scanline(self, line):
        if self.isok() or self.regex.search(line):
            self.setok()


def validate(validators, flog):
    for line in flog:
        for v in validators:
            v.scanline(line)

    ok = True
    for v in validators:
        if not v.isok():
            ok = False
            # complain
            print("Test failed")

    if ok:
        print("OK")
    return ok

def runtest(name, ffmpeg, args, suffix, validators):
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

def sayok(log_root, flog):
    print("Woohoo")
    return True

if __name__ == '__main__':

    argp = argparse.ArgumentParser(description="FFmpeg filter tester")
    argp.add_argument("--ffmpeg", default="./ffmpeg", help="ffmpeg exec name")
    args = argp.parse_args()

    runtest("ATest", args.ffmpeg, ["-v", "verbose", "-no_cvt_hw", "-an", "-c:v", "h264_v4l2m2m", "-i",
                                   "/home/johncox/server/TestMedia/Sony/jellyfish-10-mbps-hd-h264.mkv",
#                                    "/home/jc/rpi/streams/jellyfish-3-mbps-hd-h264.mkv",
                                   "-c:v", "h264_v4l2m2m", "-b:v", "2M"], ".mkv",
            [valid_regex(r'Output stream #0:0 \(video\): 900 frames encoded; 900 packets muxed')])
