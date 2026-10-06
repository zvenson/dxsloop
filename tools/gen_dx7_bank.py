#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Sven Trogus
"""sloopDX factory voices: DX7 voices (unpacked, 156 bytes as Dx7Note reads them) for the synth parts
and the FM drum kit, written to firmware/src/dx7_bank.h. Every voice here is made for sloopDX; none is
copied from a Yamaha or M-VAVE bank.

    python3 tools/gen_dx7_bank.py firmware/src/dx7_bank.h
"""
import math
import sys


class Op:
    def __init__(self, r=(99, 99, 99, 99), l=(99, 99, 99, 0), out=0, ratio=1.0, hz=None, det=7, vel=0,
                 rs=0, ams=0, bp=39, ld=0, rd=0, lc=0, rc=0):
        self.r, self.l, self.out, self.det, self.vel, self.rs, self.ams = r, l, out, det, vel, rs, ams
        self.bp, self.ld, self.rd, self.lc, self.rc = bp, ld, rd, lc, rc
        if hz is not None:                       # fixed frequency: 10^(coarse + fine/100)
            lg = math.log10(max(1.0, hz))
            c = min(3, int(math.floor(lg)))
            f = int(round((lg - c) * 100))
            if f > 99:
                c, f = (c + 1, 0) if c < 3 else (3, 99)
            self.fixed, self.coarse, self.fine = 1, c, f
        else:                                    # ratio: coarse (0 = 0.5) * (1 + fine / 100)
            c = 0 if ratio < 1 else int(math.floor(ratio))
            base = 0.5 if c == 0 else c
            self.fixed, self.coarse, self.fine = 0, c, max(0, min(99, int(round((ratio / base - 1) * 100))))

    def bytes(self):
        return [*self.r, *self.l, self.bp, self.ld, self.rd, self.lc, self.rc, self.rs, self.ams, self.vel,
                self.out, self.fixed, self.coarse, self.fine, self.det]


def voice(name, alg, ops, fb=0, pr=(99, 99, 99, 99), pl=(50, 50, 50, 50), lfo=(35, 0, 0, 0, 1, 0, 0), trans=24):
    """ops: dict op number (1..6) -> Op; missing operators are silent"""
    b = []
    for n in range(6, 0, -1):                    # stored OP6 first
        b += (ops.get(n) or Op()).bytes()
    b += [*pr, *pl, alg - 1, fb, 1, *lfo, trans]
    b += list(name.ljust(10)[:10].encode())
    b += [0]
    assert len(b) == 156, (name, len(b))
    return name, b


# ---------------------------------------------------------------- synth voices
D = lambda r1, r2, r3, r4, l1=99, l2=80, l3=0, l4=0: ((r1, r2, r3, r4), (l1, l2, l3, l4))


def env(r, l):
    return dict(r=r, l=l)


