#!/usr/bin/env python3

import time
import string
import os
import tempfile
import subprocess
import re
import argparse
import sys
import csv
from stat import *

class tstats:
    close_threshold = 0.01

    def __init__(self, stats_dict=None):
        if stats_dict != None:
            self.name = stats_dict["name"]
            self.elapsed = float(stats_dict["elapsed"])
            self.user = float(stats_dict["user"])
            self.sys = float(stats_dict["sys"])

    def times_str(self):
        ctime = self.sys + self.user
        return "time=%6.2f, cpu=%6.2f (%4.2f%%)" % (self.elapsed, ctime, (ctime * 100.0) / self.elapsed)

    def dict(self):
        return {"name":self.name, "elapsed":self.elapsed, "user":self.user, "sys":self.sys}

    def is_close(self, other):
        return abs(self.elapsed - other.elapsed) / self.elapsed < self.close_threshold

    def __lt__(self, other):
        return self.elapsed < other.elapsed
    def __gt__(self, other):
        return self.elapsed > other.elapsed

    def time_file(name, prefix, ffmpeg="./ffmpeg"):
        stats = tstats()
        stats.name = name
        start_time = time.clock_gettime(time.CLOCK_MONOTONIC);
        cproc = subprocess.Popen([ffmpeg, "-no_cvt_hw",
                                  "-vcodec", "hevc_rpi",
                                  "-t", "30", "-i", prefix + name,
                                  "-f", "vout_rpi", os.devnull], bufsize=-1, stdout=flog, stderr=flog);
        pinfo = os.wait4(cproc.pid, 0)
        end_time = time.clock_gettime(time.CLOCK_MONOTONIC);
        stats.elapsed = end_time - start_time
        stats.user = pinfo[2].ru_utime
        stats.sys = pinfo[2].ru_stime
        return stats


def common_prefix(s1, s2):
    for i in range(min(len(s1),len(s2))):
        if s1[i] != s2[i]:
            return s1[:i]
    return s1[:i+1]

def main():
    global flog

    argp = argparse.ArgumentParser(description="FFmpeg performance tester", epilog="""
To blank the screen before starting use "xdg-screensaver activate"
(For some reason this doesn't seem to work from within python).
""")

    argp.add_argument("streams", nargs='*')
    argp.add_argument("--csv_out", default="ffperf_out.csv", help="CSV output filename")
    argp.add_argument("--csv_in", help="CSV input filename")
    argp.add_argument("--prefix", help="Filename prefix (include terminal '/' if a directory).")
    argp.add_argument("--repeat", default=3, type=int, help="Run repeat count")
    argp.add_argument("--ffmpeg", default="./ffmpeg", help="FFmpeg executable")

    args = argp.parse_args()

    csv_out = csv.DictWriter(open(args.csv_out, 'w', newline=''), ["name", "elapsed", "user", "sys"])
    csv_out.writeheader()

    stats_in = {}
    if args.csv_in != None:
        with open(args.csv_in, 'r', newline='') as f_in:
            stats_in = {x["name"]:tstats(x) for x in csv.DictReader(f_in)}

    flog = open(os.path.join(tempfile.gettempdir(), "ffperf.log"), "wt")

    streams = args.streams
    if not streams:
        if not stats_in:
            print ("No source streams specified")
            return 1
        prefix = "" if args.prefix == None else args.prefix
        streams = [k for k in stats_in]
    elif args.prefix != None:
        prefix = args.prefix
    else:
        prefix = streams[0]
        for f in streams[1:]:
            prefix = common_prefix(prefix, f)
        pp = prefix.rpartition(os.sep)
        prefix = pp[0] + pp[1]
        streams = [s[len(prefix):] for s in streams]

    for f in sorted(streams, key=lambda x : "~" * x.count(os.sep) + x.lower()):
        print ("====", f)

        t0 = tstats({"name":f, "elapsed":999, "user":999, "sys":999})
        for i in range(args.repeat):
            t = tstats.time_file(f, prefix, args.ffmpeg)
            print ("...", t.times_str())
            if t0 > t:
                t0 = t

        if t0.name in stats_in:
            pstat = stats_in[t0.name]
            print("---" if pstat.is_close(t0) else "<<<" if t0 < pstat else ">>>", pstat.times_str())

        csv_out.writerow(t0.dict())

        print ()

    return 0


if __name__ == '__main__':
    exit(main())

