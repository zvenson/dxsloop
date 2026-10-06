/* SPDX-License-Identifier: Apache-2.0 AND GPL-3.0-only
 * DX7 voice core: a C port of Dexed's msfa (Copyright 2012 Google Inc., 2016-2025 Pascal Gauthier,
 * Apache-2.0), integer only (the floating point of the original is in dx7_tables.h, computed on the
 * build machine). Port and SLOOP integration: Copyright (C) 2026 Sven Trogus, GPL-3.0-only.
 *
 * One dxv_t is one sounding DX7 note: six operators with their envelopes, the pitch envelope, the LFO
 * and the feedback state. dx_init() starts it from an unpacked 156-byte voice, dx_compute() renders one
 * block of DX_N samples into buf (added), as Dx7Note::compute with Dexed's FmCore. Differences from
 * Dexed, all off the sound path of a plain note: no portamento, no MTS / scale tuning, no pitch bend or
 * controllers (the caller passes its pitch offset), the LFO per note (Dexed: one per synth), the
 * feedback history cleared at note-on (Dexed leaves it from the last note: hits sound alike here).
 * With DX_LG_N 6 the render matches Dexed: 99 % of samples identical, the rest within 1-2 LSB, mostly where
 * AMS uses a table for Dexed's float exp (tests/dx7_exact_test.cc). */
#ifndef DX_LG_N
#define DX_LG_N 5                /* SLOOP renders blocks of CTL = 32 samples */
#endif
#define DX_N (1 << DX_LG_N)
#include "dx7_tables.h"
#if DX_LG_N == 5
#define DX_LFO_UNIT DX_LFO_UNIT32
#define DX_LFO_DELTA DX_LFO_DELTA32
#define DX_PENV_UNIT DX_PENV_UNIT32
#else
#define DX_LFO_UNIT DX_LFO_UNIT64
#define DX_LFO_DELTA DX_LFO_DELTA64
#define DX_PENV_UNIT DX_PENV_UNIT64
#endif

static inline int32_t dx_sin(int32_t phase)    /* Sin::lookup, Q24 phase -> Q24 */
{
    int32_t lowbits = phase & ((1 << 14) - 1);
    int32_t pi = (phase >> 13) & (1023 << 1);
    return DX_SINTAB[pi + 1] + (int32_t)(((int64_t)DX_SINTAB[pi] * lowbits) >> 14);
}

static inline int32_t dx_exp2(int32_t x)       /* Exp2::lookup, Q24 -> Q24 */
{
    int32_t lowbits = x & ((1 << 14) - 1);
    int32_t xi = (x >> 13) & (1023 << 1);
    int32_t y = DX_EXP2TAB[xi + 1] + (int32_t)(((int64_t)DX_EXP2TAB[xi] * lowbits) >> 14);
    return y >> (6 - (x >> 24));
}

static inline int32_t dx_freq(int32_t logfreq) /* Freqlut::lookup: Q24 log2 -> phase increment */
{
    int32_t ix = (logfreq & 0xffffff) >> 14;
    int32_t y0 = DX_FREQLUT[ix], y1 = DX_FREQLUT[ix + 1];
    int32_t y = y0 + (int32_t)(((int64_t)(y1 - y0) * (logfreq & ((1 << 14) - 1))) >> 14);
    return y >> (20 - (logfreq >> 24));
}

/* ------------------------------------------------------------------ envelope (Env) --- */
typedef struct {
    uint8_t rates[4], levels[4];
    uint8_t ix, rising, down, active;
    int16_t outlevel, rate_scaling;
    int32_t level, target, inc, statics;
} dx_env_t;

