"""Strict Yaesu FTX-1 CAT emulator on a pseudo-terminal.

Built from the FTX-1 CAT Operation Reference Manual 2508-C, plus behaviour
the Hamlib FTX-1 backend (KJ5HST) verified on real radios:
  - NB / NR on/off commands do not exist (answer ?;)
  - BS is set only (read answers ?;)
  - field head PC uses tenths below 10 W (PC10.5;)

Every command that does not match the manual's format answers ?; exactly
like the radio.  Every exchange is logged so runs can be compared.
"""
import os, re, select, socket, threading, time
try:
    import pty, tty            # not on Windows: use serve_tcp there
except ImportError:
    pty = tty = None

FIXED_MODES = set("45ABDFHI")          # AM FM DATA-FM FM-N AM-N DATA-FM-N C4FM
SSB_MODES = set("12")                  # LSB USB: SH 00-23
NARROW_TABLE_MODES = set("36789CE")    # CW RTTY DATA PSK: SH 00-21

# EX menu address -> (digits, validator)
EX_MENUS = {
    "020203": (2, lambda v: v.isdigit() and 25 <= int(v) <= 45),   # CW WEIGHT
    "020117": (1, lambda v: v in "0123"),                           # QSK DELAY
    "030113": (3, lambda v: re.fullmatch(r"[+-]\d\d", v) and int(v) in range(-25, 26)),
    "030510": (1, lambda v: v in "012"),                            # VOX SELECT
}


