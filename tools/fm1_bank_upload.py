#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Sven Trogus
"""Load a DX7 bank (.syx, a 32-voice bulk dump) into sloopDX on an FM-1 over USB-MIDI.

The same as the web editor's library page: BANK_BEGIN, eight BANK_WRITE of 512 bytes, BANK_END with the
dump's checksum byte (web/EDITOR_PROTOCOL.md, v6). The device keeps the bank in flash; U01..U32 then play
the voices and the USER drum kit the first 16.

  fm1_bank_upload.py BANK.syx [--port NAME]      load the bank
  fm1_bank_upload.py --names [--port NAME]       the names of the bank on the device
  fm1_bank_upload.py --erase [--port NAME]       no user bank
  fm1_bank_upload.py --check BANK.syx            only check the file (no device)

Needs mido with python-rtmidi (pip install mido python-rtmidi); the MIDI plumbing is fm1_install.py's.
Exit codes: 0 done, 1 the device refused it or another error, 2 bad arguments or file, 3 FM-1 not found.
"""
import argparse
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fm1_install as I  # noqa: E402

HDR = bytes([0xF0, 0x7D, 0x46, 0x4C])
INFO, DESC, BANK_BEGIN, BANK_WRITE, BANK_END, BANK_INFO, BANK_ERASE = 1, 5, 34, 35, 36, 37, 38
PIECE = 512
NVOICE, VOICE = 32, 128


class BankError(Exception):
    def __init__(self, code, msg):
        super().__init__(msg)
        self.code = code


def checksum(voices):
    return (128 - (sum(voices) & 127)) & 127


def check_bank(data):
    """the voice bytes (4096) of a DX7 32-voice bulk dump, or BankError"""
    if len(data) != 4104:
        raise BankError(2, f"not a DX7 32-voice bulk dump: {len(data)} bytes, want 4104")
    if data[0] != 0xF0 or data[1] != 0x43 or data[2] & 0xF0 or data[3] != 0x09 or data[4] != 0x20 or data[5] or \
            data[4103] != 0xF7:
        raise BankError(2, "not a DX7 32-voice bulk dump (header F0 43 0n 09 20 00 .. F7)")
    voices = data[6:4102]
    if any(b & 0x80 for b in voices):
        raise BankError(2, "not a DX7 dump: a voice byte above 127")
    if checksum(voices) != data[4102]:
        raise BankError(2, "the checksum of the dump is wrong")
    return voices


def names_of(voices):
    out = []
    for k in range(NVOICE):
        raw = voices[k * VOICE + 118:k * VOICE + 128]
        out.append("".join(chr(b) if 32 <= b < 127 else " " for b in raw).rstrip())
    return out


def frame(cmd, args=b""):
    return HDR + bytes([cmd]) + bytes(args) + b"\xF7"


def request(link, cmd, args=b"", timeout=1.0):
    """one editor request; the reply's bytes after the cmd, or BankError"""
    link.drain()
    if not link.send(frame(cmd, args)):
        raise BankError(1, "the MIDI port went away")
    end = time.monotonic() + timeout
    while (left := end - time.monotonic()) > 0:
        p = link.read(left)
        if p is None:
            break
        if p[:4] == HDR and len(p) > 5 and p[4] == cmd:
            return p[5:-1]
    raise BankError(1, f"no reply to editor command {cmd} (sloopDX firmware? the editor open elsewhere?)")


def cstr(b, i):
    j = b.index(0, i)
    return b[i:j].decode("ascii", "replace"), j + 1


def device_info(link):
    r = request(link, INFO)
    version, i = cstr(r, 0)
    neng = r[i]
    i += 5
    for _ in range(neng):
        _, i = cstr(r, i)
    proto = r[i + 1] if i + 1 < len(r) else 0
    return version, proto


def bank_names(link):
    r = request(link, BANK_INFO)
    if not r or not r[0]:
        return None
    out, i = [], 1
    for _ in range(NVOICE):
        s, i = cstr(r, i)
        out.append(s)
    return out


