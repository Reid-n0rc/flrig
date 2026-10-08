"""Run flrig against the FT-710 and FTX-1 emulators and fail on any problem.

usage: python3 tests/emu/ci_emu.py path/to/flrig [SCENARIO ...]

The emulator is always started before flrig, and every flrig run has a time
limit (60 s online, 30-45 s exit), so a pop-up can never hang the job.
Serial scenarios use a pseudo-terminal and run on Linux and macOS; TCP/IP
scenarios run everywhere, Windows included.

A run passes when flrig comes online, exits on rig.shutdown in time and does
not crash, the emulator rejects no command, and (FT-710) every reply flrig
read was the command's own and the power and SWR meters never read 0.
Results go to tests/emu/runs/<run>/ (debug_log.txt, cat.log, result.json).
"""
import os, sys, time
import harness, ftx1_harness

HAVE_PTY = os.name == "posix"

# name: (radio, serial?, harness arguments)
SCENARIOS = {
    "ft710_serial":    ("FT-710", True,  dict(xml=[])),
    "ft710_latency":   ("FT-710", True,  dict(xml=[], extra_latency_ms=20)),
    "ft710_dribble":   ("FT-710", True,  dict(xml=[], chunk=3, chunk_gap_ms=2.0)),
    "ft710_meters":    ("FT-710", True,  dict(xml=["rig.get_pwrmeter", "rig.get_swrmeter"],
                                              xml_ms=100)),
    "ft710_tcp":       ("FT-710", False, dict(xml=[], tcp=1)),
    "ft710_tcp_meters": ("FT-710", False, dict(xml=["rig.get_pwrmeter", "rig.get_swrmeter"],
                                               xml_ms=100, tcp=1)),
    "ftx1_read":       ("FTX-1", True,  dict(scen="S1")),
    "ftx1_spa1":       ("FTX-1", True,  dict(scen="S2")),
    "ftx1_field":      ("FTX-1", True,  dict(scen="S3")),
    "ftx1_2m":         ("FTX-1", True,  dict(scen="S4")),
    "ftx1_tcp_read":   ("FTX-1", False, dict(scen="S1", tcp=1)),
    "ftx1_tcp_spa1":   ("FTX-1", False, dict(scen="S2", tcp=1)),
}


def check_ft710(name, binary, kw):
    t0 = time.time()
    s, rows = harness.run("ci_" + name, binary, **kw)
    problems = []
    if s["error"]:
        problems.append(s["error"])
    if s.get("crashed"):
        problems.append("crashed, signal %s" % s.get("signal"))
    if s["emu_rejects"]:
        problems.append("%d commands rejected" % s["emu_rejects"])
    if s["emu_corrupt"]:
        problems.append("corrupt commands %s" % s["emu_corrupt"][:4])
    bad = [r for r in rows if r["status"] != "ok"]
    if bad:
        problems.append("%d replies not the command's own, e.g. %s" % (
            len(bad), ["%s %s %r" % (r["status"], r["label"], r["reply"]) for r in bad[:3]]))
    if not rows:
        problems.append("no replies in debug_log.txt")
    for meth in ("rig.get_pwrmeter", "rig.get_swrmeter"):
        zero = sum(1 for _, m, v in s["xmlres"] if m == meth and str(v) in ("0", "0.0"))
        if zero:
            problems.append("%s read 0 %d times" % (meth, zero))
    return problems, "%d replies ok, %d XML-RPC reads, %.0f s" % (
        len(rows) - len(bad), len(s["xmlres"]), time.time() - t0)


def check_ftx1(name, binary, kw):
    t0 = time.time()
    s = ftx1_harness.run("ci", binary, kw["scen"], tcp=kw.get("tcp", 0))
    problems = []
    if s["error"]:
        problems.append(s["error"])
    rc = s.get("returncode")
    if rc is not None and rc < 0:
        problems.append("crashed, signal %d" % -rc)
    if s["rejected"]:
        problems.append("%d commands rejected: %s" % (s["rejected"], " ".join(s["rejected_cmds"][:8])))
    if not s["ok"]:
        problems.append("no commands reached the emulator")
    return problems, "%d commands ok, %.0f s" % (s["ok"], time.time() - t0)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 64
    binary = os.path.abspath(sys.argv[1])
    names = sys.argv[2:] or list(SCENARIOS)
    os.makedirs(harness.RUNS, exist_ok=True)
    os.makedirs(ftx1_harness.RUNS, exist_ok=True)
    failed = 0
    for name in names:
        radio, serial, kw = SCENARIOS[name]
        if serial and not HAVE_PTY:
            print("%-18s SKIP  (serial needs a pseudo-terminal)" % name, flush=True)
            continue
        try:
            fn = check_ft710 if radio == "FT-710" else check_ftx1
            problems, info = fn(name, binary, dict(kw))
        except Exception as exc:
            problems, info = ["harness error %r" % exc], ""
        if problems:
            failed += 1
            print("%-18s FAIL  %s" % (name, "; ".join(problems)), flush=True)
        else:
            print("%-18s PASS  %s" % (name, info), flush=True)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