static const uint8_t DX_LEVELLUT[20] = {0, 5, 9, 13, 17, 20, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 42, 43, 45, 46};
static const int32_t DX_STATICS[77] = {
    1764000, 1764000, 1411200, 1411200, 1190700, 1014300, 992250, 882000, 705600, 705600, 584325, 507150,
    502740, 441000, 418950, 352800, 308700, 286650, 253575, 220500, 220500, 176400, 145530, 145530, 125685,
    110250, 110250, 88200, 88200, 74970, 61740, 61740, 55125, 48510, 44100, 37485, 31311, 30870, 27562,
    27562, 22050, 18522, 17640, 15435, 14112, 13230, 11025, 9261, 9261, 7717, 6615, 6615, 5512, 5512, 4410,
    3969, 3969, 3439, 2866, 2690, 2249, 1984, 1896, 1808, 1411, 1367, 1234, 1146, 926, 837, 837, 705, 573,
    573, 529, 441, 441};

static inline int dx_scaleoutlevel(int ol) { return ol >= 20 ? 28 + ol : DX_LEVELLUT[ol]; }

static void dx_env_advance(dx_env_t *e, int newix)
{
    e->ix = (uint8_t)newix;
    if (newix < 4) {
        int newlevel = e->levels[newix];
        int actual = dx_scaleoutlevel(newlevel) >> 1;
        int qrate;
        actual = (actual << 6) + e->outlevel - 4256;
        actual = actual < 16 ? 16 : actual;
        e->target = actual << 16;
        e->rising = e->target > e->level;
        qrate = (e->rates[newix] * 41) >> 6;
        qrate += e->rate_scaling;
        qrate = qrate > 63 ? 63 : qrate;
        if (e->target == e->level || (newix == 0 && newlevel == 0)) {
            int sr = e->rates[newix] + e->rate_scaling;
            sr = sr > 99 ? 99 : sr;
            e->statics = sr < 77 ? DX_STATICS[sr] : 20 * (99 - sr);
            if (sr < 77 && newix == 0 && newlevel == 0)
                e->statics /= 20;
        } else {
            e->statics = 0;
        }
        e->inc = (4 + (qrate & 3)) << (2 + DX_LG_N + (qrate >> 2));   /* (44.1 kHz: no rate factor) */
    }
}

/* new rates / levels for an envelope that is running (Dexed Env::update): a held note goes on from its
 * sustain stage, a released one keeps fading */
static void dx_env_update(dx_env_t *e, const uint8_t *r, const uint8_t *l, int ol, int rate_scaling)
{
    int i;
    for (i = 0; i < 4; i++) {
        e->rates[i] = r[i];
        e->levels[i] = l[i];
    }
    e->outlevel = (int16_t)ol;
    e->rate_scaling = (int16_t)rate_scaling;
    if (e->down)
        dx_env_advance(e, 2);
}

static void dx_env_init(dx_env_t *e, const uint8_t *r, const uint8_t *l, int ol, int rate_scaling)
{
    int i;
    for (i = 0; i < 4; i++) {
        e->rates[i] = r[i];
        e->levels[i] = l[i];
    }
    e->outlevel = (int16_t)ol;
    e->rate_scaling = (int16_t)rate_scaling;
    e->level = 0;
    e->down = 1;
    e->active = 1;
    dx_env_advance(e, 0);
}

static int32_t dx_env_sample(dx_env_t *e)
{
    if (e->statics) {
        e->statics -= DX_N;
        if (e->statics <= 0) {
            e->statics = 0;
            dx_env_advance(e, e->ix + 1);
        }
    }
    if (e->ix < 3 || (e->ix < 4 && !e->down)) {
        if (e->statics) {
            ;
        } else if (e->rising) {
            const int32_t jump = 1716 << 16;
            if (e->level < jump)
                e->level = jump;
            e->level += (((17 << 24) - e->level) >> 24) * e->inc;
            if (e->level >= e->target) {
                e->level = e->target;
                dx_env_advance(e, e->ix + 1);
            }
        } else {
            e->level -= e->inc;
            if (e->level <= e->target) {
                e->level = e->target;
                dx_env_advance(e, e->ix + 1);
            }
        }
    }
    return e->level;
}

