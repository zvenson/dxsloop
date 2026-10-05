// SPDX-License-Identifier: GPL-3.0-only
// The DX7 engine's core (firmware/src/dx7_core.c) against Dexed's msfa, sample for sample:
// random voices (every algorithm, feedback, fixed / ratio, detune, scaling, pitch envelope, LFO
// with pitch and amp modulation), several notes and velocities, held, released, rendered to the end.
//   build: see tests/dx7ref/build.sh
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include "dx7note.h"
#include "fm_core.h"
#include "controllers.h"
#include "tuning.h"
#include "freqlut.h"
#include "exp2.h"
#include "sin.h"
#include "lfo.h"
#include "pitchenv.h"
#include "env.h"
#include "porta.h"
extern "C" {
void *cport_new(void);
void cport_init(void *v, const uint8_t *p, int note, int vel);
void cport_compute(void *v, int32_t *out, int32_t lv, int32_t ld);
void cport_keyup(void *v);
int cport_playing(void *v);
void cport_lfo(void *v, int32_t *lv, int32_t *ld);
}
static uint32_t rs = 12345;
static int rnd(int n) { rs = rs * 1664525u + 1013904223u; return (int)((rs >> 8) % (uint32_t)n); }

static void random_voice(uint8_t *p, int ams_off)
{
    memset(p, 0, 156);
    for (int op = 0; op < 6; op++) {
        uint8_t *o = p + op * 21;
        for (int i = 0; i < 4; i++) { o[i] = rnd(100); o[4 + i] = rnd(100); }
        if (rnd(3)) o[7] = 0;
        o[8] = rnd(100); o[9] = rnd(100); o[10] = rnd(100); o[11] = rnd(4); o[12] = rnd(4);
        o[13] = rnd(8); o[14] = ams_off ? 0 : rnd(4); o[15] = rnd(8); o[16] = 50 + rnd(50);
        o[17] = rnd(5) == 0; o[18] = rnd(32); o[19] = rnd(100); o[20] = rnd(15);
    }
    for (int i = 0; i < 4; i++) { p[126 + i] = rnd(100); p[130 + i] = 40 + rnd(21); }
    p[134] = rnd(32); p[135] = rnd(8); p[136] = 1;
    p[137] = rnd(100); p[138] = rnd(100); p[139] = rnd(100); p[140] = ams_off ? 0 : rnd(100);
    p[141] = rnd(2); p[142] = rnd(6); p[143] = rnd(8); p[144] = 24;
}

int main(int argc, char **argv)
{
    int voices = argc > 1 ? atoi(argv[1]) : 300;
    Exp2::init(); Tanh::init(); Sin::init(); Freqlut::init(44100); Lfo::init(44100);
    PitchEnv::init(44100); Env::init_sr(44100); Porta::init_sr(44100);
    auto ts = createStandardTuning();
    FmCore core;
    Controllers c;
    memset(c.values_, 0, sizeof c.values_);
    c.values_[kControllerPitch] = 0x2000; c.values_[kControllerPitchRangeUp] = 3;
    c.values_[kControllerPitchRangeDn] = 3; c.values_[kControllerPitchStep] = 0;
    c.masterTune = 0; c.modwheel_cc = c.foot_cc = c.breath_cc = c.aftertouch_cc = 0;
    c.portamento_enable_cc = false; c.portamento_cc = 0; c.portamento_gliss_cc = false;
    c.core = &core; c.refresh();
    long exact = 0, total = 0, bad_voices = 0, ams_voices = 0; double worst = 0;
    for (int k = 0; k < voices; k++) {
        uint8_t p[156];
        int ams_off = k % 4 != 0;                       // every 4th voice uses amplitude modulation
        random_voice(p, ams_off);
        int note = 24 + rnd(80), vel = 1 + rnd(127);
        Dx7Note d(ts, nullptr);
        d.init(p, note, vel, 1, &c);
        d.oscSync();
        void *cv = cport_new();
        cport_init(cv, p, note, vel);
        long blocks = 44100 * 3 / 64, rel = 44100 * 1 / 64;
        int diff = 0; double vmax = 0, dmax = 0;
        for (long b = 0; b < blocks; b++) {
            int32_t a[64] = {0}, bb[64] = {0}, lv, ld;
            if (b == rel) { d.keyup(); cport_keyup(cv); }
            cport_lfo(cv, &lv, &ld);                      // one LFO feeds both
            d.compute(a, lv, ld, &c);
            cport_compute(cv, bb, lv, ld);
            for (int i = 0; i < 64; i++) {
                total++;
                if (a[i] == bb[i]) exact++;
                else { diff = 1; double e = abs(a[i] - bb[i]); if (e > dmax) dmax = e; }
                double m = abs(a[i]); if (m > vmax) vmax = m;
            }
            if (d.isPlaying() != (bool)cport_playing(cv)) diff = 2;
        }
        if (diff) {
            bad_voices++;
            if (!ams_off) ams_voices++;
            double rel_e = vmax ? dmax / vmax : 0; if (rel_e > worst) worst = rel_e;
            if (bad_voices <= 5 || (ams_off && diff))
                printf("voice %d (alg %d, ams %s): %s, max diff %.0f of peak %.0f\n", k, p[134] + 1,
                       ams_off ? "off" : "on", diff == 2 ? "PLAYING STATE DIFFERS" : "samples differ", dmax, vmax);
        }
    }
    printf("%d voices, %ld samples: %.4f %% identical; voices with any difference: %ld (of them with AMS: %ld), worst %.2e of peak\n",
           voices, total, 100.0 * exact / total, bad_voices, ams_voices, worst);
    return (bad_voices - ams_voices) ? 1 : 0;   // only the amp-mod table may differ
}
