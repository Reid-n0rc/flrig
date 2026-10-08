"""Strict Yaesu FT-710 CAT emulator on a pseudo-terminal, with a timing model.

Formats from FT-710 CAT Operation Reference Manual 2306-C; timing calibrated
from a real FT-710 capture (38400 8N1, RTS/CTS, radio710/rx_rtscts.txt):
  * reply first byte 4.6 - 10 ms after the command; each reply in one chunk
  * two commands written back to back -> both replies, the second one
    following the first by its wire time (10 chars @ 38400 = 2.6 ms)
  * RM2; RM9; VE; -> ?;    RMp answers RMpnnn000; (10 chars)

Timing model (per command, in arrival order, single CAT "line"):
    start   = max(arrival + proc_delay, line_free)
    done    = start + len(reply) * 10 / baud
    line_free = done
  chunk == 0  -> whole reply written to the pty at `done` (USB-CDC style)
  chunk  > 0  -> reply dribbled in `chunk`-byte pieces, each piece written
                 when its last byte would have left the wire, plus
                 `chunk_gap_ms` extra between pieces.
proc_delay is drawn from `proc_ms` (list -> random.choice, number -> fixed)
plus `extra_latency_ms` (the knob used to reproduce slow radios / USB hubs /
remote serial servers).

Auto Information: `ai_period_ms` > 0 makes the radio send unsolicited
meter / state messages while AI is on (`ai_force` keeps AI on even after
flrig sends AI0;, i.e. a radio where AI was not turned off).

One-time reply delay (patch 8 late-reply test): `delay_once_after=N` adds
`delay_ms` (default 300) to the processing time of the reply after the
first N replies, once; later replies queue behind it (one CAT line), as a
radio that stalls once.  A script may also arm it at run time by setting
`emu.delay_once_after = emu.n_replies` (= the next reply).  Logged as
  DELAY  t_ms  reply  delay_ms  wall_time_s
Default None = no delay (behaviour unchanged).

Every exchange is logged (tab separated):
  RX   t_ms  cmd;                 (command complete at emulator)
  TXQ  t_ms  cmd;  reply  t_deliver_ms
  OUT  t_ms  bytes                (bytes actually written to the pty)
"""
import os, random, re, select, threading, time, heapq
try:
    import pty, tty            # not on Windows: use the TCP front end there
except ImportError:
    pty = tty = None

T0 = time.monotonic()


def now_ms():
    return (time.monotonic() - T0) * 1000.0


