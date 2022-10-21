#!/usr/bin/env python3

import string
import os
import subprocess
import re
import argparse
import sys
import csv
from stat import *

def runtest(name, ffmpeg, args, suffix, validate):
    log_root = os.path.join("/tmp", "testfilt", name);
    ofilename = os.path.join(log_root, name + suffix);

    if not os.path.exists(log_root):
        os.makedirs(log_root)

    try:
        os.remove(ofilename)
    except:
        pass

    flog = open(os.path.join(log_root, name + ".log"), "wb")
    ffargs = [ffmpeg] + args + [ofilename];

    subprocess.call(ffargs, stdout=flog, stderr=subprocess.STDOUT, text=False)
    flog.close

    flog = open(os.path.join(log_root, name + ".log"), "rt")
    return validate(log_root, flog)

def sayok(log_root, flog):
    print("Woohoo")
    return True

if __name__ == '__main__':

    argp = argparse.ArgumentParser(description="FFmpeg filter tester")
    argp.add_argument("--ffmpeg", default="./ffmpeg", help="ffmpeg exec name")
    args = argp.parse_args()

    runtest("ATest", args.ffmpeg, ["-v", "verbose", "-no_cvt_hw", "-an", "-c:v", "h264_v4l2m2m", "-i", "/home/jc/rpi/streams/jellyfish-3-mbps-hd-h264.mkv",
                                   "-c:v", "h264_v4l2m2m", "-b:v", "2M"], ".mkv", sayok)
