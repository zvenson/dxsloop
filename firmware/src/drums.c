/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; FM drums: (C) 2026 Sven Trogus */
/* The drum track (track 4), in FM: every sound is a DX7 voice (dx7_bank.h DX_DRUM_VOICE) played by the
 * DX7 core (dx7_core.c), with what a drum needs and a DX7 has not: a fast exponential pitch sweep at
 * the hit (kick and tom punch), bursts (the clap's three or four hits), choke groups (a closed or pedal
 * hat cuts the open one) and one-shots (note-offs are ignored; a voice ends when it has died away).
 * Played by its step pattern, the keys when track 4 is selected, and its own MIDI channel (GLO ->
 * DRUMS, default 10); its own voices (outside the parts' voice budget). LEVEL / REV: GLO > DRUMS
 * (G_DRLVL, G_DRREV); PAN and MUTE: the drum track's P_PAN / P_MUTE. Rendered from the audio ISR.
 * P_E0 is the kit: four treatments of the FM kit, and USER (the first 16 voices of the DX7 user bank,
 * eng_dx7.c, one per lane). */
#define NDRUM 6

typedef struct {                  /* a kit: a treatment of the FM voices */
    const char *name, *style;
    int8_t dec, tune, bright;     /* rate offset (+: shorter), semitones, modulator level offset */
    uint8_t sweep;                /* sweep depth, Q7 (128 = as the voice) */
} fm_kit_t;
static const fm_kit_t FM_KITS[] = {
    {"ZVEN FM", "CLASSIC", 0, 0, 0, 128},
    {"TIGHT", "PUNCHY", 8, 2, 0, 100},
    {"BOOM", "DEEP", -6, -3, -6, 160},
    {"METAL", "BRIGHT", 0, 5, 10, 128},
    {"USER", "DX7 BANK", 0, 0, 0, 128},
};
#define DRUM_KITS (sizeof FM_KITS / sizeof FM_KITS[0])
#define DRUM_KIT_USER (DRUM_KITS - 1u)
static const char *const DRUM_KIT_NAMES[] = {"ZVEN FM", "TIGHT", "BOOM", "METAL", "USER"};
static const char *const DRUM_KIT_STYLES[] = {"CLASSIC", "PUNCHY", "DEEP", "BRIGHT", "DX7 BANK"};
static uint32_t drum_kit(void) { return (uint32_t)clamp(TDRUM->p[P_E0], 0, DRUM_KITS - 1); }
#define DRUM_DEFAULT_KIT 0

static struct {
    voice_t v[NDRUM];            /* active, note, vel, age; s[7]: the last output (declick) */
    uint32_t age;
    int32_t tail;                /* declick: the last output of cut voices, decaying */
    int32_t peak;                /* largest |output| since the UI last looked (TRACKS meter) */
    dxv_t dx[NDRUM];
    uint8_t patch[NDRUM][156];   /* the voice as started (bursts start it again) */
    uint8_t drum[NDRUM];         /* DX_DRUM index */
    uint8_t burst[NDRUM], quiet[NDRUM];
    int32_t sweep[NDRUM];        /* pitch sweep left, Q24 log2 */
    uint32_t t[NDRUM], next[NDRUM];   /* samples since the hit, next burst hit */
    int32_t gain[NDRUM];         /* the drum's level, Q15 */
    volatile uint16_t hits;      /* bit per lane hit since the UI last looked (pads, key LEDs) */
    volatile uint8_t kick;       /* a kick was hit (fx.c DUCK) */
    int32_t a0, a1;              /* the drum track's mute / solo attenuation over this block (fx.c), Q15, 0 = heard */
} drums;

/* ------------------------------------------------------------ lanes --- */
/* The drum track's 16 sounds, one per white key, F3 (kick) .. G5 (cowbell): kicks, snare and clap,
 * the hi-hats, rim and a second snare, the toms, the cymbals, the percussion. Each plays a GM note
 * (the sampled kits: their samples; the synthesised kits: drum_synth.c DS_MAP). A black key plays
 * the lane of the white key left of it (two fingers on one sound). */