static void dx_env_keydown(dx_env_t *e, int d)
{
    if (e->down != d) {
        e->down = (uint8_t)d;
        dx_env_advance(e, d ? 0 : 3);
    }
}

static inline int dx_env_active(const dx_env_t *e) { return e->active && (e->ix < 4 || e->levels[3] > 0); }

/* ------------------------------------------------------------ pitch envelope (PitchEnv) --- */
static const uint8_t DX_PRATE[100] = {
    1, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 16, 16, 17,
    18, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 30, 31, 33, 34, 36, 37, 38, 39, 41, 42, 44, 46, 47, 49,
    51, 53, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 79, 82, 85, 88, 91, 94, 98, 102, 106, 110, 115,
    120, 125, 130, 135, 141, 147, 153, 159, 165, 171, 178, 185, 193, 202, 211, 232, 243, 254, 255};
static const int8_t DX_PITCH[100] = {
    -128, -116, -104, -95, -85, -76, -68, -61, -56, -52, -49, -46, -43, -41, -39, -37, -35, -33, -32, -31,
    -30, -29, -28, -27, -26, -25, -24, -23, -22, -21, -20, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10,
    -9, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 38, 40, 43, 46, 49, 53, 58, 65, 73, 82,
    92, 103, 115, 127};

typedef struct {
    uint8_t rates[4], levels[4];
    uint8_t ix, rising, down;
    int32_t level, target, inc;
} dx_penv_t;

static void dx_penv_advance(dx_penv_t *p, int newix)
{
    p->ix = (uint8_t)newix;
    if (newix < 4) {
        p->target = DX_PITCH[p->levels[newix]] * (1 << 19);
        p->rising = p->target > p->level;
        p->inc = DX_PRATE[p->rates[newix]] * DX_PENV_UNIT;
    }
}

static void dx_penv_set(dx_penv_t *p, const uint8_t *r, const uint8_t *l)
{
    int i;
    for (i = 0; i < 4; i++) {
        p->rates[i] = r[i];
        p->levels[i] = l[i];
    }
    p->level = DX_PITCH[l[3]] * (1 << 19);
    p->down = 1;
    dx_penv_advance(p, 0);
}

static int32_t dx_penv_sample(dx_penv_t *p)
{
    if (p->ix < 3 || (p->ix < 4 && !p->down)) {
        if (p->rising) {
            p->level += p->inc;
            if (p->level >= p->target) {
                p->level = p->target;
                dx_penv_advance(p, p->ix + 1);
            }
        } else {
            p->level -= p->inc;
            if (p->level <= p->target) {
                p->level = p->target;
                dx_penv_advance(p, p->ix + 1);
            }
        }
    }
    return p->level;
}

static void dx_penv_keydown(dx_penv_t *p, int d)
{
    if (p->down != d) {
        p->down = (uint8_t)d;
        dx_penv_advance(p, d ? 0 : 3);
    }
}

/* ------------------------------------------------------------------------ LFO (Lfo) --- */
typedef struct {
    uint32_t phase, delta, delaystate, delayinc, delayinc2;
    uint8_t wave, sync, rnd;
} dx_lfo_t;

static void dx_lfo_reset(dx_lfo_t *o, const uint8_t *p)   /* p: the voice's bytes 137..142 */
{
    int a = 99 - p[1];
    o->delta = DX_LFO_DELTA[p[0] % 100u];
    if (a == 99) {
        o->delayinc = o->delayinc2 = ~0u;
    } else {
        a = (16 + (a & 15)) << (1 + (a >> 4));
        o->delayinc = DX_LFO_UNIT * (uint32_t)a;
        a &= 0xff80;
        a = a < 0x80 ? 0x80 : a;
        o->delayinc2 = DX_LFO_UNIT * (uint32_t)a;
    }
    o->wave = p[5];
    o->sync = p[4] != 0;
}