SYNTH = [
    voice("EPIANO 1", 5, {
        1: Op(**env(*D(96, 25, 25, 67, 99, 75, 0, 0)), out=99, vel=2, rs=3),
        2: Op(**env(*D(95, 50, 35, 78, 99, 75, 0, 0)), out=58, ratio=14, vel=7, rs=3),
        3: Op(**env(*D(95, 20, 20, 50, 99, 95, 0, 0)), out=95, det=8, vel=2, rs=2),
        4: Op(**env(*D(95, 29, 20, 50, 99, 95, 0, 0)), out=74, vel=6, rs=2),
        5: Op(**env(*D(95, 20, 20, 50, 99, 95, 0, 0)), out=80, det=6, vel=2, rs=2),
        6: Op(**env(*D(95, 29, 20, 50, 99, 95, 0, 0)), out=70, vel=6, rs=2)}, fb=6),
    voice("EPIANO 2", 5, {
        1: Op(**env(*D(96, 30, 22, 70, 99, 80, 0, 0)), out=99, vel=3, rs=3),
        2: Op(**env(*D(97, 60, 40, 80, 99, 60, 0, 0)), out=66, ratio=1, vel=7, rs=3),
        3: Op(**env(*D(96, 30, 22, 70, 99, 80, 0, 0)), out=90, det=9, vel=3, rs=3),
        4: Op(**env(*D(98, 70, 40, 80, 99, 50, 0, 0)), out=50, ratio=17, vel=7, rs=3),
        5: Op(**env(*D(96, 35, 25, 70, 99, 80, 0, 0)), out=80, det=5, vel=3, rs=3),
        6: Op(**env(*D(97, 50, 30, 80, 99, 60, 0, 0)), out=64, ratio=1, vel=7, rs=3)}, fb=0),
    voice("FM BASS", 5, {
        1: Op(**env(*D(99, 40, 30, 70, 99, 90, 85, 0)), out=99, ratio=0.5, vel=2),
        2: Op(**env(*D(99, 55, 40, 70, 99, 70, 55, 0)), out=82, ratio=0.5, vel=6),
        3: Op(**env(*D(99, 60, 40, 70, 99, 75, 0, 0)), out=84, ratio=1, vel=3),
        4: Op(**env(*D(99, 75, 50, 70, 99, 60, 0, 0)), out=68, ratio=3, vel=7)}, fb=0),
    voice("SLAP BASS", 16, {
        1: Op(**env(*D(99, 45, 30, 75, 99, 85, 80, 0)), out=99, ratio=0.5, vel=2),
        2: Op(**env(*D(99, 70, 45, 75, 99, 65, 50, 0)), out=78, ratio=0.5, vel=6),
        3: Op(**env(*D(99, 85, 60, 75, 99, 40, 0, 0)), out=70, ratio=2, vel=7),
        4: Op(**env(*D(99, 90, 60, 75, 99, 30, 0, 0)), out=60, ratio=5, vel=7),
        5: Op(**env(*D(99, 80, 50, 75, 99, 45, 0, 0)), out=72, ratio=1, vel=6),
        6: Op(**env(*D(99, 85, 60, 75, 99, 40, 0, 0)), out=65, ratio=7, vel=7)}, fb=5),
    voice("SUB BASS", 1, {
        1: Op(**env(*D(99, 50, 40, 72, 99, 95, 92, 0)), out=99, ratio=0.5, vel=1),
        2: Op(**env(*D(99, 60, 40, 72, 99, 60, 40, 0)), out=62, ratio=0.5, vel=5)}),
    voice("BRASS", 22, {
        1: Op(**env(*D(72, 40, 30, 60, 99, 92, 88, 0)), out=98, vel=1),
        2: Op(**env(*D(58, 45, 30, 60, 99, 88, 80, 0)), out=78, vel=4, det=6),
        3: Op(**env(*D(72, 40, 30, 60, 99, 92, 88, 0)), out=96, det=9, vel=1),
        4: Op(**env(*D(72, 40, 30, 60, 99, 92, 88, 0)), out=94, det=5, vel=1),
        5: Op(**env(*D(72, 40, 30, 60, 99, 92, 88, 0)), out=90, ratio=2, det=8, vel=1),
        6: Op(**env(*D(60, 45, 30, 60, 99, 88, 80, 0)), out=80, ratio=1, vel=4)}, fb=6,
        lfo=(30, 40, 4, 0, 0, 4, 3)),
    voice("STRINGS", 2, {
        1: Op(**env(*D(45, 30, 30, 56, 99, 95, 92, 0)), out=96, det=4),
        2: Op(**env(*D(45, 30, 30, 56, 99, 90, 88, 0)), out=66, ratio=1, det=10),
        3: Op(**env(*D(48, 30, 30, 56, 99, 95, 92, 0)), out=94, det=10),
        4: Op(**env(*D(48, 30, 30, 56, 99, 90, 88, 0)), out=64, ratio=1, det=4),
        5: Op(**env(*D(48, 30, 30, 56, 99, 90, 88, 0)), out=60, ratio=2, det=7),
        6: Op(**env(*D(48, 30, 30, 56, 99, 90, 88, 0)), out=50, ratio=3, det=7)}, fb=4,
        lfo=(28, 35, 5, 0, 0, 4, 3)),
    voice("GLASS PAD", 5, {
        1: Op(**env(*D(40, 25, 25, 52, 99, 92, 90, 0)), out=94, det=6),
        2: Op(**env(*D(38, 25, 25, 52, 99, 85, 80, 0)), out=58, ratio=3, det=9),
        3: Op(**env(*D(42, 25, 25, 52, 99, 92, 90, 0)), out=90, ratio=2, det=8),
        4: Op(**env(*D(40, 25, 25, 52, 99, 80, 75, 0)), out=55, ratio=5),
        5: Op(**env(*D(40, 25, 25, 52, 99, 92, 90, 0)), out=80, det=5),
        6: Op(**env(*D(40, 25, 25, 52, 99, 85, 80, 0)), out=52, ratio=7)}, lfo=(22, 50, 3, 0, 0, 4, 2)),
    voice("BELLS", 5, {
        1: Op(**env(*D(99, 30, 22, 40, 99, 70, 0, 0)), out=96, vel=3, rs=2),
        2: Op(**env(*D(99, 35, 25, 40, 99, 60, 0, 0)), out=76, ratio=3.5, vel=5, rs=2),
        3: Op(**env(*D(99, 28, 20, 40, 99, 72, 0, 0)), out=84, ratio=2, det=9, vel=3, rs=2),
        4: Op(**env(*D(99, 40, 25, 40, 99, 55, 0, 0)), out=70, ratio=7.07, vel=5, rs=2),
        5: Op(**env(*D(99, 34, 22, 40, 99, 65, 0, 0)), out=72, ratio=4, det=5, vel=3, rs=2),
        6: Op(**env(*D(99, 45, 30, 40, 99, 50, 0, 0)), out=66, ratio=11.3, vel=5, rs=2)}),
    voice("MARIMBA", 5, {
        1: Op(**env(*D(99, 50, 35, 60, 99, 60, 0, 0)), out=99, vel=3, rs=3),
        2: Op(**env(*D(99, 75, 50, 70, 99, 40, 0, 0)), out=70, ratio=4, vel=6, rs=3),
        3: Op(**env(*D(99, 65, 45, 65, 99, 50, 0, 0)), out=72, ratio=4, vel=3, rs=3),
        4: Op(**env(*D(99, 80, 55, 70, 99, 30, 0, 0)), out=55, ratio=10, vel=6, rs=3)}),
    voice("ORGAN", 32, {
        1: Op(**env(*D(99, 99, 99, 75, 99, 99, 99, 0)), out=94, ratio=0.5),
        2: Op(**env(*D(99, 99, 99, 75, 99, 99, 99, 0)), out=96, ratio=1),
        3: Op(**env(*D(99, 99, 99, 75, 99, 99, 99, 0)), out=90, ratio=2),
        4: Op(**env(*D(99, 99, 99, 75, 99, 99, 99, 0)), out=86, ratio=3),
        5: Op(**env(*D(99, 99, 99, 75, 99, 99, 99, 0)), out=80, ratio=4),
        6: Op(**env(*D(99, 80, 60, 75, 99, 60, 0, 0)), out=78, ratio=6)}, fb=2,
        lfo=(40, 0, 3, 0, 0, 4, 2)),
    voice("CLAV", 3, {
        1: Op(**env(*D(99, 60, 40, 80, 99, 70, 0, 0)), out=96, vel=3, rs=3),
        2: Op(**env(*D(99, 70, 50, 80, 99, 60, 0, 0)), out=80, ratio=1, vel=6, rs=3),
        3: Op(**env(*D(99, 80, 60, 80, 99, 50, 0, 0)), out=72, ratio=3, vel=6),
        4: Op(**env(*D(99, 60, 40, 80, 99, 70, 0, 0)), out=90, ratio=2, vel=3, rs=3),
        5: Op(**env(*D(99, 70, 50, 80, 99, 60, 0, 0)), out=76, ratio=1, vel=6),
        6: Op(**env(*D(99, 80, 60, 80, 99, 50, 0, 0)), out=66, ratio=5, vel=6)}, fb=6),
    voice("PLUCK", 5, {
        1: Op(**env(*D(99, 38, 28, 60, 99, 70, 0, 0)), out=99, vel=3, rs=3),
        2: Op(**env(*D(99, 65, 40, 65, 99, 40, 0, 0)), out=76, ratio=1, vel=7, rs=3),
        3: Op(**env(*D(99, 42, 30, 60, 99, 68, 0, 0)), out=86, ratio=2, det=9, vel=3, rs=3),
        4: Op(**env(*D(99, 70, 45, 65, 99, 35, 0, 0)), out=66, ratio=3, vel=7, rs=3)}),
    voice("FLUTE", 1, {
        1: Op(**env(*D(62, 50, 40, 60, 99, 95, 92, 0)), out=99),
        2: Op(**env(*D(60, 50, 40, 60, 99, 70, 60, 0)), out=52, ratio=1, vel=2),
        3: Op(**env(*D(70, 60, 50, 60, 99, 60, 40, 0)), out=40, ratio=2),
        4: Op(**env(*D(80, 70, 60, 60, 99, 50, 30, 0)), out=50, ratio=11, det=14)}, fb=7,
        lfo=(32, 45, 6, 0, 0, 4, 3)),
    voice("SAW LEAD", 32, {
        1: Op(**env(*D(95, 40, 30, 65, 99, 92, 88, 0)), out=0),
        5: Op(**env(*D(95, 40, 30, 65, 99, 92, 88, 0)), out=90, det=4),
        6: Op(**env(*D(95, 40, 30, 65, 99, 92, 88, 0)), out=92, det=10)}, fb=7,
        lfo=(34, 50, 5, 0, 0, 4, 2)),
    voice("KOTO", 5, {
        1: Op(**env(*D(99, 45, 30, 60, 99, 65, 0, 0)), out=98, vel=3, rs=3),
        2: Op(**env(*D(99, 75, 50, 65, 99, 40, 0, 0)), out=78, ratio=3, vel=7, rs=3),
        3: Op(**env(*D(99, 55, 35, 60, 99, 60, 0, 0)), out=80, ratio=1, det=9, vel=3, rs=3),
        4: Op(**env(*D(99, 80, 55, 65, 99, 35, 0, 0)), out=70, ratio=9, vel=7, rs=3)}, fb=3),
    voice("INIT VOICE", 1, {1: Op(out=99)}),
]