static const uint8_t LANE_NOTE[DRUM_LANES] = {36, 35, 38, 39, 42, 46, 44, 37, 40, 43, 48, 49, 51, 70, 63, 56};
static const char *const LANE_NAME[DRUM_LANES] = {
    "KICK", "KICK 2", "SNARE", "CLAP", "HAT", "OPEN HAT", "PEDAL", "RIM",
    "SNARE 2", "LOW TOM", "HI TOM", "CRASH", "RIDE", "SHAKER", "CONGA", "COWBELL"};
static const char *const LANE_SHORT[DRUM_LANES] = {           /* 5 characters: tiles, dials */
    "kick", "kick2", "snare", "clap", "hat", "open", "pedal", "rim",
    "snr 2", "tom l", "tom h", "crash", "ride", "shake", "conga", "bell"};
/* a GM note (MIDI in, old projects) -> its lane: the nearest sound of the 16 (35..81; below: kick, above: shaker) */
static const uint8_t LANE_OF_GM[81 - 35 + 1] = {
    /* 35 */ 1, 0, 7, 2, 3, 8, 9, 4, 9, 6,
    /* 45 */ 9, 5, 10, 10, 11, 10, 12, 11, 12, 13,
    /* 55 */ 11, 15, 11, 13, 12, 14, 14, 14, 14, 14,
    /* 65 */ 14, 14, 15, 15, 13, 13, 15, 15, 13, 13,
    /* 75 */ 7, 7, 7, 14, 14, 15, 15};
static uint32_t lane_of_note(uint32_t note)
{
    return note < 35u ? 0u : note > 81u ? 13u : LANE_OF_GM[note - 35u];
}
/* the lane of key k (0 = F3 .. 26 = G5): white keys in order, a black key the white key left of it */
static uint32_t lane_of_key(uint32_t k)
{
    static const int8_t W[12] = {0, -1, 1, -1, 2, -1, 3, 4, -1, 5, -1, 6};   /* from F */
    int32_t i = W[k % 12u];
    if (i < 0)
        i = W[(k - 1u) % 12u];
    return (uint32_t)((int32_t)(k / 12u) * 7 + i) & 15u;
}
/* the white key of lane l (key index), for the key LEDs */
static uint32_t key_of_lane(uint32_t l)
{
    static const uint8_t K[7] = {0, 2, 4, 6, 7, 9, 11};      /* F G A B C D E */
    return (l / 7u) * 12u + K[l % 7u];
}

/* drum steps: a lane's bit, level, ratchet */
static int dstep_has(const dstep_t *s, uint32_t l) { return (s->on[(l >> 3) & 1u] >> (l & 7u)) & 1u; }
static uint32_t dstep_lvl(const dstep_t *s, uint32_t l) { return (s->lvl[(l >> 2) & 3u] >> ((l & 3u) * 2u)) & 3u; }
static uint32_t dstep_rat(const dstep_t *s, uint32_t l) { return (s->rat[(l >> 2) & 3u] >> ((l & 3u) * 2u)) & 3u; }
static uint32_t dstep_mask(const dstep_t *s) { return (uint32_t)s->on[0] | (uint32_t)s->on[1] << 8; }
static void dstep_set(dstep_t *s, uint32_t l, uint32_t lvl, uint32_t rat)   /* lane on, with its level and ratchet */
{
    uint32_t sh = (l & 3u) * 2u, b = (l >> 2) & 3u;
    s->on[(l >> 3) & 1u] |= (uint8_t)(1u << (l & 7u));
    s->lvl[b] = (uint8_t)((s->lvl[b] & ~(3u << sh)) | (lvl & 3u) << sh);
    s->rat[b] = (uint8_t)((s->rat[b] & ~(3u << sh)) | (rat & 3u) << sh);
}
static void dstep_clr(dstep_t *s, uint32_t l)                                /* lane off */
{
    uint32_t sh = (l & 3u) * 2u, b = (l >> 2) & 3u;
    s->on[(l >> 3) & 1u] &= (uint8_t)~(1u << (l & 7u));
    s->lvl[b] &= (uint8_t)~(3u << sh);
    s->rat[b] &= (uint8_t)~(3u << sh);
}

