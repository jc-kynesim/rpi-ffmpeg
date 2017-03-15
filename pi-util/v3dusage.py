#!/usr/bin/env python

import sys
import argparse
import re

def main():
    argp = argparse.ArgumentParser(description="QPU/VPU perf summary")
    argp.add_argument("logfile")
    args = argp.parse_args()


    rmatch = re.compile(r'^([0-9]+\.[0-9]{3}): (done )?((vpu0)|(vpu1)|(qpu1)) ([A-Z_]+) cb:([0-9a-f]+) ')

    ttotal = {'idle':0.0}
    tstart = {}
    time0 = None
    idle_start = None
    qpu_op_no = 0
    op_count = 0

    with open(args.logfile, "rt") as infile:
        for line in infile:
            match = rmatch.match(line)
            if match:
#                print match.group(1), ":", match.group(2), ":", match.group(3), ":", match.group(7), ":"
                time = float(match.group(1))
                unit = match.group(3)
                opstart = not match.group(2)
                optype = match.group(7)
                hascb = match.group(8) != "0"

                if unit == 'qpu1':
                    unit = unit + "." + str(qpu_op_no)
                    if not opstart:
                        if hascb or optype == 'EXECUTE_SYNC':
                            qpu_op_no = 0
                        else:
                            qpu_op_no += 1

                # Ignore sync type
                if optype == 'EXECUTE_SYNC':
                    continue

                if not time0:
                    time0 = time

                if opstart:
                    tstart[unit] = time;
                elif unit in tstart:
                    op_count += 1
                    if not unit in ttotal:
                        ttotal[unit] = 0.0
                    ttotal[unit] += time - tstart[unit]
                    del tstart[unit]

                if not idle_start and not tstart:
                    idle_start = time
                elif idle_start and tstart:
                    ttotal['idle'] += time - idle_start
                    idle_start = None

    tlogged = time - time0

    print "Logged time:", tlogged, "  Op count:", op_count
    for unit in sorted(ttotal):
        print b'%6s: %10.3f    %7.3f%%' % (unit, ttotal[unit], ttotal[unit] * 100.0 / tlogged)


if __name__ == '__main__':
   main()