static void dx_lfo_keydown(dx_lfo_t *o)
{
    if (o->sync)
        o->phase = (1u << 31) - 1;
    o->delaystate = 0;
}

static int32_t dx_lfo_sample(dx_lfo_t *o)      /* 0 .. 1 in Q24 */
{
    int32_t x;
    o->phase += o->delta;
    switch (o->wave) {
    case 0:
        x = (int32_t)(o->phase >> 7);
        x ^= -(int32_t)(o->phase >> 31);
        return x & ((1 << 24) - 1);
    case 1:
        return (int32_t)((~o->phase ^ (1u << 31)) >> 8);
    case 2:
        return (int32_t)((o->phase ^ (1u << 31)) >> 8);
    case 3:
        return (int32_t)(((~o->phase) >> 7) & (1u << 24));
    case 4:
        return (1 << 23) + (dx_sin((int32_t)(o->phase >> 8)) >> 1);
    case 5:
        if (o->phase < o->delta)
            o->rnd = (uint8_t)((o->rnd * 179 + 17) & 0xff);
        return ((o->rnd ^ 0x80) + 1) << 16;
    }
    return 1 << 23;
}

static int32_t dx_lfo_delay(dx_lfo_t *o)
{
    uint32_t delta = o->delaystate < (1u << 31) ? o->delayinc : o->delayinc2;
    uint64_t d = (uint64_t)o->delaystate + delta;
    if (d > 0xFFFFFFFFull)
        return 1 << 24;
    o->delaystate = (uint32_t)d;
    return d < (1u << 31) ? 0 : (int32_t)((d >> 7) & ((1 << 24) - 1));
}

/* ------------------------------------------------------------------ the note (Dx7Note) --- */
typedef struct {
    dx_env_t env[6];
    dx_penv_t penv;
    dx_lfo_t lfo;
    int32_t base[6];             /* log frequency per operator, Q24 */
    int32_t phase[6], gain[6];   /* FmOpParams: phase, gain_out */
    int32_t level_in[6];
    int32_t fb[2];
    uint32_t ams[6];
    uint8_t fixed[6];
    uint8_t alg, fb_shift, on;
    int32_t pmdepth, pmsens, amdepth;
    int32_t bright;              /* sloopDX: the modulators' level offset, Q24 log2 (0 = as Dexed) */
    uint8_t noise;               /* sloopDX drums: a bit per operator (bit op, 0 = OP6) that plays noise instead of a
                                  * sine (dx_op_noise); set by drums.c after dx_init, never from a voice or a .syx */
    uint32_t nseed;              /* the noise generator (xorshift32) */
    int32_t nval[6];             /* each noise operator's held value */
} dxv_t;

