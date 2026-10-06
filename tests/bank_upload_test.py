#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Sven Trogus
"""tools/fm1_bank_upload.py against a simulated sloopDX (no hardware, no mido): the file checks, the upload
in pieces, the device's checksum, names, erase. Run from the repo root: python3 tests/bank_upload_test.py"""
import io
import queue
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_bank_upload as B  # noqa: E402

failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<64} {'ok' if cond else 'FAIL'}")
    failed += not cond


class FakeDevice:                                   # firmware/src/editor.c, the v6 part
    def __init__(self, proto=6):
        self.proto, self.bank, self.busy, self.ok, self.flash = proto, bytearray(4096), False, False, None

    def handle(self, p):
        cmd, a = p[4], p[5:-1]
        r = bytes([cmd])
        if cmd == B.INFO:
            return r + b"FELUCCA sloopDX 1.3\0" + bytes([1, 58, 32, 64, 50]) + b"DX7\0" + bytes([4, self.proto])
        if self.proto < 6 and cmd >= 34:
            return None
        if cmd == B.BANK_BEGIN:
            self.busy, self.ok = True, False
            self.bank[:] = bytes(4096)
            return r + b"\0"
        if cmd == B.BANK_WRITE:
            off, d = a[0] | a[1] << 7, a[2:]
            if not self.busy or not d or off + len(d) > 4096:
                return r + a[:2] + b"\1"
            self.bank[off:off + len(d)] = d
            return r + a[:2] + b"\0"
        if cmd == B.BANK_END:
            good = self.busy and B.checksum(self.bank) == a[0]
            self.busy = False
            self.ok = good
            self.flash = bytes(self.bank) if good else None
            return r + bytes([0 if good else 1])
        if cmd == B.BANK_INFO:
            if not self.ok:
                return r + b"\0" + b"\0" * 32
            return r + b"\1" + b"".join(n.encode() + b"\0" for n in B.names_of(self.bank))
        if cmd == B.BANK_ERASE:
            self.ok, self.flash = False, None
            return r + b"\0"
        return None


class FakeLink:
    def __init__(self, dev):
        self.dev, self.q = dev, queue.Queue()

    def send(self, pkt):
        r = self.dev.handle(pkt)
        if r is not None:
            self.q.put(B.HDR + r + b"\xF7")
        return True

    def read(self, timeout):
        try:
            return self.q.get(timeout=min(timeout, 0.05))
        except queue.Empty:
            return None

    def drain(self):
        while not self.q.empty():
            self.q.get_nowait()

    def close(self):
        pass


class FakeBackend:
    def __init__(self, dev):
        self.dev = dev

    def input_names(self):
        return ["Felucca"]

    def output_names(self):
        return ["Felucca"]

    def open(self, i, o):
        return FakeLink(self.dev)


def dump(names):
    v = bytearray((k * 37 + 11) % 100 for k in range(4096))      # voice bytes (not all equal: a lost byte shows)
    for k, n in enumerate(names):
        v[k * 128 + 118:k * 128 + 128] = n.ljust(10).encode()[:10]
    return bytes([0xF0, 0x43, 0x00, 0x09, 0x20, 0x00]) + v + bytes([B.checksum(v), 0xF7])


names = [f"VOICE {k + 1:02d}" for k in range(32)]
names[3] = "E.PIANO 1"
syx = dump(names)
d = Path(ROOT / "build" / "host")
d.mkdir(parents=True, exist_ok=True)
good, bad, short = d / "bank_ok.syx", d / "bank_bad.syx", d / "bank_short.syx"
good.write_bytes(syx)
bad.write_bytes(syx[:100] + bytes([syx[100] ^ 1]) + syx[101:])
short.write_bytes(syx[:4000])

out = io.StringIO()
ok(B.main([str(good), "--check"], out=out) == 0 and "U04 E.PIANO 1" in out.getvalue(), "--check: a right bank, its names")
ok(B.main([str(bad), "--check"], out=io.StringIO()) == 2, "--check: a wrong checksum is refused")
ok(B.main([str(short), "--check"], out=io.StringIO()) == 2, "--check: a short file is refused")

dev = FakeDevice()
out = io.StringIO()
rc = B.main([str(good)], backend=FakeBackend(dev), out=out)
ok(rc == 0 and dev.ok and dev.flash == syx[6:4102], "upload: the device has the bank")
ok("U04 E.PIANO 1" in out.getvalue() and "U32 VOICE 32" in out.getvalue(), "upload: the names come back")
out = io.StringIO()
ok(B.main(["--names"], backend=FakeBackend(dev), out=out) == 0 and "U01 VOICE 01" in out.getvalue(), "--names")
ok(B.main(["--erase"], backend=FakeBackend(dev), out=io.StringIO()) == 0 and not dev.ok, "--erase")
out = io.StringIO()
ok(B.main(["--names"], backend=FakeBackend(dev), out=out) == 0 and "no user bank" in out.getvalue(), "--names without a bank")

old = FakeDevice(proto=5)
ok(B.main([str(good)], backend=FakeBackend(old), out=io.StringIO()) == 1 and not old.ok, "SLOOP 2.x firmware: refused, nothing sent")


class Lossy(FakeDevice):                            # a byte lost on the way: the device's checksum catches it
    def handle(self, p):
        if p[4] == B.BANK_WRITE and (p[5] | p[6] << 7) == 1024:
            p = p[:20] + p[21:]
        return super().handle(p)


lossy = Lossy()
ok(B.main([str(good)], backend=FakeBackend(lossy), out=io.StringIO()) == 1 and not lossy.ok, "a damaged piece: the device refuses the bank")

print("BANK UPLOAD TEST FAILED" if failed else "bank upload ok")
sys.exit(1 if failed else 0)
