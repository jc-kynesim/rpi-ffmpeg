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

global flog

class tstats:
    close_threshold = 0.01

    def __init__(self, stats_dict):
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

def time_file(name):
    short_name= os.path.split(name)[1];
    start_time = time.clock_gettime(time.CLOCK_MONOTONIC);
    cproc = subprocess.Popen(["./ffmpeg", "-t", "30", "-i", name, "-f", "null", os.devnull], bufsize=-1, stdout=flog, stderr=flog);
    pinfo = os.wait4(cproc.pid, 0)
    end_time = time.clock_gettime(time.CLOCK_MONOTONIC);
    stats = tstats({"name":short_name, "elapsed":end_time - start_time, "user":pinfo[2].ru_utime, "sys":pinfo[2].ru_stime})
    return stats


if __name__ == '__main__':

    argp = argparse.ArgumentParser(description="FFmpeg performance tester")
    argp.add_argument("streams", nargs='*')
    argp.add_argument("--csv_out", default="ffperf_out.csv", help="CSV output filename")
    argp.add_argument("--csv_in", help="CSV input filename")

    args = argp.parse_args()

    csv_out = csv.DictWriter(open(args.csv_out, 'w', newline=''), ["name", "elapsed", "user", "sys"])
    csv_out.writeheader()

    stats_in = {}
    if args.csv_in != None:
        with open(args.csv_in, 'r', newline='') as f_in:
            stats_in = {x["name"]: tstats(x) for x in csv.DictReader(f_in)}

    flog = open(os.path.join(tempfile.gettempdir(), "ffperf.log"), "wt")

    for f in args.streams:
        short_name= os.path.split(f)[1];
        print ("====", short_name)

        t0 = tstats({"name":short_name, "elapsed":999, "user":999, "sys":999})
        for i in range(3):
            t = time_file(f)
            print ("...", t.times_str())
            if t0 > t:
                t0 = t

        if t0.name in stats_in:
            pstat = stats_in[t0.name]
            print("---" if pstat.is_close(t0) else "<<<" if t0 < pstat else ">>>", pstat.times_str())

        csv_out.writerow(t0.dict())

        print ()