static const uint8_t DX_ALG[32][6] = {      /* FmCore::algorithms (Dexed) */
    {0xc1, 0x11, 0x11, 0x14, 0x01, 0x14}, {0x01, 0x11, 0x11, 0x14, 0xc1, 0x14}, {0xc1, 0x11, 0x14, 0x01, 0x11, 0x14},
    {0xc1, 0x11, 0x94, 0x01, 0x11, 0x14}, {0xc1, 0x14, 0x01, 0x14, 0x01, 0x14}, {0xc1, 0x94, 0x01, 0x14, 0x01, 0x14},
    {0xc1, 0x11, 0x05, 0x14, 0x01, 0x14}, {0x01, 0x11, 0xc5, 0x14, 0x01, 0x14}, {0x01, 0x11, 0x05, 0x14, 0xc1, 0x14},
    {0x01, 0x05, 0x14, 0xc1, 0x11, 0x14}, {0xc1, 0x05, 0x14, 0x01, 0x11, 0x14}, {0x01, 0x05, 0x05, 0x14, 0xc1, 0x14},
    {0xc1, 0x05, 0x05, 0x14, 0x01, 0x14}, {0xc1, 0x05, 0x11, 0x14, 0x01, 0x14}, {0x01, 0x05, 0x11, 0x14, 0xc1, 0x14},
    {0xc1, 0x11, 0x02, 0x25, 0x05, 0x14}, {0x01, 0x11, 0x02, 0x25, 0xc5, 0x14}, {0x01, 0x11, 0x11, 0xc5, 0x05, 0x14},
    {0xc1, 0x14, 0x14, 0x01, 0x11, 0x14}, {0x01, 0x05, 0x14, 0xc1, 0x14, 0x14}, {0x01, 0x14, 0x14, 0xc1, 0x14, 0x14},
    {0xc1, 0x14, 0x14, 0x14, 0x01, 0x14}, {0xc1, 0x14, 0x14, 0x01, 0x14, 0x04}, {0xc1, 0x14, 0x14, 0x14, 0x04, 0x04},
    {0xc1, 0x14, 0x14, 0x04, 0x04, 0x04}, {0xc1, 0x05, 0x14, 0x01, 0x14, 0x04}, {0x01, 0x05, 0x14, 0xc1, 0x14, 0x04},
    {0x04, 0xc1, 0x11, 0x14, 0x01, 0x14}, {0xc1, 0x14, 0x01, 0x14, 0x04, 0x04}, {0x04, 0xc1, 0x11, 0x14, 0x04, 0x04},
    {0xc1, 0x14, 0x04, 0x04, 0x04, 0x04}, {0xc4, 0x04, 0x04, 0x04, 0x04, 0x04}};

static const int32_t DX_COARSE[32] = {
    -16777216, 0, 16777216, 26591258, 33554432, 38955489, 43368474, 47099600, 50331648, 53182516, 55732705,
    58039632, 60145690, 62083076, 63876816, 65546747, 67108864, 68576247, 69959732, 71268397, 72509921,
    73690858, 74816848, 75892776, 76922906, 77910978, 78860292, 79773775, 80654032, 81503396, 82323963,
    83117622};
static const uint8_t DX_VELDATA[64] = {
    0, 70, 86, 97, 106, 114, 121, 126, 132, 138, 142, 148, 152, 156, 160, 163, 166, 170, 173, 174, 178, 181,
    184, 186, 189, 190, 194, 196, 198, 200, 202, 205, 206, 209, 211, 214, 216, 218, 220, 222, 224, 225, 227,
    229, 230, 232, 233, 235, 237, 238, 240, 241, 242, 243, 244, 246, 246, 248, 249, 250, 251, 252, 253, 254};
static const uint8_t DX_EXPSCALE[33] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 14, 16, 19, 23, 27, 33,
                                        39, 47, 56, 66, 80, 94, 110, 126, 142, 158, 174, 190, 206, 222, 238, 250};
static const uint8_t DX_PMSENS[8] = {0, 10, 20, 33, 55, 92, 153, 255};
static const uint32_t DX_AMSENS[4] = {0, 4342338, 7171437, 16777216};

static int dx_scale_curve(int group, int depth, int curve)
{
    int scale = (curve == 0 || curve == 3) ? (group * depth * 329) >> 12
                                           : (DX_EXPSCALE[group > 32 ? 32 : group] * depth * 329) >> 15;
    return curve < 2 ? -scale : scale;
}

static int dx_scale_level(int note, int bp, int ld, int rd, int lc, int rc)
{
    int off = note - bp - 17;
    return off >= 0 ? dx_scale_curve((off + 1) / 3, rd, rc) : dx_scale_curve(-(off - 1) / 3, ld, lc);
}

static int32_t dx_osc_freq(int note, int fixed, int coarse, int fine, int detune)
{
    int32_t lf;
    if (!fixed) {
        lf = DX_DETUNED[(note & 127) * 15 + (detune > 14 ? 14 : detune)];
        lf += DX_COARSE[coarse & 31];
        lf += DX_FINE[fine % 100];
    } else {
        lf = (4458616 * ((coarse & 3) * 100 + fine)) >> 3;
        lf += detune > 7 ? 13457 * (detune - 7) : 0;
    }
    return lf;
}

