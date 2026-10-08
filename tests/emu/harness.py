"""Run flrig (FT-710 driver) against ft710_emu, key TX over XML-RPC, poll
meters, then classify every wait_char reply in flrig's debug log.

usage: python3 harness.py NAME BUILD [key=value ...]
  emu keys : baud proc_ms extra_latency_ms chunk chunk_gap_ms ai_period_ms ai_force
  run keys : serloop (ms) stimeout (serial_timeout ms) tx (s) xml (comma list
             of rig.* meter methods polled during TX) xml_ms (poll interval)
             tcpip (unused)
"""
import json, os, re, shutil, socket, subprocess, sys, threading, time
import xmlrpc.client
from ft710_emu import FT710, serve, now_ms

HERE = os.path.dirname(os.path.abspath(__file__))
RUNS = os.path.join(HERE, "runs")
PORT = 12399

FLRIG_PREFS = """; FLTK preferences file format 1.0
; vendor: w1hkj.com
; application: flrig

[.]

xcvr_name:FT-710
"""

XCVR_PREFS = """; FLTK preferences file format 1.0
; vendor: w1hkj.com
; application: FT-710

[.]

version:{version}
xcvr_serial_port:{port}
serial_baudrate:7
serial_stopbits:1
serial_retries:2
serial_write_delay:0
serial_post_write_delay:0
serial_timeout:{stimeout}
serloop_timing:{serloop}
ptt_via_cat:1
ptt_via_rts:0
ptt_via_dtr:0
rts_cts_flow:0
rts_plus:0
dtr_plus:0
use_tcpip:{tcp}
tcpip_addr:127.0.0.1
tcpip_port:4011
xmlport:{xmlport}
use_rig_data:1
restore_rig_data:0
poll_smeter:1
poll_frequency:1
poll_mode:1
poll_bandwidth:1
poll_pout:1
poll_swr:1
poll_alc:1
poll_power_control:1
poll_split:1
poll_voltage:1
poll_ptt:1
"""

# label in wait_char trace -> expected answer prefix(es)
EXPECT = {
    "get pout": ("RM5",), "get swr": ("RM6",), "get alc": ("RM4", "RM7"),
    "get vdd": ("RM8",), "get idd": ("RM7",), "get smeter": ("SM0",), "get PTT": ("TX",),
    "get vfo A": ("FA",), "get vfo B": ("FB",), "get vfoAorB()": ("VS",),
    "Get split": ("FT",), "get power": ("PC",), "check": ("ID",),
    "get mode A": ("MD",), "get mode B": ("MD",), "get bw A": ("SH0",),
    "get bw B": ("SH0",), "get vol": ("AG0",), "get mic": ("MG",),
    "get rfgain": ("RG0",), "get squelch": ("SQ0",), "get if shift": ("IS0",),
    "get notch on/off": ("BP",), "get notch val": ("BP",),
    "get auto notch": ("BC",), "get att": ("RA0",), "get pre": ("PA0",),
    "get NB": ("NB0",), "GET noise reduction": ("NR0",),
    "GET noise reduction val": ("RL0",), "get break in": ("BI",),
    "get tune": ("AC",), "get band": ("IF",),
}
EXPLEN = {"RM": 10, "SM": 7, "TX": 4, "FA": 12, "FB": 12, "VS": 4, "FT": 4,
          "PC": 6, "ID": 7}

WC = re.compile(r"\[(\d\d:\d\d:\d\d\.\d+)\] D: wait_char: (.*?): read (\d+) bytes in (\d+) msec, (\d+) tries, ?(.*)$")


# --- crash detection (H1): a signal death or a new macOS crash report ---
CRASH_DIR = os.path.expanduser("~/Library/Logs/DiagnosticReports")


def crash_reports(binary):
    """Set of existing DiagnosticReports .ips files for this binary."""
    base = os.path.basename(binary) + "-"
    try:
        return {f for f in os.listdir(CRASH_DIR) if f.startswith(base) and f.endswith(".ips")}
    except OSError:
        return set()


def _ips_pid(path):
    """pid recorded in an .ips report (header line, then a JSON body)."""
    try:
        txt = open(path, errors="replace").read()
        body = json.loads(txt.split("\n", 1)[1])
        return body.get("pid")
    except Exception:
        return None