class FTX1:
    def __init__(self, head="spa1", log_path=None):
        self.head = head                       # "spa1" or "field"
        self.log = open(log_path, "w") if log_path else None
        self.lock = threading.Lock()
        side = lambda: dict(freq=14074000, mode="C", width="16", narrow="0",
                            ifshift=0, notch_on=0, notch_f=150, dnf=0, nb=0,
                            nr=0, af=100, rf=255, sql=0, agc=4, smeter=60)
        self.main, self.sub = side(), side()
        self.sub["freq"] = 7074000
        self.sub["mode"] = "8"
        self.vs = "0"; self.ft = "0"; self.st = "0"
        self.pc = "2050" if head == "spa1" else "1005"
        self.pa = {"0": "0", "1": "0", "2": "0"}
        self.ra = "0"; self.tx = "0"; self.ac = "000"
        self.mg = 50; self.vx = "0"; self.vg = 50; self.vd = 8; self.sd = 3
        self.ks = 20; self.kp = 40; self.kr = "1"; self.cs = "0"
        self.ml = {"0": "001", "1": "050"}; self.ps = "1"; self.pl = 20
        self.pr = {"0": "1", "1": "1"}; self.ai = "0"
        self.ex = {"020203": "30", "020117": "0", "030113": "+00", "030510": "0"}
        self.counts = {"ok": 0, "rejected": 0}

    def side(self, p1):
        return self.main if p1 == "0" else self.sub

    # ------------------------------------------------------------------
    def handle(self, cmd):
        """cmd without ';'.  Returns answer string (with ';') or ''."""
        try:
            ans = self._handle(cmd)
        except Exception:                       # malformed numbers etc.
            ans = None
        if ans is None:
            self.counts["rejected"] += 1
            ans = "?;"
            tag = "REJECT"
        else:
            self.counts["ok"] += 1
            tag = "ok"
        if self.log:
            self.log.write(f"{tag}\t{cmd};\t{ans}\n")
            self.log.flush()
        return ans

    def _handle(self, c):
        m = lambda rx: re.fullmatch(rx, c)
        if c == "ID":
            return "ID0840;"
        if g := m(r"F([AB])(\d{9})?"):
            s = self.main if g[1] == "A" else self.sub
            if g[2]:
                f = int(g[2])
                if not 30000 <= f <= 470000000:
                    return None
                s["freq"] = f
                return ""
            return f"F{g[1]}{s['freq']:09d};"
        if g := m(r"VS([01])?"):
            if g[1] is None:
                return f"VS{self.vs};"
            self.vs = g[1]; self.ft = g[1]
            return ""
        if c == "SV":
            self.main, self.sub = self.sub, self.main
            return ""
        if c == "AB":
            self.sub.update({k: self.main[k] for k in ("freq", "mode", "width")})
            return ""
        if c == "BA":
            self.main.update({k: self.sub[k] for k in ("freq", "mode", "width")})
            return ""
        if g := m(r"FT([01])?"):
            if g[1] is None:
                return f"FT{self.ft};"
            self.ft = g[1]
            return ""
        if g := m(r"ST([01])?"):
            if g[1] is None:
                return f"ST{self.st};"
            self.st = g[1]
            return ""
        if g := m(r"SM([01])"):
            return f"SM{g[1]}{self.side(g[1])['smeter']:03d};"
        if g := m(r"RM([1-8])"):
            return f"RM{g[1]}{120:03d}000;"
        if g := m(r"PC(.*)"):
            v = g[1]
            if v == "":
                return f"PC{self.pc};"
            if self.head == "field":
                if g2 := re.fullmatch(r"1(\d{3})", v):
                    w = int(g2[1])
                elif g2 := re.fullmatch(r"1(\d\.\d)", v):
                    w = float(g2[1])
                else:
                    return None
                if not 0.5 <= w <= 10:
                    return None
            else:
                if not (g2 := re.fullmatch(r"2(\d{3})", v)) or not 5 <= int(g2[1]) <= 100:
                    return None
            self.pc = v
            return ""
        if g := m(r"AG([01])(\d{3})?"):
            s = self.side(g[1])
            if g[2] is None:
                return f"AG{g[1]}{s['af']:03d};"
            if int(g[2]) > 255:
                return None
            s["af"] = int(g[2]); return ""
        if g := m(r"TX([01])?"):
            if g[1] is None:
                return f"TX{self.tx};"
            self.tx = g[1]; return ""
        if g := m(r"AC(([01])([02])([0-3]))?"):
            if g[1] is None:
                return f"AC{self.ac};"
            if g[4] == "2" and g[3] != "2":       # P3 2 only for ATAS
                return None
            self.ac = g[1]; return ""
        if g := m(r"RA0([01])?"):
            if g[1] is None:
                return f"RA0{self.ra};"
            self.ra = g[1]; return ""
        if g := m(r"PA([012])(\d)?"):
            if g[2] is None:
                return f"PA{g[1]}{self.pa[g[1]]};"
            if int(g[2]) > (2 if g[1] == "0" else 1):
                return None
            self.pa[g[1]] = g[2]; return ""
        if g := m(r"MD([01])([1-9A-FHI])?"):
            s = self.side(g[1])
            if g[2] is None:
                return f"MD{g[1]}{s['mode']};"
            s["mode"] = g[2]; return ""
        if g := m(r"SH([01])(0(\d\d))?"):
            s = self.side(g[1])
            if g[2] is None:
                return f"SH{g[1]}0{s['width']};"
            code = int(g[3])
            if s["mode"] in FIXED_MODES:
                return None
            top = 23 if s["mode"] in SSB_MODES else 21
            if code > top:
                return None
            s["width"] = g[3]; return ""
        if g := m(r"NA([01])([01])?"):
            s = self.side(g[1])
            if g[2] is None:
                return f"NA{g[1]}{s['narrow']};"
            s["narrow"] = g[2]; return ""
        if g := m(r"IS([01])(0([+-])(\d{4}))?"):
            s = self.side(g[1])
            if g[2] is None:
                v = s["ifshift"]
                return f"IS{g[1]}0{'+' if v >= 0 else '-'}{abs(v):04d};"
            v = int(g[4])
            if v > 1200:
                return None
            s["ifshift"] = v if g[3] == "+" else -v; return ""
        if g := m(r"BP([01])([01])(\d{3})?"):
            s = self.side(g[1])
            if g[3] is None:
                if g[2] == "0":
                    return f"BP{g[1]}0{s['notch_on']:03d};"
                return f"BP{g[1]}1{s['notch_f']:03d};"
            v = int(g[3])
            if g[2] == "0":
                if v > 1:
                    return None
                s["notch_on"] = v
            else:
                if not 1 <= v <= 320:
                    return None
                s["notch_f"] = v
            return ""
        if g := m(r"BC([01])([01])?"):
            s = self.side(g[1])
            if g[2] is None:
                return f"BC{g[1]}{s['dnf']};"
            s["dnf"] = int(g[2]); return ""
        if g := m(r"NL([01])(\d{3})?"):
            s = self.side(g[1])
            if g[2] is None:
                return f"NL{g[1]}{s['nb']:03d};"
            if int(g[2]) > 10:
                return None
            s["nb"] = int(g[2]); return ""
        if g := m(r"RL([01])(\d{2})?"):
            s = self.side(g[1])
            if g[2] is None:
                return f"RL{g[1]}{s['nr']:02d};"
            if int(g[2]) > 10:
                return None
            s["nr"] = int(g[2]); return ""
        if g := m(r"MG(\d{3})?"):
            if g[1] is None:
                return f"MG{self.mg:03d};"
            if int(g[1]) > 100:
                return None
            self.mg = int(g[1]); return ""
        if g := m(r"RG([01])(\d{3})?"):
            s = self.side(g[1])
            if g[2] is None:
                return f"RG{g[1]}{s['rf']:03d};"
            if int(g[2]) > 255:
                return None
            s["rf"] = int(g[2]); return ""
        if g := m(r"SQ([01])(\d{3})?"):
            s = self.side(g[1])
            if g[2] is None:
                return f"SQ{g[1]}{s['sql']:03d};"
            if int(g[2]) > 255:
                return None
            s["sql"] = int(g[2]); return ""
        if g := m(r"VX([01])?"):
            if g[1] is None:
                return f"VX{self.vx};"
            self.vx = g[1]; return ""
        if g := m(r"VG(\d{3})?"):
            if g[1] is None:
                return f"VG{self.vg:03d};"
            if int(g[1]) > 100:
                return None
            self.vg = int(g[1]); return ""
        if g := m(r"([VS]D)(\d{2})?"):
            attr = "vd" if g[1] == "VD" else "sd"
            if g[2] is None:
                return f"{g[1]}{getattr(self, attr):02d};"
            if int(g[2]) > 33:
                return None
            setattr(self, attr, int(g[2])); return ""
        if g := m(r"EX(\d{6})(.*)"):
            addr, val = g[1], g[2]
            if addr not in EX_MENUS:
                return None
            digits, ok = EX_MENUS[addr]
            if val == "":
                return f"EX{addr}{self.ex[addr]};"
            if len(val) != digits or not ok(val):
                return None
            self.ex[addr] = val; return ""
        if g := m(r"KS(\d{3})?"):
            if g[1] is None:
                return f"KS{self.ks:03d};"
            if not 4 <= int(g[1]) <= 60:
                return None
            self.ks = int(g[1]); return ""
        if g := m(r"KP(\d{2})?"):
            if g[1] is None:
                return f"KP{self.kp:02d};"
            if int(g[1]) > 75:
                return None
            self.kp = int(g[1]); return ""
        if g := m(r"KR([01])?"):
            if g[1] is None:
                return f"KR{self.kr};"
            self.kr = g[1]; return ""
        if g := m(r"CS([01])?"):
            if g[1] is None:
                return f"CS{self.cs};"
            self.cs = g[1]; return ""
        if g := m(r"ML([01])(\d{3})?"):
            if g[2] is None:
                return f"ML{g[1]}{self.ml[g[1]]};"
            if (g[1] == "0" and int(g[2]) > 1) or int(g[2]) > 100:
                return None
            self.ml[g[1]] = g[2]; return ""
        if g := m(r"PS([01])?"):
            if g[1] is None:
                return f"PS{self.ps};"
            self.ps = g[1]; return ""
        if g := m(r"PL(\d{3})?"):
            if g[1] is None:
                return f"PL{self.pl:03d};"
            if int(g[1]) > 100:
                return None
            self.pl = int(g[1]); return ""
        if g := m(r"PR([01])([12])?"):
            if g[2] is None:
                return f"PR{g[1]}{self.pr[g[1]]};"
            self.pr[g[1]] = g[2]; return ""
        if g := m(r"BS([01])(\d\d)"):
            if int(g[2]) > 14 or g[2] == "12":
                return None
            return ""
        if g := m(r"GT([01])([0-4])?"):
            s = self.side(g[1])
            if g[2] is None:
                return f"GT{g[1]}{s['agc']};"
            s["agc"] = int(g[2]); return ""
        if m(r"ZI[01]"):
            return ""
        if g := m(r"AI([01])?"):
            if g[1] is None:
                return f"AI{self.ai};"
            self.ai = g[1]; return ""
        return None