# ---------------------------------------------------------------- drum kit
# from the FM-1 Drums prototype (fm1-drums/src/default_kit.cc); lanes in SLOOP's order
DRUM_LANES = ["KICK", "KICK 2", "SNARE", "CLAP", "HAT", "OPEN HAT", "PEDAL", "RIM", "SNARE 2", "LOW TOM", "HI TOM",
              "CRASH", "RIDE", "SHAKER", "CONGA", "COWBELL"]   # SLOOP's lanes (drums.c); + CLAVE: the click


def dec(r, out, **kw):
    return Op(r=(99, r, 99, 99), l=(99, 0, 0, 0), out=out, **kw)


def dec2(r2, l2, r3, out, **kw):
    return Op(r=(99, r2, r3, 99), l=(99, l2, 0, 0), out=out, **kw)


# rates as tuned in the prototype (default_kit.h)
KICK808_R, KICKP_R, SN_BODY_R, SN_NOISE_R, CLAP_R, HATC_R, HATO_R = 48, 61, 67, 63, 66, 78, 51
TOM_R, RIM_R, COWB_R, CLAVE_R, CRASH_R, SHAKER_R = 54, 90, 54, 88, 37, 69

# (name, voice, internal note, level 0..127, sweep semitones, sweep ms, burst, burst ms, choke)
_BYNAME = [
    ("KICK 808", voice("KICK 808", 5, {1: dec(KICK808_R, 99, vel=2), 2: dec(80, 55), 3: dec(96, 55, hz=1600, vel=4),
                                       4: dec(97, 70, hz=3400)}), 31, 100, 20, 28, 1, 0, 0),
    ("SNARE", voice("SNARE", 5, {1: dec(SN_BODY_R, 92, vel=3), 2: dec(85, 50, ratio=1.5), 3: dec(SN_BODY_R, 70, ratio=1.9, vel=3),
                                 5: dec(SN_NOISE_R, 95, hz=5000, vel=3), 6: dec(SN_NOISE_R, 99, hz=8500)}, fb=7),
     54, 100, 5, 12, 1, 0, 0),
    ("CLAP", voice("CLAP", 5, {3: dec(CLAP_R, 88, hz=1100, vel=3), 4: dec(CLAP_R, 99, hz=2200, det=12),
                               5: dec(CLAP_R, 92, hz=1700, vel=3), 6: dec(CLAP_R, 99, hz=3100)}, fb=7),
     60, 100, 0, 0, 4, 11, 0),
    ("HAT CLOSED", voice("HAT CLOSED", 5, {1: dec(HATC_R, 80, hz=7300, vel=3), 2: dec(HATC_R, 92, hz=5200),
                                           3: dec(HATC_R, 70, hz=9100, vel=3), 4: dec(HATC_R, 90, hz=6300),
                                           5: dec(HATC_R, 88, hz=8800, vel=3), 6: dec(HATC_R, 99, hz=9700)}, fb=7),
     60, 100, 0, 0, 1, 0, 1),
    ("HAT OPEN", voice("HAT OPEN", 5, {1: dec(HATO_R, 80, hz=7300, vel=3), 2: dec(HATO_R, 92, hz=5200),
                                       3: dec(HATO_R, 70, hz=9100, vel=3), 4: dec(HATO_R, 90, hz=6300),
                                       5: dec(HATO_R, 88, hz=8800, vel=3), 6: dec(HATO_R, 99, hz=9700)}, fb=7),
     60, 100, 0, 0, 1, 0, 1),
    ("TOM LOW", voice("TOM LOW", 5, {1: dec(TOM_R, 97, vel=2), 2: dec(80, 50), 3: dec(95, 45, hz=1200, vel=4),
                                     4: dec(96, 60, hz=2500)}), 43, 100, 9, 45, 1, 0, 0),
    ("TOM HIGH", voice("TOM HIGH", 5, {1: dec(TOM_R + 4, 97, vel=2), 2: dec(80, 50), 3: dec(95, 45, hz=1800, vel=4),
                                       4: dec(96, 60, hz=2500)}), 50, 100, 9, 45, 1, 0, 0),
    ("CRASH", voice("CRASH", 5, {1: dec2(70, 80, CRASH_R, 82, hz=4700, vel=2), 2: dec2(70, 80, CRASH_R, 95, hz=3300),
                                 3: dec2(70, 80, CRASH_R, 75, hz=6100, vel=2), 4: dec2(70, 80, CRASH_R, 95, hz=8900),
                                 5: dec2(70, 80, CRASH_R, 85, hz=7000, vel=2), 6: dec2(70, 80, CRASH_R, 99, hz=9700)}, fb=7),
     60, 100, 0, 0, 1, 0, 0),
    ("RIDE", voice("RIDE", 5, {1: dec2(60, 85, 45, 84, hz=3900, vel=2), 2: dec2(60, 85, 45, 80, hz=5600),
                               3: dec2(60, 85, 45, 70, hz=7900, vel=2), 4: dec2(60, 85, 45, 72, hz=11000),
                               5: dec2(75, 70, 50, 70, hz=6800, vel=2), 6: dec2(75, 70, 50, 85, hz=9300)}, fb=5),
     60, 127, 0, 0, 1, 0, 0),
    ("SHAKER", voice("SHAKER", 5, {5: Op(r=(80, SHAKER_R, 99, 99), l=(99, 0, 0, 0), out=90, hz=7500, vel=3),
                                   6: Op(r=(80, SHAKER_R, 99, 99), l=(99, 0, 0, 0), out=99, hz=9700)}, fb=7),
     60, 100, 0, 0, 1, 0, 0),
    ("CONGA", voice("CONGA", 5, {1: dec(62, 97, vel=2), 2: dec(85, 55), 3: dec(96, 40, hz=2400, vel=4)}),
     62, 100, 4, 20, 1, 0, 0),
    ("RIMSHOT", voice("RIMSHOT", 5, {1: dec(RIM_R, 90, hz=1700, vel=3), 2: dec(RIM_R, 60, hz=3300),
                                     3: dec(RIM_R - 3, 85, hz=480, vel=3), 5: dec(97, 60, hz=6000, vel=3),
                                     6: dec(97, 99, hz=9000)}, fb=6), 60, 100, 0, 0, 1, 0, 0),
    ("COWBELL", voice("COWBELL", 5, {1: dec2(88, 70, COWB_R, 97, hz=540, vel=3), 2: dec2(88, 70, COWB_R, 70, hz=540),
                                     3: dec2(88, 70, COWB_R, 95, hz=800, vel=3), 4: dec2(88, 70, COWB_R, 68, hz=800)}),
     60, 120, 0, 0, 1, 0, 0),
    ("CLAVE", voice("CLAVE", 5, {1: dec(CLAVE_R, 95, hz=2500, vel=3), 2: dec(95, 40, hz=5000)}), 60, 100, 0, 0, 1, 0, 0),
    ("KICK PUNCH", voice("KICK PUNCH", 5, {1: dec(KICKP_R, 99, vel=2), 2: dec(78, 72, vel=3), 5: dec(98, 70, hz=4000, vel=4),
                                           6: dec(98, 99, hz=7000)}, fb=7), 33, 100, 24, 14, 1, 0, 0),
    ("SNR TIGHT", voice("SNR TIGHT", 5, {1: dec(SN_BODY_R + 6, 85, vel=3), 2: dec(88, 60, ratio=2.3),
                                         5: dec(SN_NOISE_R + 5, 97, hz=6500, vel=3), 6: dec(SN_NOISE_R + 5, 99, hz=9500)}, fb=7),
     60, 100, 7, 8, 1, 0, 0),
]
_BYNAME.append(("PEDAL HAT", voice("PEDAL HAT", 5, {1: dec(HATC_R + 6, 70, hz=7300, vel=3), 2: dec(HATC_R + 6, 92, hz=5200),
                                                 3: dec(HATC_R + 6, 60, hz=9100, vel=3), 4: dec(HATC_R + 6, 90, hz=6300),
                                                 5: dec(HATC_R + 6, 80, hz=8800, vel=3), 6: dec(HATC_R + 6, 99, hz=9700)}, fb=7),
                 60, 127, 0, 0, 1, 0, 1))