/* an operator's output level (key scaling and velocity in) and its rate scaling, as Dx7Note::init */
static int dx_op_level(const uint8_t *o, int note, int vel, int *rs)
{
    int ol = dx_scaleoutlevel(o[16]) + dx_scale_level(note, o[8], o[9], o[10], o[11], o[12]);
    int vv = DX_VELDATA[(vel < 0 ? 0 : vel > 127 ? 127 : vel) >> 1] - 239;
    int x = note / 3 - 7;
    ol = ol > 127 ? 127 : ol;
    ol = ol * 32 + ((o[15] * vv + 7) >> 3) * 16;
    x = x < 0 ? 0 : x > 31 ? 31 : x;
    *rs = (o[13] * x) >> 3;
    return ol < 0 ? 0 : ol;
}

/* the voice changed while the note sounds (a knob, the voice list, the editor): everything but the
 * oscillator phases and the envelopes' positions, as Dexed's Dx7Note::update */
static void dx_update(dxv_t *v, const uint8_t *p, int note, int vel)
{
    int op, i;
    for (op = 0; op < 6; op++) {
        const uint8_t *o = p + op * 21;
        int rs, ol = dx_op_level(o, note, vel, &rs);
        dx_env_update(&v->env[op], o, o + 4, ol, rs);
        v->fixed[op] = o[17];
        v->base[op] = dx_osc_freq(note, o[17], o[18], o[19], o[20]);
        v->ams[op] = DX_AMSENS[o[14] & 3];
    }
    for (i = 0; i < 4; i++) {
        v->penv.rates[i] = p[126 + i];
        v->penv.levels[i] = p[130 + i];
    }
    v->alg = p[134] & 31;
    v->fb_shift = p[135] ? (uint8_t)(8 - p[135]) : 16;
    v->pmdepth = (p[139] * 165) >> 6;
    v->pmsens = DX_PMSENS[p[143] & 7];
    v->amdepth = (p[140] * 165) >> 6;
    dx_lfo_reset(&v->lfo, p + 137);
}

/* a new note from an unpacked voice (156 bytes); keeps nothing of the last one */
static void dx_init(dxv_t *v, const uint8_t *p, int note, int vel)
{
    int op;
    for (op = 0; op < 6; op++) {
        const uint8_t *o = p + op * 21;
        int rs, ol = dx_op_level(o, note, vel, &rs);
        dx_env_init(&v->env[op], o, o + 4, ol, rs);
        v->fixed[op] = o[17];
        v->base[op] = dx_osc_freq(note, o[17], o[18], o[19], o[20]);
        v->ams[op] = DX_AMSENS[o[14] & 3];
        v->phase[op] = 0;
        v->gain[op] = 0;
        v->nval[op] = 0;
    }
    dx_penv_set(&v->penv, p + 126, p + 130);
    v->alg = p[134] & 31;
    v->fb_shift = p[135] ? (uint8_t)(8 - p[135]) : 16;
    v->pmdepth = (p[139] * 165) >> 6;
    v->pmsens = DX_PMSENS[p[143] & 7];
    v->amdepth = (p[140] * 165) >> 6;
    v->fb[0] = v->fb[1] = 0;
    dx_lfo_reset(&v->lfo, p + 137);
    dx_lfo_keydown(&v->lfo);
    v->bright = 0;
    v->noise = 0;                                /* (a synth voice never has noise: drums.c sets it after this) */
    v->on = 1;
}

static void dx_keyup(dxv_t *v)
{
    int op;
    for (op = 0; op < 6; op++)
        dx_env_keydown(&v->env[op], 0);
    dx_penv_keydown(&v->penv, 0);
}