def serve(emu, stop_event):
    """Run the emulator on a new pty; returns (slave_path, thread)."""
    master_fd, slave_fd = pty.openpty()
    tty.setraw(master_fd)
    path = os.ttyname(slave_fd)

    def loop():
        buf = b""
        while not stop_event.is_set():
            r, _, _ = select.select([master_fd], [], [], 0.1)
            if not r:
                continue
            try:
                data = os.read(master_fd, 1024)
            except OSError:
                time.sleep(0.05)
                continue
            buf += data
            while b";" in buf:
                raw, buf = buf.split(b";", 1)
                cmd = raw.decode("ascii", "replace").strip()
                if not cmd:
                    continue
                ans = emu.handle(cmd.upper())
                if ans:
                    os.write(master_fd, ans.encode())

    t = threading.Thread(target=loop, daemon=True)
    t.start()
    return path, slave_fd, t


def serve_tcp(emu, stop_event, port=4011):
    """Run the emulator as a TCP server (like ser2net); returns the thread."""
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", port))
    srv.listen(1)
    srv.settimeout(0.2)

    def client(c):
        buf = b""
        while not stop_event.is_set():
            r, _, _ = select.select([c], [], [], 0.1)
            if not r:
                continue
            try:
                data = c.recv(1024)
            except OSError:
                break
            if not data:
                break
            buf += data
            while b";" in buf:
                raw, buf = buf.split(b";", 1)
                cmd = raw.decode("ascii", "replace").strip()
                if not cmd:
                    continue
                ans = emu.handle(cmd.upper())
                if ans:
                    c.sendall(ans.encode())
        c.close()

    def loop():
        while not stop_event.is_set():
            try:
                c, _ = srv.accept()
            except socket.timeout:
                continue
            c.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            threading.Thread(target=client, args=(c,), daemon=True).start()
        srv.close()

    t = threading.Thread(target=loop, daemon=True)
    t.start()
    return t