def crash_info(proc, binary, before, harness_killed=False, wait_s=10.0):
    """Call after proc has ended.  before = crash_reports(binary) taken just
    before Popen.  Returns dict(crashed, signal, crash_report, returncode).
    A signal death counts as a crash unless the harness itself killed the
    process (timeout kill); a new .ips report for this pid always counts."""
    rc = proc.returncode
    sig = -rc if rc is not None and rc < 0 else None
    by_signal = sig is not None and not harness_killed
    report = None
    t_end = time.time() + (wait_s if by_signal else 1.0)   # ReportCrash lags a few s
    while True:
        new = sorted(crash_reports(binary) - before)
        mine = [f for f in new if _ips_pid(os.path.join(CRASH_DIR, f)) in (proc.pid, None)]
        if mine:
            report = os.path.join(CRASH_DIR, mine[-1]); break
        if time.time() >= t_end:
            break
        time.sleep(0.5)
    return dict(crashed=bool(by_signal or report), signal=sig,
                crash_report=report, returncode=rc)


def wait_port_free(timeout=120):
    t0 = time.time()
    while time.time() - t0 < timeout:
        sk = socket.socket()
        try:
            sk.bind(("127.0.0.1", PORT)); return True
        except OSError:
            time.sleep(1)
        finally:
            sk.close()
    return False


def wait_online(proxy, proc, timeout=60):
    t0 = time.time()
    while time.time() - t0 < timeout:
        if proc.poll() is not None:
            return False
        try:
            if proxy.rig.get_xcvr() and proxy.rig.get_vfo():
                time.sleep(2); return True
        except Exception:
            pass
        time.sleep(0.5)
    return False


def classify(debug_path):
    rows, alc_seen = [], 0
    separate_idd = 'get idd' in open(debug_path, errors='replace').read()
    for line in open(debug_path, errors="replace"):
        m = WC.search(line)
        if not m:
            continue
        ts, label, n, ms, tries, reply = m.groups()
        exp = EXPECT.get(label)
        if not exp:
            continue
        if label == "get alc" and not separate_idd:   # old builds share a label
            exp = (("RM4",), ("RM7",))[alc_seen % 2]; alc_seen += 1
        frames = [f + ";" for f in reply.split(";")[:-1]]
        tail = reply.split(";")[-1]
        ok_prefix = any(f.startswith(exp) for f in frames)
        status = "ok"
        if not frames:
            status = "EMPTY" if not reply else "TRUNC"
        elif not frames[-1].startswith(exp) or not ok_prefix:
            status = "WRONG"                 # parser rfinds; last/any frame
        elif len(frames) > 1 or tail:
            status = "EXTRA"
        if status == "ok":
            want = EXPLEN.get(exp[0][:2])
            if want and len(frames[-1]) != want:
                status = "LEN"
        rows.append(dict(ts=ts, label=label, exp=exp[0], n=int(n),
                         ms=int(ms), reply=reply, status=status))
    return rows