/* the velocity of a level: as played (LV_NORM: vel), ghost, soft, hard */
static uint32_t lvl_vel(uint32_t lvl, uint32_t vel)
{
    static const uint8_t V[4] = {0, 42, 72, 127};
    return lvl & 3u ? V[lvl & 3u] : vel;
}
/* a MIDI velocity -> the level it records as (keys play 100: LV_NORM) */
static uint32_t vel_lvl(uint32_t vel)
{
    return vel < 56u ? LV_GHOST : vel < 88u ? LV_SOFT : vel < 116u ? LV_NORM : LV_HARD;
}

/* the voice of drum d in the current kit (kit treatments applied) */
static void drum_voice(uint32_t d, uint32_t kit, uint8_t *p)
{
    const fm_kit_t *k = &FM_KITS[kit];
    uint32_t i, op, alg;
    if (kit == DRUM_KIT_USER && dx_user_ok && d < 16u) {
        dx_unpack(dx_user[d], p);
        dx_sanitize(p);
        return;
    }
    for (i = 0; i < 156u; i++)
        p[i] = DX_DRUM_VOICE[d][i];
    alg = p[134] & 31u;
    for (op = 0; op < 6u; op++) {
        uint8_t *o = p + op * 21u;
        if (k->dec)
            o[1] = (uint8_t)dx_clampi(o[1] + k->dec, 1, 99);
        if (k->bright && !(DX_ALG[alg][op] & 4) && o[16])
            o[16] = (uint8_t)dx_clampi(o[16] + k->bright, 0, 99);
    }
}

static void drum_start(uint32_t i)               /* (re)start voice i: a hit, or the next hit of a burst */
{
    const dx_drum_t *dd = &DX_DRUM[drums.drum[i]];
    int32_t note = dd->note + FM_KITS[drum_kit()].tune;
    if (drums.drum[i] == DX_NDRUM - 1u && drums.v[i].note == 77u)
        note += 7;                                   /* the click's accent: a fifth up */
    dx_init(&drums.dx[i], drums.patch[i], clamp(note, 0, 127), drums.v[i].vel);
    drums.sweep[i] = dd->sweep * (int32_t)(((1 << 24) / 12) * FM_KITS[drum_kit()].sweep >> 7);
}

static void drum_on(uint32_t note, uint32_t vel)
{
    uint32_t i, d, lane = lane_of_note(note), kit = drum_kit();
    voice_t *v = 0;
    if (note == 76u || note == 77u) {                /* the click (seq.c click_tick): the clave */
        d = DX_NDRUM - 1u;
    } else {
        d = lane;
        drums.hits |= (uint16_t)(1u << lane);       /* the pads and key LEDs */
    }
    if (lane <= 1u && d == lane)
        drums.kick = 1;                             /* (DUCK) */
    if (DX_DRUM[d].choke)                           /* the others of its choke group stop (declicked) */
        for (i = 0; i < NDRUM; i++)
            if (drums.v[i].active && drums.drum[i] != d && DX_DRUM[drums.drum[i]].choke == DX_DRUM[d].choke) {
                drums.v[i].active = 0;
                drums.tail += drums.v[i].s[7];
            }
    for (i = 0; i < NDRUM && !v; i++)               /* the same drum again: its own voice */
        if (drums.v[i].active && drums.drum[i] == d)
            v = &drums.v[i];
    for (i = 0; i < NDRUM && !v; i++)
        if (!drums.v[i].active)
            v = &drums.v[i];
    if (!v) {                                       /* the oldest */
        v = &drums.v[0];
        for (i = 1; i < NDRUM; i++)
            if (drums.v[i].age < v->age)
                v = &drums.v[i];
    }
    i = (uint32_t)(v - drums.v);
    if (v->active)
        drums.tail += v->s[7];
    v->note = (uint8_t)note;
    v->vel = (uint8_t)(vel ? vel : 1u);
    v->active = 1;
    v->s[7] = 0;
    v->age = ++drums.age;
    drums.drum[i] = (uint8_t)d;
    drums.burst[i] = DX_DRUM[d].burst > 1u ? (uint8_t)(DX_DRUM[d].burst - 1u) : 0;
    drums.t[i] = 0;
    drums.next[i] = DX_DRUM[d].burst_n;
    drums.quiet[i] = 0;
    drums.gain[i] = DX_DRUM[d].level * 258;
    drum_voice(d, kit, drums.patch[i]);
    drum_start(i);
}

