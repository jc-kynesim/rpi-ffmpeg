#!/usr/bin/env python3

import string
import json
import os
import subprocess
import re
import argparse
import sys
import csv
from stat import *

CODEC_HEVC_RPI  = 1
HWACCEL_RPI     = 2
HWACCEL_DRM     = 3
HWACCEL_VAAPI   = 4

aommd5 = "md5_semiplanar_aom"
md5path = '/tmp/out.yuv'

def pixtype(props):
    monochrome = int(props["Monochrome"])
    bits = int(props["Bit depth"])
    subx = int(props["Subsampling X"])
    suby = int(props["Subsampling Y"])

    pix_fmt = []
    if monochrome:
        pix = "grey"
    else:
        if subx and suby:
            pix = "yuv420p"
        elif subx:
            pix = "yuv422p"
        elif not suby:
            pix = "yuv444p"
        else:
            print("SubY but not SubX!")
            return -1
    if bits > 8:
        pix += str(bits) + "le"
    return pix

def testone(args, name, props):
    ffmpeg_exec = args.ffmpeg
    gen_yuv = args.gen_yuv
    valgrind = args.valgrind
    rv = 0

    hwaccel = ""
    if dectype == HWACCEL_RPI:
        hwaccel = "rpi"
    elif dectype == HWACCEL_DRM:
        hwaccel = "drm"
    elif dectype == HWACCEL_VAAPI:
        hwaccel = "vaapi"

    tmp_root = "/tmp/av1conf"

    if not os.path.exists(tmp_root):
        os.makedirs(tmp_root)

    dec_file = os.path.join(tmp_root, name + ".dec.md5")
    try:
        os.remove(dec_file)
    except:
        pass

    yuv_file = os.path.join(tmp_root, name + ".dec.yuv")
    try:
        os.remove(yuv_file)
    except:
        pass

    flog = open(os.path.join(tmp_root, name + ".log"), "w+t")

    ffargs = [ffmpeg_exec, "-flags", "unaligned"] +\
    (["-hwaccel", hwaccel] if hwaccel else []) +\
        ["-no_cvt_hw", "-f", "av1"] +\
        ["-alllayers", "1", "-vcodec", "av1", "-i", props["pathname"]] +\
        ["-noautoscale", "-fps_mode", "passthrough"] +\
        ["-conform_yuv", "1" if gen_yuv else "0"] +\
        ["-f", "conform", yuv_file if gen_yuv else dec_file]

    if valgrind:
        ffargs = ['valgrind', '--leak-check=full'] + ffargs

#    print(ffargs)

    # Unaligned needed for cropping conformance
    rstr = subprocess.call(ffargs, stdout=flog, stderr=subprocess.STDOUT)

    if gen_yuv:
        with open(dec_file, 'wt') as f:
            subprocess.call(["md5sum", yuv_file], stdout=f, stderr=subprocess.STDOUT)

    m1 = None
    m2 = None
    try:
#        m1 = props[aommd5]
        m1 = props["md5"]
        with open(dec_file) as f:
            m2 = re.search("[0-9a-f]{32}", f.readline())[0]
    except:
        pass

    if valgrind:
        flog.seek(0)
        leak = True
        valerr = True

        for line in flog:
            if re.search("^==[0-9]+== All heap blocks were freed", line):
                leak = False
            if re.search("^==[0-9]+== ERROR SUMMARY: 0 errors", line):
                valerr = False
        if leak or valerr:
            rv = 4

    if  m1 and m2 and m1 == m2:
        print("Match: " + m1, file=flog)
    elif not m1:
        print("****** Cannot find m1", file=flog)
        rv = 3
    elif not m2:
        print("****** Cannot find m2", file=flog)
        rv = 2
    else:
        print("****** Mismatch: " + m1 + " != " + m2, file=flog)
        rv = 1
    flog.close()
    return rv

def mergeprops(aconf, name, props):
    if name not in aconf:
        aconf[name] = props
    else:
        overlap = [a for a in props.keys() if a in aconf[name]]
        if overlap:
            print("Merge fail", name, "- overwite:", overlap)
            exit(1)
        aconf[name] |= props


