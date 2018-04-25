#!/usr/bin/env python

import sys
import argparse
import re

def do_logparse(logname):

    rmatch = re.compile(r'^([0-9]+\.[0-9]{3}): (done )?((vpu0)|(vpu1)|(qpu1)) ([A-Z_]+) cb:([0-9a-f]+) ')
    rqcycle = re.compile(r'^([0-9]+\.[0-9]{3}): v3d: QPU Total clock cycles for all QPUs doing vertex/coordinate shading +([0-9]+)$')
    rqtscycle = re.compile(r'^([0-9]+\.[0-9]{3}): v3d: QPU Total clock cycles for all QPUs stalled waiting for TMUs +([0-9]+)$')
    rl2hits = re.compile(r'^([0-9]+\.[0-9]{3}): v3d: L2C Total Level 2 cache ([a-z]+) +([0-9]+)$')

    ttotal = {'idle':0.0}
    tstart = {}
    qctotal = {}
    qtstotal = {}
    l2hits = {}
    l2total = {}
    time0 = None
    idle_start = None
    qpu_op_no = 0
    op_count = 0

    with open(logname, "rt") as infile:
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

            match = rqcycle.match(line)
            if match:
                unit = "qpu1." + str(qpu_op_no)
                if not unit in qctotal:
                    qctotal[unit] = 0
                qctotal[unit] += int(match.group(2))

            match = rqtscycle.match(line)
            if match:
                unit = "qpu1." + str(qpu_op_no)
                if not unit in qtstotal:
                    qtstotal[unit] = 0
                qtstotal[unit] += int(match.group(2))

            match = rl2hits.match(line)
            if match:
                unit = "qpu1." + str(qpu_op_no)
                if not unit in l2total:
                    l2total[unit] = 0
                    l2hits[unit] = 0
                l2total[unit] += int(match.group(3))
                if match.group(2) == "hits":
                    l2hits[unit] += int(match.group(3))


    if not time0:
        print "No v3d profile records found"
    else:
        tlogged = time - time0

        print "Logged time:", tlogged, "  Op count:", op_count
        for unit in sorted(ttotal):
            print b'%6s: %10.3f    %7.3f%%' % (unit, ttotal[unit], ttotal[unit] * 100.0 / tlogged)
        print
        for unit in sorted(qctotal):
            if not unit in qtstotal:
                qtstotal[unit] = 0;
            print b'%6s: Qcycles: %10d, TMU stall: %10d (%7.3f%%)' % (unit, qctotal[unit], qtstotal[unit], (qtstotal[unit] * 100.0)/qctotal[unit])
            if unit in l2total:
                print b'        L2Total: %10d, hits:      %10d (%7.3f%%)' % (l2total[unit], l2hits[unit], (l2hits[unit] * 100.0)/l2total[unit])



if __name__ == '__main__':
    argp = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="QPU/VPU perf summary from VC logging",
        epilog = """
Will also summarise TMU stalls if logging requests set in qpu noflush param
in the profiled code.

Example use:
  vcgencmd set_logging level=0xc0
  <command to profile>
  sudo vcdbg log msg >& t.log
  v3dusage.py t.log
""")

    argp.add_argument("logfile")
    args = argp.parse_args()

    do_logparse(args.logfile)