static int dx_playing(const dxv_t *v)          /* any carrier envelope still active */
{
    int op;
    if (!v->on)
        return 0;
    for (op = 0; op < 6; op++)
        if ((DX_ALG[v->alg][op] & 4) && dx_env_active(&v->env[op]))
            return 1;
    return 0;
}

/* FmOpKernel: one operator over the block (input: the modulation bus, 0 = none) */
static void dx_op(int32_t *out, const int32_t *in, int32_t phase, int32_t freq, int32_t g1, int32_t g2, int add)
{
    int32_t dg = (g2 - g1 + (DX_N >> 1)) >> DX_LG_N, g = g1;
    int i;
    for (i = 0; i < DX_N; i++) {
        int32_t y;
        g += dg;
        y = (int32_t)(((int64_t)dx_sin((int32_t)((uint32_t)phase + (uint32_t)(in ? in[i] : 0))) * g) >> 24);
        out[i] = add ? out[i] + y : y;
        phase = (int32_t)((uint32_t)phase + (uint32_t)freq);
    }
}

static void dx_op_fb(int32_t *out, int32_t phase, int32_t freq, int32_t g1, int32_t g2, int32_t *fb, int shift,
                     int add)
{
    int32_t dg = (g2 - g1 + (DX_N >> 1)) >> DX_LG_N, g = g1, y0 = fb[0], y = fb[1];
    int i;
    for (i = 0; i < DX_N; i++) {
        int32_t s;
        g += dg;
        s = (y0 + y) >> (shift + 1);
        y0 = y;
        y = (int32_t)(((int64_t)dx_sin((int32_t)((uint32_t)phase + (uint32_t)s)) * g) >> 24);
        out[i] = add ? out[i] + y : y;
        phase = (int32_t)((uint32_t)phase + (uint32_t)freq);
    }
    fb[0] = y0;
    fb[1] = y;
}

/* sloopDX drums: a noise operator. Sample and hold of a 32-bit xorshift, a new value twice per period of the
 * operator's frequency (its ratio / fixed frequency is the colour: low = rumble, high = hiss), at the operator's
 * level and envelope as a sine would have. No modulation input (a noise operator ignores what feeds it) */
__attribute__((noinline))                       /* (out of dx_compute: the synth path stays as it was) */
static void dx_op_noise(int32_t *out, int32_t phase, int32_t freq, int32_t g1, int32_t g2, int add, int32_t *val,
                        uint32_t *seed)
{
    int32_t dg = (g2 - g1 + (DX_N >> 1)) >> DX_LG_N, g = g1, x = *val;
    uint32_t s = *seed, every = (uint32_t)freq >= (1u << 23);
    int i;
    for (i = 0; i < DX_N; i++) {
        int32_t y, pn = (int32_t)((uint32_t)phase + (uint32_t)freq);
        g += dg;
        if (every || (((uint32_t)pn ^ (uint32_t)phase) & (1u << 23))) {
            s ^= s << 13;
            s ^= s >> 17;
            s ^= s << 5;
            x = (int32_t)s >> 7;                     /* +-2^24, as dx_sin */
        }
        y = (int32_t)(((int64_t)x * g) >> 24);
        out[i] = add ? out[i] + y : y;
        phase = pn;
    }
    *val = x;
    *seed = s;
}

/* one block: adds the note into out[DX_N]. pitch: an offset in Q24 log2 for the ratio operators and the
 * fixed ones alike (Dexed's pitch bend / master tune: the part's glide, LFO and tuning here); lfo_val /
 * lfo_delay as Dexed's synth gives them (dx_compute_lfo: the note's own) */