def scanstreams(root, aconf):
    ents = os.listdir(root)
    for name in ents:
        test_path = os.path.join(root, name)
        if S_ISDIR(os.stat(test_path).st_mode):
            scanstreams(test_path, aconf)
            continue

        (base, ext) = os.path.splitext(name)

        if ext == ".obu":
            mergeprops(aconf, base, {"pathname" : test_path})
        elif ext == ".md5" and not re.search("_layer[0-9]+$", base):
            grain_suffix = ""
            if re.search("no_film_grain", root):
                grain_suffix = "_no_grain"
            with open(test_path) as f:
                for line in f:
                    m1 = re.search("[0-9a-f]{32}", line.lower())
                    if m1:
                        key = "md5" + grain_suffix
                        mergeprops(aconf, base, {key: m1[0]})
                        break
        elif ext == ".txt":
            r = re.search("^(level[0-9]+)\\.x_list$", base)
            if r:
                with open(test_path, "rt") as f:
                    for line in f:
                        (b, e) = os.path.splitext(line.strip())
                        if e != '.obu':
                            print("Unexpected line in level file", line)
                        else:
                            mergeprops(aconf, b, {r[1]: True})

    return aconf


def scantests(root, tests = {}):
    ents = os.listdir(root)
    thisdir = os.path.split(root)[1]

    for name in ents:
        test_path = os.path.join(root, name)
        if S_ISDIR(os.stat(test_path).st_mode):
            scantests(test_path, tests)
            continue

        (base, ext) = os.path.splitext(name)

        if ext == ".csv":
            print("Found csv", test_path)
            with open(test_path, "rt") as f:
                aconf = {}
                reader = csv.DictReader(f)
                for row in reader:
                    n = None
                    if "Stream name" in row:
                        sn = "Stream name"
                    if "\ufeffStream name" in row:
                        sn = "\ufeffStream name"
                    if not sn:
                        break
                    (base, ext) = os.path.splitext(row[sn])
                    del row[sn]
                    if ext != ".obu":
                        print("CSV has streamname with unxpected extension:", ext)
                        continue

                    if base not in aconf or "CVS" not in aconf[base]:
                        mergeprops(aconf, base, {"CVS": [row]})
                    else:
                        aconf[base]["CVS"].append(row)

                scanstreams(root, aconf)
                tests[thisdir] = aconf

    return tests


def runtest(name, tests):
    if not tests:
        return True
    for t in tests:
        if name[0:len(t)] == t or name.find("/" + t) != -1:
            return True
    return False


class ConfCSVDialect(csv.Dialect):
    delimiter = ','
    doublequote = True
    lineterminator = '\n'
    quotechar='"'
    quoting = csv.QUOTE_MINIMAL
    skipinitialspace = True
    strict = True

def run_aommd5(aompath, testpath):
    subprocess.run([aompath, '--modules=', '--rawvideo', '--annexb', testpath, '--semiplanar', '--all-layers', '--output='+md5path], check=True)
    md5stat = os.stat(md5path)
    if not md5stat:
        print("Aomdec failed to create yuv file:", md5path)
        exit(1)
    rv = subprocess.run(["md5sum", md5path], check=True, capture_output=True, text=True)
    m2 = re.search("[0-9a-f]{32}", rv.stdout)
    if not m2:
        print("Failed to get md5 of aomdec")
        exit(1)
    return {aommd5: m2[0], "yuv_size": md5stat.st_size}


def mkregfilter(afilter):
    fexp = ""
    if not afilter:
        return re.compile('.')
    for f in afilter:
        if fexp != "":
            fexp += '|'
        fexp += '((^|[^0-9])' + f + '($|[^0-9]))'
    return re.compile(fexp)

def loadcache(args):
    print("Loading tests from cache", args.cache_file)
    with open(args.cache_file, "rt") as f:
        testdict = json.load(f)
    return testdict

