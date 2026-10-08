"""Run flrig builds against the strict FTX-1 emulator and log every CAT
exchange.  usage: python3 harness.py BUILD=/path/to/flrig [...] [-s S1 S2]"""
import json, os, shutil, socket, subprocess, sys, threading, time, xmlrpc.client
from ftx1_emu import FTX1, serve, serve_tcp

HERE = os.path.dirname(os.path.abspath(__file__))
RUNS = os.path.join(HERE, "runs")

FLRIG_PREFS = """; FLTK preferences file format 1.0
; vendor: w1hkj.com
; application: flrig

[.]

xcvr_name:FTX-1
"""

XCVR_PREFS = """; FLTK preferences file format 1.0
; vendor: w1hkj.com
; application: FTX-1

[.]

version:2.0.12
xcvr_serial_port:{port}
serial_baudrate:7
serial_stopbits:1
serial_retries:2
serial_write_delay:0
serial_post_write_delay:5
serial_timeout:50
ptt_via_cat:1
ptt_via_rts:0
ptt_via_dtr:0
rts_cts_flow:0
rts_plus:0
dtr_plus:0
use_tcpip:{tcp}
tcpip_addr:127.0.0.1
tcpip_port:4011
xmlport:12399
use_rig_data:{use_rig_data}
restore_rig_data:1
restore_mode:1
{extra}"""

# settings flrig writes to the radio at startup when use_rig_data is 0
WRITE_SETTINGS = """bool_noise:1
nb_level:5
noise_reduction:1
noise_red_val:4
int_preamp:2
int_att:1
bool_shift:1
int_shift:200
bool_notch:1
int_notch:1500
compON:1
compression:30
dbl_power:{power}
rfgain:80
squelch:10
int_mic:40
vox_onoff:1
vox_gain:60
vox_hang:700
vox_on_dataport:1
cw_weight:3.5
cw_qsk:20
cw_delay:450
cw_vol:40
cw_wpm:22
cw_spot_tone:650
vfo_adj:-7
mode_A:{mode}
bw_A:5
freq_A_u:0
freq_A_l:{freq}
"""

# XML-RPC actions, run in order; each is (method, args...)
ACTIONS = [
    ("rig.get_vfoA",), ("rig.get_vfoB",), ("rig.get_mode",), ("rig.get_bw",),
    ("rig.set_frequency", 7150000.0), ("rig.set_mode", "LSB"), ("rig.set_bw", 3),
    ("rig.set_mode", "PSK"), ("rig.set_bw", 9), ("rig.set_mode", "AM"),
    ("rig.set_mode", "FM-N"), ("rig.set_mode", "CW-U"), ("rig.set_bw", 7),
    ("rig.set_mode", "USB"), ("rig.set_bw", 12),
    ("rig.set_AB", "B"), ("rig.set_mode", "DATA-U"), ("rig.set_bw", 10),
    ("rig.set_frequency", 14074000.0), ("rig.set_AB", "A"),
    ("rig.set_split", 1), ("rig.get_split",), ("rig.set_split", 0),
    ("rig.set_volume", 30), ("rig.set_rfgain", 60), ("rig.set_micgain", 45),
    ("rig.set_power", 25), ("rig.set_notch", 1200), ("rig.get_smeter",),
    ("rig.get_pwrmeter",), ("rig.get_swrmeter",), ("rig.swap",),
    ("rig.swap",), ("rig.get_maxpwr",), ("rig.tune", 1),
]

SCENARIOS = {
    "S1": dict(desc="read radio state, SPA-1, xmlrpc actions",
               head="spa1", use_rig_data=1, extra="", actions=ACTIONS),
    "S2": dict(desc="write saved settings, SPA-1, 20 m USB",
               head="spa1", use_rig_data=0,
               extra=WRITE_SETTINGS.format(power=50, mode=1, freq=14200000),
               emu=dict(pr={"0": "2", "1": "1"}, pl=30),
               actions=[("rig.get_vfoA",), ("rig.get_power",)]),
    "S3": dict(desc="write saved settings, field head 7.5 W",
               head="field", use_rig_data=0,
               extra=WRITE_SETTINGS.format(power=7.5, mode=1, freq=14200000),
               actions=[("rig.get_power",), ("rig.set_power", 4),
                        ("rig.get_maxpwr",)]),
    "S4": dict(desc="write saved settings, SPA-1, 2 m FM",
               head="spa1", use_rig_data=0,
               extra=WRITE_SETTINGS.format(power=20, mode=3, freq=145500000),
               actions=[("rig.get_vfoA",)]),
}


