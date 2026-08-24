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

    for v in validators:
        if not v.isok():
            return False
    return True

def runtest2(name, ffmpeg, args, suffix, validators):

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

def runtest(name, ffmpeg, args, suffix, validators):

    print (f'==== {name}: ', end="")
    sys.stdout.flush()

    rv = runtest2(name, ffmpeg, args, suffix, validators)

    if rv:
        print("ok")
    else:
        print("FAIL")

    return rv


def sayok(log_root, flog):
    print("Woohoo")
    return True

def main():
    argp = argparse.ArgumentParser(description="FFmpeg filter tester")
    argp.add_argument("--ffmpeg", default="./ffmpeg", help="ffmpeg exec name or build directory")
    argp.add_argument("--test_root", default="../streams", help="directory with 3mbit h264/265 jellyfish files")
    args = argp.parse_args()

    if os.path.isdir(args.ffmpeg):
        args.ffmpeg = os.path.join(args.ffmpeg, "ffmpeg")
    if not os.path.isfile(args.ffmpeg):
        print("FFmpeg file '%s' not found" % args.ffmpeg)
        return 2

    if not os.path.isdir(args.test_root):
        print("Jellyfish source dir '%s' not found" % args.test_root)
        return 2
    h264_src = os.path.join(args.test_root, "jellyfish-3-mbps-hd-h264.mkv")
    h265_src = os.path.join(args.test_root, "jellyfish-3-mbps-hd-hevc.mkv")
    if not os.path.isfile(h264_src):
        print(f"Jellyfish 3Mbit H264 source file {h264_src} not found")
    if not os.path.isfile(h265_src):
        print(f"Jellyfish 3Mbit H265 source file {h265_src} not found")

    rv = runtest("H264H264", args.ffmpeg, ["-v", "verbose", "-no_cvt_hw", "-an", "-c:v", "h264_v4l2m2m",
                                   "-i", h264_src,
                                   "-c:v", "h264_v4l2m2m", "-b:v", "2M"], ".mkv",
            [valid_regex(r'Output stream #0:0 \(video\): 900 frames encoded; 900 packets muxed')])

    rv = runtest("H265H264", args.ffmpeg, ["-v", "verbose", "-no_cvt_hw", "-an",
                                   "-hwaccel", "drm", "-init_hw_device", "drm:,v4l2fmts=NC12", "-c:v", "hevc",
                                   "-i", h265_src,
                                   "-c:v", "h264_v4l2m2m", "-b:v", "2M"], ".mkv",
            [valid_regex(r'Output stream #0:0 \(video\): 900 frames encoded; 900 packets muxed')])

    rv = runtest("H265ScaleH264", args.ffmpeg, ["-v", "verbose", "-no_cvt_hw", "-an",
                                   "-hwaccel", "drm", "-init_hw_device", "drm:,v4l2fmts=NC12", "-c:v", "hevc",
                                   "-i", h265_src,
                                   "-vf", "scale_v4l2m2m=w=1280:h=720:format=nv12",
                                   "-c:v", "h264_v4l2m2m", "-b:v", "2M"], ".mkv",
            [valid_regex(r'Output stream #0:0 \(video\): 900 frames encoded; 900 packets muxed')])


    if not rv:
        return 3
    return 0

if __name__ == '__main__':
    exit(main())