/* adds the drums into the dry mix and the reverb send; mono != 0: into mono instead, before the
 * pan and the send (the SLICER, slicer.c slicer_drums, does those after it) */
static inline void drums_mix(int32_t *ml, int32_t *mr, int32_t *rev, int32_t *mono, uint32_t n)
{
    uint32_t k, i;
    int32_t lvl = song.g[G_DRLVL] * 200, send = song.g[G_DRREV] * 258, pk = drums.peak;
    int32_t pan = trk[TRK_DRUM].p[P_PAN], gl = 4096 - (pan > 0 ? pan * 64 : 0), gr = 4096 + (pan < 0 ? pan * 64 : 0);
    int32_t buf[DX_N];
    for (i = 0; i < n && drums.tail; i++) {         /* declick tail, ~0.4 ms */
        if (mono) {
            mono[i] += drums.tail;
        } else {
            ml[i] += drums.tail;
            mr[i] += drums.tail;
        }
        drums.tail -= drums.tail / 16 + (drums.tail > 0 ? 1 : drums.tail < 0 ? -1 : 0);
    }
    if (n != DX_N)                                  /* (the mix runs in blocks of CTL) */
        return;
    for (k = 0; k < NDRUM; k++) {
        voice_t *v = &drums.v[k];
        int32_t rp = 0;
        if (!v->active)
            continue;
        if (drums.burst[k] && drums.t[k] >= drums.next[k]) {   /* the next hit of a burst */
            drums.burst[k]--;
            drums.next[k] += DX_DRUM[drums.drum[k]].burst_n;
            drum_start(k);
        }
        for (i = 0; i < DX_N; i++)
            buf[i] = 0;
        dx_compute(&drums.dx[k], buf, drums.sweep[k]);
        if (drums.sweep[k])
            drums.sweep[k] = mulq16(drums.sweep[k], DX_DRUM[drums.drum[k]].sweep_k);
        for (i = 0; i < DX_N; i++) {
            int32_t s = clamp(buf[i] >> 10, -65535, 65535);   /* one carrier at full level: 32768 */
            rp = s > rp ? s : -s > rp ? -s : rp;
            s = mulq15(mulq15(s, drums.gain[k]),
                       mulq15(lvl, 32767 - drums.a0 - (((drums.a1 - drums.a0) * (int32_t)i) >> CTL_LOG2)));
            v->s[7] = s;
            if (s > pk || -s > pk)
                pk = s < 0 ? -s : s;
            if (mono) {
                mono[i] += s;
                continue;
            }
            ml[i] += (s * gl) >> 12;
            mr[i] += (s * gr) >> 12;
            if (send)
                rev[i] += mulq15(s, send);
        }
        drums.t[k] += DX_N;
        /* the end: died away (the voice itself below -66 dB of a full carrier for 8 blocks, whatever its
         * level, after the first 10 ms and the last burst hit),
         * the carriers done, or 8 s */
        drums.quiet[k] = (uint8_t)(rp < 16 && drums.t[k] > FS / 100u && !drums.burst[k] ? drums.quiet[k] + 1u : 0u);
        if (drums.quiet[k] >= 8u || !dx_playing(&drums.dx[k]) || drums.t[k] > 8u * FS) {
            v->active = 0;
            drums.tail += v->s[7];                  /* no step at the end */
            v->s[7] = 0;
        }
    }
    drums.peak = pk;
}
static void drums_render(int32_t *ml, int32_t *mr, int32_t *rev, uint32_t n) { drums_mix(ml, mr, rev, 0, n); }
static void drums_render_mono(int32_t *mono, uint32_t n) { drums_mix(0, 0, 0, mono, n); }