def wait_port_free(port=12399, timeout=120):
    t0 = time.time()
    while time.time() - t0 < timeout:
        sk = socket.socket()
        try:
            sk.bind(("0.0.0.0", port))
            return True
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
                time.sleep(3)
                return True
        except Exception:
            pass
        time.sleep(0.5)
    return False


def run(build_name, binary, scen_name, tcp=0):
    sc = SCENARIOS[scen_name]
    tag = f"{scen_name}-{build_name}" + ("-tcp" if tcp else "")
    cfg = os.path.join(RUNS, tag)
    shutil.rmtree(cfg, ignore_errors=True)
    os.makedirs(cfg)
    emu = FTX1(head=sc["head"], log_path=os.path.join(cfg, "cat.log"))
    for key, value in sc.get("emu", {}).items():
        setattr(emu, key, value)
    stop = threading.Event()
    if tcp:
        serve_tcp(emu, stop)
        port, slave_fd = "NONE", None
    else:
        port, slave_fd, _ = serve(emu, stop)
    open(os.path.join(cfg, "flrig.prefs"), "w").write(FLRIG_PREFS)
    open(os.path.join(cfg, "FTX-1.prefs"), "w").write(XCVR_PREFS.format(
        port=port, use_rig_data=sc["use_rig_data"], extra=sc["extra"],
        tcp=int(bool(tcp))))

    wait_port_free()
    out = open(os.path.join(cfg, "stdout.txt"), "w")
    proc = subprocess.Popen([binary, "--config-dir", cfg, "--debug-level", "4"],
                            stdout=out, stderr=subprocess.STDOUT)
    proxy = xmlrpc.client.ServerProxy("http://127.0.0.1:12399")
    results, error = [], None
    try:
        if not wait_online(proxy, proc):
            error = "flrig did not come online"
        else:
            for act in sc["actions"]:
                try:
                    r = getattr(proxy, act[0])(*act[1:])
                except Exception as e:
                    r = f"EXC {e}"
                results.append([act[0], list(act[1:]), r])
                time.sleep(0.6)
            try:
                proxy.rig.shutdown()
            except Exception:
                pass
            try:
                proc.wait(timeout=45)
            except subprocess.TimeoutExpired:
                error = "flrig did not exit"
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()
        out.close()
        time.sleep(0.5)
        stop.set()
        if slave_fd is not None:
            os.close(slave_fd)

    rejected = []
    for line in open(os.path.join(cfg, "cat.log")):
        if line.startswith("REJECT"):
            rejected.append(line.split("\t")[1])
    summary = dict(scenario=scen_name, build=build_name, error=error,
                   returncode=proc.returncode,
                   ok=emu.counts["ok"], rejected=emu.counts["rejected"],
                   rejected_cmds=sorted(set(rejected)), xmlrpc=results)
    json.dump(summary, open(os.path.join(cfg, "summary.json"), "w"), indent=1)
    return summary


if __name__ == "__main__":
    builds, scens = [], []
    args = sys.argv[1:]
    if "-s" in args:
        i = args.index("-s")
        scens = args[i + 1:]
        args = args[:i]
    for a in args:
        name, path = a.split("=", 1)
        builds.append((name, path))
    os.makedirs(RUNS, exist_ok=True)
    for sn in scens or list(SCENARIOS):
        for name, path in builds:
            s = run(name, path, sn)
            print(f"{sn} {name:8s} ok={s['ok']:4d} rejected={s['rejected']:3d} "
                  f"error={s['error']}", flush=True)
            if s["rejected_cmds"]:
                print("    rejected:", " ".join(s["rejected_cmds"]), flush=True)
