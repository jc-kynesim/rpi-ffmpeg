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

    def time_file(name, prefix):
        stats = tstats()
        stats.name = name
        start_time = time.clock_gettime(time.CLOCK_MONOTONIC);
        cproc = subprocess.Popen(["./ffmpeg", "-t", "30", "-i", prefix + name,
                                  "-f", "null", os.devnull], bufsize=-1, stdout=flog, stderr=flog);
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
    argp = argparse.ArgumentParser(description="FFmpeg performance compare")

    argp.add_argument("stream0", help="CSV to compare")
    argp.add_argument("stream1", nargs='?', default="ffperf_out.csv", help="CSV to compare")

    args = argp.parse_args()

    with open(args.stream0, 'r', newline='') as f_in:
        stats0 = {x["name"]:tstats(x) for x in csv.DictReader(f_in)}
    with open(args.stream1, 'r', newline='') as f_in:
        stats1 = {x["name"]:tstats(x) for x in csv.DictReader(f_in)}

    print (args.stream0, "<<-->>", args.stream1)
    print ()

    for f in sorted(stats0.keys() | stats1.keys(), key=lambda x : "~" * x.count(os.sep) + x.lower()):
       if not (f in stats0) :
           print ("           XX               :", f)
           continue
       if not (f in stats1) :
           print ("       XX                   :", f)
           continue

       s0 = stats0[f]
       s1 = stats1[f]

       pcent = ((s0.elapsed - s1.elapsed) / s0.elapsed) * 100.0
       thresh = 0.3
       tc = 6

       nchar = min(tc - 1, int(abs(pcent) / thresh))
       cc = "  --  " if nchar == 0 else "<" * nchar + " " * (tc - nchar) if pcent < 0 else " " * (tc - nchar) + ">" * nchar

       print ("%6.2f %s%6.2f (%+5.2f) : %s" %
           (s0.elapsed, cc, s1.elapsed, pcent, f))

    return 0


if __name__ == '__main__':
    exit(main())