def upload(link, voices, out):
    if request(link, BANK_BEGIN)[:1] != b"\x00":
        raise BankError(1, "BANK_BEGIN refused")
    for off in range(0, len(voices), PIECE):
        r = request(link, BANK_WRITE, bytes([off & 0x7F, (off >> 7) & 0x7F]) + voices[off:off + PIECE])
        if len(r) < 3 or r[2]:
            raise BankError(1, f"BANK_WRITE at {off} refused (rc {r[2] if len(r) > 2 else '?'})")
        print(f"\rsending {100 * (off + PIECE) // len(voices):3d}%\033[K", end="", file=out, flush=True)
    print(file=out, flush=True)
    r = request(link, BANK_END, bytes([checksum(voices)]), timeout=3.0)
    rc = r[0] if r else 9
    if rc == 1:
        raise BankError(1, "the device refused the bank: checksum (a byte was lost on the way; try again)")
    if rc == 2:
        print("the bank plays, but the flash write failed: it is gone at power-off", file=out)
    elif rc:
        raise BankError(1, f"BANK_END rc {rc}")


def find_device(backend, port):
    """the FM-1's editor port (its MIDI port is named Felucca); a MidoLink"""
    ins, outs = backend.input_names(), backend.output_names()
    cands = [n for n in ins if (port and port.lower() in n.lower()) or (not port and I.PORT_RE.search(n))]
    for name in cands:
        o = I.pair_output(name, outs)
        if not o:
            continue
        try:
            link = backend.open(name, o)
        except Exception:
            continue
        try:
            device_info(link)
            return link, name
        except BankError:
            link.close()
    where = f"no MIDI port matching {port!r} answered" if port else "FM-1 not found"
    raise BankError(3, f"{where} (USB data cable? the web editor open?)\n  MIDI inputs: {', '.join(ins) or '(none)'}")


def run(a, backend, out):
    voices = None
    if a.bank:
        voices = check_bank(Path(a.bank).read_bytes())
        if a.check:
            print(f"{a.bank}: a DX7 32-voice bank", file=out)
            for k, n in enumerate(names_of(voices)):
                print(f"  U{k + 1:02d} {n}", file=out)
            return 0
    if backend is None:
        try:
            backend = I.MidoBackend()
        except ImportError:
            raise BankError(2, "needs mido and python-rtmidi (pip install mido python-rtmidi)")
    link, name = find_device(backend, a.port)
    try:
        version, proto = device_info(link)
        if proto < 6:
            raise BankError(1, f"{name} runs {version} (protocol {proto}): no DX7 bank there (sloopDX needed)")
        if a.erase:
            r = request(link, BANK_ERASE, timeout=3.0)
            print("the user bank is empty" + (" (flash write failed)" if r and r[0] else ""), file=out)
            return 1 if r and r[0] else 0
        if voices is not None:
            print(f"loading {a.bank} into {name} ({version})", file=out)
            upload(link, voices, out)
        names = bank_names(link)
        if names is None:
            print("no user bank on the device", file=out)
            return 0
        for k, n in enumerate(names):
            print(f"  U{k + 1:02d} {n}", file=out)
        return 0
    finally:
        link.close()


def main(argv=None, backend=None, out=sys.stdout):
    ap = argparse.ArgumentParser(description="load a DX7 .syx bank into sloopDX on an FM-1")
    ap.add_argument("bank", nargs="?", help="a DX7 32-voice bulk dump (.syx, 4104 bytes)")
    ap.add_argument("--port", help="MIDI port name (substring)")
    ap.add_argument("--names", action="store_true", help="show the bank on the device")
    ap.add_argument("--erase", action="store_true", help="remove the user bank from the device")
    ap.add_argument("--check", action="store_true", help="only check the file")
    a = ap.parse_args(argv)
    if not a.bank and not (a.names or a.erase):
        ap.print_usage(file=out)
        return 2
    try:
        return run(a, backend, out)
    except BankError as e:
        print(f"fm1_bank_upload: {e}", file=out)
        return e.code


if __name__ == "__main__":
    sys.exit(main())