class FT710:
    # TX meter raw values (distinct so each one is recognisable downstream)
    TX_RM = {"1": "000", "3": "010", "4": "040", "5": "150", "6": "030",
             "7": "120", "8": "214", "0": "000"}
    RX_RM = {"1": "128", "3": "000", "4": "000", "5": "000", "6": "000",
             "7": "000", "8": "214", "0": "130"}

    def __init__(self, log_path=None, baud=38400, proc_ms=(2.0, 2.4, 4.9),
                 extra_latency_ms=0.0, chunk=0, chunk_gap_ms=0.0,
                 ai_period_ms=0, ai_force=False, seed=1, tx_rm="",
                 delay_once_after=None, delay_ms=300.0):
        self.log = open(log_path, "w") if log_path else None
        self.loglock = threading.Lock()
        self.baud = baud
        self.proc_ms = proc_ms
        self.extra_latency_ms = extra_latency_ms
        self.chunk = chunk
        self.chunk_gap_ms = chunk_gap_ms
        self.ai_period_ms = ai_period_ms
        self.ai_force = ai_force
        self.rng = random.Random(seed)
        self.TX_RM = dict(FT710.TX_RM)
        for kv in filter(None, str(tx_rm).split("/")):   # e.g. "5:020/6:100"
            k, v = kv.split(":"); self.TX_RM[k] = f"{int(v):03d}"
        self.line_free = 0.0
        self.delay_once_after = delay_once_after
        self.delay_ms = delay_ms
        self.n_replies = 0          # replies scheduled so far
        self.delay_wall = None      # time.time() when the delay was applied
        # radio state
        self.fa = 14074000; self.fb = 7074000
        self.md = {"0": "2", "1": "2"}
        self.vs = "0"; self.ft = "0"; self.st = "0"
        self.pc = 100; self.ag = 100; self.rg = 255; self.sq = 0; self.mg = 50
        self.ra = "0"; self.pa = "0"; self.sh = {"0": "13", "1": "13"}
        self.is_ = 0; self.bp_on = 0; self.bp_f = 150; self.bc = "0"
        self.nb = "0"; self.nr = "0"; self.rl = 1; self.bi = "0"; self.cs = "0"
        self.ac = "000"; self.tx = "0"; self.ai = "0"; self.ps = "1"
        self.vx = "0"; self.vg = 50; self.vd = 15; self.ks = 20; self.kr = "1"
        self.counts = {"ok": 0, "rejected": 0}

    # ------------------------------------------------------------ logging
    def w(self, *f):
        if not self.log:
            return
        with self.loglock:
            self.log.write("\t".join(str(x) for x in f) + "\n")
            self.log.flush()

    # ------------------------------------------------------------ commands
    def handle(self, c):
        try:
            a = self._handle(c)
        except Exception:
            a = None
        if a is None:
            self.counts["rejected"] += 1
            return "?;", True
        self.counts["ok"] += 1
        return a, False

    def _handle(self, c):
        m = lambda rx: re.fullmatch(rx, c)
        if c == "ID": return "ID0800;"
        if c == "VE": return None
        if g := m(r"AI([01])?"):
            if g[1] is None: return f"AI{self.ai};"
            self.ai = "1" if self.ai_force else g[1]; return ""
        if g := m(r"PS([01])?"):
            if g[1] is None: return f"PS{self.ps};"
            self.ps = g[1]; return ""
        if g := m(r"F([AB])(\d{9})?"):
            if g[2] is None:
                return f"F{g[1]}{(self.fa if g[1]=='A' else self.fb):09d};"
            if g[1] == "A": self.fa = int(g[2])
            else: self.fb = int(g[2])
            return ""
        if c == "IF":
            return f"IF000{self.fa:09d}+000000{self.md['0']}00000;"
        if g := m(r"VS([01])?"):
            if g[1] is None: return f"VS{self.vs};"
            self.vs = g[1]; return ""
        if g := m(r"FT([0-3])?"):
            if g[1] is None: return f"FT{self.ft};"
            self.ft = g[1]; return ""
        if g := m(r"ST([0-2])?"):
            if g[1] is None: return f"ST{self.st};"
            self.st = g[1]; return ""
        if c in ("SV", "AB", "BA", "VM"): return ""
        if m(r"BS\d\d") or m(r"MC\d{3}"): return ""
        if c == "SM0":
            return "SM0000;" if self.tx != "0" else "SM0128;"
        if g := m(r"RM(\d)"):
            p = g[1]
            if p in ("2", "9"): return None
            v = (self.TX_RM if self.tx != "0" else self.RX_RM)[p]
            return f"RM{p}{v}000;"
        if g := m(r"PC(\d{3})?"):
            if g[1] is None: return f"PC{self.pc:03d};"
            v = int(g[1])
            if not 5 <= v <= 100: return None
            self.pc = v; return ""
        if g := m(r"AG0(\d{3})?"):
            if g[1] is None: return f"AG0{self.ag:03d};"
            self.ag = int(g[1]); return ""
        if g := m(r"RG0(\d{3})?"):
            if g[1] is None: return f"RG0{self.rg:03d};"
            self.rg = int(g[1]); return ""
        if g := m(r"SQ0(\d{3})?"):
            if g[1] is None: return f"SQ0{self.sq:03d};"
            self.sq = int(g[1]); return ""
        if g := m(r"MG(\d{3})?"):
            if g[1] is None: return f"MG{self.mg:03d};"
            self.mg = int(g[1]); return ""
        if g := m(r"TX([0-2])?"):
            if g[1] is None: return f"TX{self.tx};"
            self.tx = g[1]
            if self.ai == "1": self.ai_send(f"TX{self.tx};")
            return ""
        if g := m(r"AC(\d{3})?"):
            if g[1] is None: return f"AC{self.ac};"
            self.ac = g[1]; return ""
        if g := m(r"RA0([0-3])?"):
            if g[1] is None: return f"RA0{self.ra};"
            self.ra = g[1]; return ""
        if g := m(r"PA0([0-2])?"):
            if g[1] is None: return f"PA0{self.pa};"
            self.pa = g[1]; return ""
        if g := m(r"MD([01])([1-9A-F])?"):
            if g[2] is None: return f"MD{g[1]}{self.md[g[1]]};"
            self.md[g[1]] = g[2]; return ""
        if c == "SH0": return f"SH00{self.sh['0']};"
        if g := m(r"SH00(\d\d)"):
            if int(g[1]) > 23: return None
            self.sh["0"] = g[1]; return ""
        if c == "IS0":
            return f"IS00{'+' if self.is_ >= 0 else '-'}{abs(self.is_):04d};"
        if g := m(r"IS00([+-]\d{4})"):
            self.is_ = int(g[1]); return ""
        if g := m(r"BP0([01])(\d{3})?"):
            if g[2] is None:
                return f"BP0{g[1]}{(self.bp_on if g[1]=='0' else self.bp_f):03d};"
            if g[1] == "0": self.bp_on = int(g[2])
            else: self.bp_f = int(g[2])
            return ""
        if g := m(r"AV(\d{3})?"):
            if g[1] is None: return f"AV{getattr(self, 'av', 4):03d};"
            v = int(g[1])
            if not 1 <= v <= 100: return None
            self.av = v; return ""
        if g := m(r"BC0([01])?"):
            if g[1] is None: return f"BC0{self.bc};"
            self.bc = g[1]; return ""
        if g := m(r"NB0([01])?"):
            if g[1] is None: return f"NB0{self.nb};"
            self.nb = g[1]; return ""
        if g := m(r"NR0([01])?"):
            if g[1] is None: return f"NR0{self.nr};"
            self.nr = g[1]; return ""
        if g := m(r"RL0(\d\d)?"):
            if g[1] is None: return f"RL0{self.rl:02d};"
            self.rl = int(g[1]); return ""
        if g := m(r"BI([01])?"):
            if g[1] is None: return f"BI{self.bi};"
            self.bi = g[1]; return ""
        if g := m(r"CS([01])?"):
            if g[1] is None: return f"CS{self.cs};"
            self.cs = g[1]; return ""
        if g := m(r"VX([01])?"):
            if g[1] is None: return f"VX{self.vx};"
            self.vx = g[1]; return ""
        if g := m(r"VG(\d{3})?"):
            if g[1] is None: return f"VG{self.vg:03d};"
            self.vg = int(g[1]); return ""
        if g := m(r"VD(\d{4})?"):
            if g[1] is None: return f"VD{self.vd:04d};"
            self.vd = int(g[1]); return ""
        if g := m(r"KS(\d{3})?"):
            if g[1] is None: return f"KS{self.ks:03d};"
            self.ks = int(g[1]); return ""
        if g := m(r"KR([01])?"):
            if g[1] is None: return f"KR{self.kr};"
            self.kr = g[1]; return ""
        if m(r"DT0\d{8}") or m(r"DT1\d{6}"): return ""
        if m(r"EX\d{6}.+"): return ""
        return None

    # ------------------------------------------------------------ timing
    def proc_delay(self):
        p = self.proc_ms
        d = self.rng.choice(p) if isinstance(p, (list, tuple)) else float(p)
        return d + self.extra_latency_ms

    def schedule(self, arrival, reply, queue):
        """Push the reply's byte pieces onto the delivery heap."""
        bt = 10000.0 / self.baud               # ms per char
        proc = self.proc_delay()
        if (self.delay_once_after is not None and self.delay_wall is None
                and self.n_replies >= self.delay_once_after):
            proc += self.delay_ms
            self.delay_wall = time.time()
            self.w("DELAY", f"{now_ms():.1f}", reply, f"{self.delay_ms:.0f}",
                   f"{self.delay_wall:.3f}")
        self.n_replies += 1
        start = max(arrival + proc, self.line_free)
        done = start + len(reply) * bt
        self.line_free = done
        b = reply.encode()
        if self.chunk <= 0:
            pieces = [(done, b)]
        else:
            pieces, t = [], start
            for i in range(0, len(b), self.chunk):
                piece = b[i:i + self.chunk]
                t += len(piece) * bt + (self.chunk_gap_ms if i else 0.0)
                pieces.append((t, piece))
            self.line_free = t
        for t, piece in pieces:
            queue.push(t, piece)
        return pieces[-1][0]

    def ai_send(self, msg):
        if hasattr(self, "q"):
            self.schedule(now_ms(), msg, self.q)
            self.w("AI", f"{now_ms():.1f}", msg)


