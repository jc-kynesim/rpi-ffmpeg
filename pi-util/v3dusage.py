#!/usr/bin/env python

import sys
import argparse
import re

def main():
    argp = argparse.ArgumentParser(description="QPU/VPU perf summary")
    argp.add_argument("logfile")
    args = argp.parse_args()


    rmatch = re.compile(r'^([0-9]+\.[0-9]{3}): (done )?((vpu0)|(vpu1)|(qpu1)) ([A-Z_]+)')

    ttotal = {'vpu0':0.0, 'vpu1':0.0, 'qpu1':0.0, 'idle':0.0}
    tstart = {}
    time0 = None
    idle_start = None

    with open(args.logfile, "rt") as infile:
        for line in infile:
            match = rmatch.match(line)
            if match:
#                print match.group(1), ":", match.group(2), ":", match.group(3), ":", match.group(7), ":"
                time = float(match.group(1))
                unit = match.group(3)
                opstart = not match.group(2)

                if not time0:
                    time0 = time

                if opstart:
                    tstart[unit] = time;
                elif unit in tstart:
                    ttotal[unit] += time - tstart[unit]
                    del tstart[unit]

                if not idle_start and not tstart:
                    idle_start = time
                elif idle_start and tstart:
                    ttotal['idle'] += time - idle_start
                    idle_start = None

    tlogged = time - time0

    print "Logged time:", tlogged
    for unit in ttotal:
        print b'%s: %10.3f' % (unit, ttotal[unit])


if __name__ == '__main__':
   main()