static void dx_compute_ext(dxv_t *v, int32_t *out, int32_t lfo_val, int32_t lfo_delay, int32_t pitch)
{
    static int32_t bus[2][DX_N];
    int32_t fq[6];
    uint8_t has[3] = {1, 0, 0};
    int32_t pmod, amod, senslfo, pm1;
    uint32_t pmd;
    int op;
    /* pitch: the LFO (Dexed: the larger of the LFO and the mod wheel; no wheel here), the pitch envelope */
    pmd = (uint32_t)v->pmdepth * (uint32_t)lfo_delay;
    senslfo = v->pmsens * (lfo_val - (1 << 23));
    pm1 = (int32_t)(((int64_t)pmd * (int64_t)senslfo) >> 39);
    pm1 = pm1 < 0 ? -pm1 : pm1;
    pmod = dx_penv_sample(&v->penv) + pm1 * (senslfo < 0 ? -1 : 1) + pitch;
    /* amplitude: the LFO */
    lfo_val = (1 << 24) - lfo_val;
    amod = (int32_t)(uint32_t)(((int64_t)v->amdepth * lfo_delay) >> 8);
    amod = (int32_t)(uint32_t)(((uint64_t)(uint32_t)amod * (uint64_t)(uint32_t)lfo_val) >> 24);
    for (op = 0; op < 6; op++) {                 /* Dx7Note::compute: frequency and level per operator */
        int32_t level;
        fq[op] = dx_freq(v->base[op] + (v->fixed[op] ? pitch : pmod));
        level = dx_env_sample(&v->env[op]);
        if (v->ams[op]) {                        /* (Dexed: a float exp per block; here a table) */
            uint32_t sensamp = (uint32_t)(((uint64_t)(uint32_t)amod * v->ams[op]) >> 24);
            uint32_t i = sensamp >> 14, pt;
            pt = i >= 1024u ? DX_AMS_PT[1024]
                            : DX_AMS_PT[i] + (uint32_t)(((uint64_t)(DX_AMS_PT[i + 1] - DX_AMS_PT[i]) * (sensamp & 16383u)) >> 14);
            level -= (int32_t)(uint32_t)(((uint64_t)(uint32_t)level * ((uint64_t)pt << 4)) >> 28);
        }
        if (v->bright && (DX_ALG[v->alg][op] & 3)) {   /* sloopDX: brightness on the modulators (ENV / LFO -> FLT, SHP) */
            level += v->bright;
            level = level < 0 ? 0 : level > (16 << 24) ? (16 << 24) : level;
        }
        v->level_in[op] = level;
    }
    for (op = 0; op < 6; op++) {                 /* FmCore::render */
        uint8_t fl = DX_ALG[v->alg][op];
        int add = (fl & 4) != 0, inb = (fl >> 4) & 3, outb = fl & 3;
        int32_t *o = outb == 0 ? out : bus[outb - 1];
        int32_t g1 = v->gain[op], g2 = dx_exp2(v->level_in[op] - (14 * (1 << 24)));
        v->gain[op] = g2;
        if (g1 >= 1120 || g2 >= 1120) {
            if (!has[outb])
                add = 0;
            if (v->noise && ((v->noise >> op) & 1u)) {   /* (drums only: a synth voice never gets here) */
                dx_op_noise(o, v->phase[op], fq[op], g1, g2, add, &v->nval[op], &v->nseed);
            } else if (inb == 0 || !has[inb]) {
                if ((fl & 0xc0) == 0xc0 && v->fb_shift < 16)
                    dx_op_fb(o, v->phase[op], fq[op], g1, g2, v->fb, v->fb_shift, add);
                else
                    dx_op(o, 0, v->phase[op], fq[op], g1, g2, add);
            } else {
                dx_op(o, bus[inb - 1], v->phase[op], fq[op], g1, g2, add);
            }
            has[outb] = 1;
        } else if (!add) {
            has[outb] = 0;
        }
        v->phase[op] = (int32_t)((uint32_t)v->phase[op] + ((uint32_t)fq[op] << DX_LG_N));
    }
}

static void dx_compute(dxv_t *v, int32_t *out, int32_t pitch)
{
    int32_t lv = dx_lfo_sample(&v->lfo), ld = dx_lfo_delay(&v->lfo);
    dx_compute_ext(v, out, lv, ld, pitch);
}
