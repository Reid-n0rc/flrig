"""TCP front end for ft710_emu (models a ser2net / remote-serial link):
same command handler and timing model, bytes go to a TCP client instead of
a pty.  serve_tcp(emu, stop, port) -> port"""
import heapq, select, socket, threading, time
from ft710_emu import DeliveryQueue, now_ms


def serve_tcp(emu, stop_event, port=4011):
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", port))
    srv.listen(1)
    q = DeliveryQueue()
    emu.q = q
    state = {"conn": None}

    def acceptor():
        srv.settimeout(0.2)
        while not stop_event.is_set():
            try:
                c, _ = srv.accept()
            except socket.timeout:
                continue
            c.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            state["conn"] = c
            threading.Thread(target=reader, args=(c,), daemon=True).start()
        srv.close()

    def reader(c):
        buf = b""
        while not stop_event.is_set():
            r, _, _ = select.select([c], [], [], 0.05)
            if not r:
                continue
            data = c.recv(1024)
            if not data:
                break
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
                    emu.w("REJ" if rej else "TXQ", f"{t:.1f}", cmd + ";", ans, f"{td:.1f}")

    def writer():
        while not stop_event.is_set():
            with q.cv:
                if not q.h:
                    q.cv.wait(0.05); continue
                t, _, data = q.h[0]
                dt = (t - now_ms()) / 1000.0
                if dt > 0:
                    q.cv.wait(min(dt, 0.05)); continue
                heapq.heappop(q.h)
            c = state["conn"]
            if c:
                try:
                    c.sendall(data)
                    emu.w("OUT", f"{now_ms():.1f}", data.decode())
                except OSError:
                    pass

    for f in (acceptor, writer):
        threading.Thread(target=f, daemon=True).start()
    return port