if __name__ == '__main__':

    argp = argparse.ArgumentParser(description="FFmpeg h265 conformance tester")
    argp.add_argument("tests", nargs='*')
    argp.add_argument("--pi4", action='store_true', help="Force pi4 cmd line")
    argp.add_argument("--drm", action='store_true', help="Force v4l2 drm cmd line")
    argp.add_argument("--vaapi", action='store_true', help="Force vaapi cmd line")
    argp.add_argument("--test_root", default="/opt/conform/h265.2016", help="Root dir for test")
    argp.add_argument("--csvgen", action='store_true', help="Generate CSV file for dir")
    argp.add_argument("--csv", default="pi-util/conf_h265.2016.csv", help="CSV filename")
    argp.add_argument("--vcodec", default="hevc_rpi", help="vcodec name to use")
    argp.add_argument("--ffmpeg", default="./ffmpeg", help="ffmpeg exec name; if directory given use <dir>/ffmpeg")
    argp.add_argument("--aomdec", help="Path to aomdec for semiplanar md5 gen")
    argp.add_argument("--valgrind", action='store_true', help="Run valgrind on tests")
    argp.add_argument("--gen_yuv", action='store_true', help="Create yuv file (stored with log under /tmp)")
    argp.add_argument("--loop", default=0, type=int, help="Loop n times, or until unexpected result")
    argp.add_argument("--cache_aom_update", action='store_true', help="Update missing aom md5s")
    argp.add_argument("--cache_refresh", action='store_true', help="Refresh the test cache file")
    argp.add_argument("--no_cache", action='store_true', help="Neither read nor write a test cache file")
    argp.add_argument("--cache_file", default='~/.cache/ffav1conf/test_cache.json', help="Test cache filename")
    argp.add_argument("--no_test", action='store_true', help="Do not run tests, just do any cache ops")
    argp.add_argument("--profile", help="Look in profile")
    argp.add_argument("--info", action='store_true', help="Find info test")

    args = argp.parse_args()

    if not args.no_cache:
        args.cache_file = os.path.expanduser(args.cache_file)


    if args.cache_refresh or args.cache_aom_update or args.no_cache or not os.path.exists(args.cache_file):
        if not os.path.isdir(args.test_root):
            print("Test root dir '%s' not found" % args.test_root)
            exit(1)

        if args.cache_aom_update:
            testdict = loadcache(args)
        else:
            print("Scanning tests in", args.test_root)
            testdict = scantests(args.test_root)

        if not testdict:
            print("No tests found. Have you created the .csv(s) from stream documentation.xlsx?")
            exit(1)
        if not args.no_cache:
            print("Writing tests to cache", args.cache_file)
            cpath = os.path.dirname(args.cache_file)
            if cpath:
                os.makedirs(cpath, exist_ok=True)

            for (pname, aconf) in sorted([a for a in testdict.items()], key=lambda a: a[0]):
                if "_not_annex_b" in pname or "_large_scale" in pname:
                    continue
                if "_error" in pname:
                    continue
                for (name, props) in sorted([a for a in aconf.items()], key=lambda a: a[0]):
                    if not ("md5" in props and "pathname" in props and "CVS" in props and "level5" in props):
                        continue
                    if aommd5 in props:
                        continue

                    print(f"\raomdec: {pname} : {name}", end="")
                    mergeprops(testdict[pname], name, run_aommd5(args.aomdec, props["pathname"]))
            print()

            with open(args.cache_file, "wt") as f:
                json.dump(testdict, f)
    else:
        testdict = loadcache(args)

    if args.no_test:
        exit(0)

    if not args.info:
        dectype = HWACCEL_DRM
        if args.vaapi:
            dectype = HWACCEL_VAAPI

        if os.path.isdir(args.ffmpeg):
            args.ffmpeg = os.path.join(args.ffmpeg, "ffmpeg")
        if not os.path.isfile(args.ffmpeg):
            print("FFmpeg file '%s' not found" % args.ffmpeg)
            exit(1)

    tfilter = mkregfilter(args.tests)
    pfilter = mkregfilter(args.profile)

    i = 0
    successes = 0
    failures = 0
    unx_failures = []
    unx_success = []
    while True:
        i = i + 1
        if args.loop:
            print("== Loop ", i)
        tfound = False
        for (pname, aconf) in sorted([a for a in [b for b in testdict.items() if pfilter.search(b[0])]], key=lambda a: a[0]):
            for (tname, props) in sorted([a for a in [b for b in aconf.items() if tfilter.search(b[0])]], key=lambda a: a[0]):
                cname = pname + " : " + tname
                if args.info:
                    print("----", cname)
                    print("  Path:          ", props["pathname"])
                    for c in props["CVS"]:
                        print(f'  Pixfmt{c["CVS number"]}:       ', pixtype(c) + " " + c["Max picture width"] + "x" + c["Max picture height"])
                    print("  YUV MD5:       ", props["md5"])
                    if aommd5 in props:
                        print("  Semiplanar MD5:", props[aommd5])
                    if "yuv_size" in props:
                        print("  YUV file size: ", props["yuv_size"])
                    tfound = True
                else:
                    if not ("md5" in props and "CVS" in props and "level5" in props):
                        continue

                    print("----", cname, end="", flush=True)
                    rv = testone(args, tname, props)
                    tfound = True

                    if (rv == 0):
                        successes += 1
                    else:
                        failures += 1
                        unx_failures.append(cname)

                    if (rv == 0):
                        print(": ok")
                    elif rv == 1:
                        print(": FAIL")
                    elif (rv == 2) :
                        print(": * CRASH *")
                    elif (rv == 3) :
                        print(": * MD5 MISSING *")
                    elif (rv == 4) :
                        print(": * VALGRIND *")
                    else :
                        print(": * BANG *")

        if not tfound:
            print("Couldn't find tests", args.tests, "in tests")
            exit(1)
        if args.loop <= i:
            break

    if successes or failures:
        print(f"Tests passed: {successes}, failed: {failures}")

    if unx_failures:
        print("Tests failed: ", unx_failures)