class DeliveryQueue:
    def __init__(self):
        self.h, self.cv, self.n = [], threading.Condition(), 0

    def push(self, t, data):
        with self.cv:
            self.n += 1
            heapq.heappush(self.h, (t, self.n, data))
            self.cv.notify()


def serve(emu, stop_event):
    """Run the emulator on a new pty; returns (slave_path, slave_fd)."""
    master_fd, slave_fd = pty.openpty()
    tty.setraw(master_fd)
    tty.setraw(slave_fd)
    path = os.ttyname(slave_fd)
    q = DeliveryQueue()
    emu.q = q

    def reader():
        buf = b""
        while not stop_event.is_set():
            r, _, _ = select.select([master_fd], [], [], 0.05)
            if not r:
                continue
            try:
                data = os.read(master_fd, 1024)
            except OSError:
                time.sleep(0.02)
                continue
            t = now_ms()
            buf += data
            while b";" in buf:
                raw, buf = buf.split(b";", 1)
                cmd = raw.decode("ascii", "replace").strip().upper()
                if not cmd:
                    continue
                emu.w("RX", f"{t:.1f}", cmd + ";")
                ans, rej = emu.handle(cmd)
                if ans:
                    td = emu.schedule(t, ans, q)
                    emu.w("REJ" if rej else "TXQ", f"{t:.1f}", cmd + ";", ans,
                          f"{td:.1f}")

    def writer():
        while not stop_event.is_set():
            with q.cv:
                if not q.h:
                    q.cv.wait(0.05)
                    continue
                t, _, data = q.h[0]
                dt = (t - now_ms()) / 1000.0
                if dt > 0:
                    q.cv.wait(min(dt, 0.05))
                    continue
                heapq.heappop(q.h)
            try:
                os.write(master_fd, data)
                emu.w("OUT", f"{now_ms():.1f}", data.decode())
            except OSError:
                pass

    def ai_loop():
        while not stop_event.is_set():
            if emu.ai_period_ms <= 0:
                time.sleep(0.2); continue
            time.sleep(emu.ai_period_ms / 1000.0)
            if emu.ai == "1" and emu.tx != "0":
                for p in ("5", "6"):
                    emu.ai_send(f"RM{p}{emu.TX_RM[p]}000;")

    for f in (reader, writer, ai_loop):
        threading.Thread(target=f, daemon=True).start()
    return path, slave_fd