_D = {d[0]: d for d in _BYNAME}
DRUMS = [_D[n] for n in ("KICK 808", "KICK PUNCH", "SNARE", "CLAP", "HAT CLOSED", "HAT OPEN", "PEDAL HAT", "RIMSHOT",
                         "SNR TIGHT", "TOM LOW", "TOM HIGH", "CRASH", "RIDE", "SHAKER", "CONGA", "COWBELL", "CLAVE")]
assert len(DRUMS) == 17


def c_bytes(b):
    return "{" + ", ".join(str(x) for x in b) + "}"


def main(path):
    L = ["/* generated by tools/gen_dx7_bank.py: sloopDX factory voices (unpacked DX7, 156 bytes) */", "#pragma once",
         f"#define DX_NSYNTH {len(SYNTH)}", "static const uint8_t DX_SYNTH[DX_NSYNTH][156] = {"]
    for n, b in SYNTH:
        L.append(f"    {c_bytes(b)},   /* {n} */")
    L.append("};")
    L.append("#define DX_SYNTH_NAME_LIST " + ", ".join(f'"{n}"' for n, _ in SYNTH))
    L.append("/* a drum: its voice, the note it plays, level (0..127), the pitch sweep at the hit (semitones and its\n"
             " * decay per CTL block, Q16), burst (hits) and the samples between them, choke group (0 = none) */")
    L.append("typedef struct { const char *name; uint8_t note, level, sweep; uint16_t sweep_k; uint8_t burst; uint16_t burst_n; uint8_t choke; } dx_drum_t;")
    L.append(f"#define DX_NDRUM {len(DRUMS)}   /* the 16 lanes, then the click (clave) */")
    L.append("static const uint8_t DX_DRUM_VOICE[DX_NDRUM][156] = {")
    for d in DRUMS:
        L.append(f"    {c_bytes(d[1][1])},   /* {d[0]} */")
    L.append("};")
    L.append("static const dx_drum_t DX_DRUM[DX_NDRUM] = {")
    for d in DRUMS:
        k = int(round(65536 * math.exp(-32 / (d[5] * 44.1)))) if d[5] else 0
        L.append(f'    {{"{d[0]}", {d[2]}, {d[3]}, {d[4]}, {min(k, 65535)}, {d[6]}, {d[7] * 441 // 10}, {d[8]}}},')
    L.append("};")
    open(path, "w").write("\n".join(L) + "\n")
    print(f"dx7 bank: {len(SYNTH)} synth voices, {len(DRUMS)} drums -> {path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "firmware/src/dx7_bank.h")