def run(name, binary, version="2.0.12", stimeout=50, serloop=50, tx=8.0,
        xml=("rig.get_pwrmeter", "rig.get_swrmeter"), xml_ms=0, tcp=0, **emu_kw):
    cfg = os.path.join(RUNS, name)
    shutil.rmtree(cfg, ignore_errors=True)
    os.makedirs(cfg)
    emu = FT710(log_path=os.path.join(cfg, "cat.log"), **emu_kw)
    stop = threading.Event()
    if tcp:
        from ft710_tcp import serve_tcp
        serve_tcp(emu, stop, 4011)
        try:                    # a real but unused serial port, as before
            import pty as _pty
            _m, slave_fd = _pty.openpty(); port = os.ttyname(slave_fd)
        except ImportError:     # Windows
            slave_fd, port = None, "NONE"
    else:
        port, slave_fd = serve(emu, stop)
    open(os.path.join(cfg, "flrig.prefs"), "w").write(FLRIG_PREFS)
    open(os.path.join(cfg, "FT-710.prefs"), "w").write(XCVR_PREFS.format(
        version=version, port=port, stimeout=stimeout, serloop=serloop,
        xmlport=PORT, tcp=int(tcp)))
    wait_port_free()
    out = open(os.path.join(cfg, "stdout.txt"), "w")
    before = crash_reports(binary)
    proc = subprocess.Popen([binary, "--config-dir", cfg, "--debug-level", "4"],
                            stdout=out, stderr=subprocess.STDOUT)
    proxy = xmlrpc.client.ServerProxy(f"http://127.0.0.1:{PORT}")
    xmlres, error, killed = [], None, False
    try:
        if not wait_online(proxy, proc):
            error = "flrig did not come online"
        else:
            emu.w("MARK", f"{now_ms():.1f}", "PTT ON")
            proxy.rig.set_ptt(1)
            t_end = time.time() + tx
            time.sleep(0.5)
            while time.time() < t_end:
                for meth in xml:
                    try:
                        v = getattr(proxy, meth)()
                    except Exception as e:
                        v = f"EXC {e}"
                    xmlres.append((round(time.time(), 3), meth, v))
                    if xml_ms:
                        time.sleep(xml_ms / 1000.0)
                if not xml:
                    time.sleep(0.2)
            proxy.rig.set_ptt(0)
            emu.w("MARK", f"{now_ms():.1f}", "PTT OFF")
            time.sleep(1.0)
            try:
                proxy.rig.shutdown()
            except Exception:
                pass
            try:
                proc.wait(timeout=30)
            except subprocess.TimeoutExpired:
                error = "flrig did not exit"
    except Exception as e:      # XML-RPC fails if flrig died mid-run
        error = f"run aborted: {e!r}"
    finally:
        if proc.poll() is None:
            proc.kill(); proc.wait(); killed = True
        out.close()
        time.sleep(0.3)
        stop.set()
        if slave_fd is not None:
            os.close(slave_fd)
    crash = crash_info(proc, binary, before, harness_killed=killed)
    if crash["crashed"]:
        error = f"CRASH signal={crash['signal']} report={crash['crash_report']}" + (f"; {error}" if error else "")
    dbg = os.path.join(cfg, "debug_log.txt")
    rows = classify(dbg) if os.path.exists(dbg) else []
    summ = dict(name=name, binary=binary, error=error, emu=emu_kw,
                stimeout=stimeout, serloop=serloop, xml=xml, xml_ms=xml_ms,
                counts={}, xmlres=xmlres, **crash)
    for r in rows:
        summ["counts"][r["status"]] = summ["counts"].get(r["status"], 0) + 1
    # emulator side: corrupted commands, rejects, RM5 double-send split
    rx = [l.rstrip("\n").split("\t") for l in open(os.path.join(cfg, "cat.log"), errors="replace")]
    cmds = [f[2] for f in rx if f[0] == "RX"]
    summ["emu_rejects"] = sum(1 for f in rx if f[0] == "REJ")
    summ["emu_corrupt"] = sorted(set(repr(c) for c in cmds if any(ord(ch) < 32 or ord(ch) > 126 for ch in c) or not re.fullmatch(r"[A-Z0-9+\-]+;", c)))
    split = 0
    for i, c in enumerate(cmds[:-1]):
        if c == "RM5;" and (i == 0 or cmds[i-1] != "RM5;") and cmds[i+1] != "RM5;":
            split += 1
    summ["rm5_split"] = split
    json.dump(dict(summary=summ, rows=rows), open(os.path.join(cfg, "result.json"), "w"), indent=1)
    return summ, rows


def parse_val(v):
    try:
        return float(v)
    except Exception:
        return v


if __name__ == "__main__":
    name, binary = sys.argv[1], sys.argv[2]
    kw = {}
    for a in sys.argv[3:]:
        k, v = a.split("=", 1)
        if k == "xml":
            kw[k] = tuple(x for x in v.split(",") if x)
        elif k == "proc_ms" and "," in v:
            kw[k] = [float(x) for x in v.split(",")]
        elif k in ("ai_force",):
            kw[k] = v in ("1", "true")
        elif k in ("version", "tx_rm"):
            kw[k] = v
        else:
            kw[k] = parse_val(v)
            if isinstance(kw[k], float) and kw[k].is_integer() and k in ("baud", "chunk", "stimeout", "serloop", "ai_period_ms", "xml_ms", "tcp"):
                kw[k] = int(kw[k])
    s, rows = run(name, binary, **kw)
    bad = [r for r in rows if r["status"] != "ok"]
    vals = {}
    for _, meth, v in s["xmlres"]:
        vals.setdefault(meth, {}).setdefault(str(v), 0)
        vals[meth][str(v)] += 1
    print(f"{name}: error={s['error']} counts={s['counts']} emu_rej={s['emu_rejects']} corrupt={s['emu_corrupt'][:4]} rm5_split={s['rm5_split']} xml={vals}")
    for r in bad[:12]:
        print(f"   {r['ts']} {r['status']:5s} {r['label']:<14s} want {r['exp']:<4s} got {r['reply']!r} ({r['ms']} ms)")
