/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; FM drums: (C) 2026 Sven Trogus */
/* The drum track (track 4), in FM: every sound is a DX7 voice (dx7_bank.h DX_DRUM_VOICE) played by the
 * DX7 core (dx7_core.c), with what a drum needs and a DX7 has not: a fast exponential pitch sweep at
 * the hit (kick and tom punch), bursts (the clap's three or four hits), choke groups (a closed or pedal
 * hat cuts the open one) and one-shots (note-offs are ignored; a voice ends when it has died away).
 * Played by its step pattern, the keys when track 4 is selected, and its own MIDI channel (GLO ->
 * DRUMS, default 10); its own voices (outside the parts' voice budget). LEVEL / REV: GLO > DRUMS
 * (G_DRLVL, G_DRREV); PAN and MUTE: the drum track's P_PAN / P_MUTE. Rendered from the audio ISR.
 * P_E0 is the kit: DX KIT (dx7_bank.h DX_DRUM_VOICE), 808 FM, ELECTRO and METAL (DX_KIT_VOICE: each its own
 * voices, algorithms and sweeps), and MY KIT (sloopDX 2.1: a kit in RAM, ukit: a copy of DX KIT at first, then
 * what SAVE on the kit page stores, a .syx kit from the editor or a dice kit; kept in flash).
 * Every lane has macros on top of its kit (TUNE DECAY SWEEP BRIGHT NOISE LEVEL PAN CHOKE REV, dext.m: in the
 * project) and a step can lock TUNE / DECAY of one lane (dext.lock). The voices sum into a stereo bus (lane pan),
 * then DRIVE and COMP (drums_bus), and a reverb send per lane after the compressor. */
#define NDRUM 6

static const char *const DRUM_KIT_NAMES[] = {"DX KIT", "808 FM", "ELECTRO", "METAL", "MY KIT"};
static const char *const DRUM_KIT_STYLES[] = {"CLASSIC", "ROUND", "PUNCHY", "INDUSTRIAL", "YOURS"};
#define DRUM_KITS 5u
#define KIT_USER 4u                               /* MY KIT */
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
    uint8_t kit[NDRUM];          /* the kit it was hit in (its sweep, burst, choke) */
    uint8_t burst[NDRUM], quiet[NDRUM];
    int32_t sweep[NDRUM];        /* pitch sweep left, Q24 log2 */
    uint32_t t[NDRUM], next[NDRUM];   /* samples since the hit, next burst hit */
    int32_t gain[NDRUM];         /* the drum's level, Q15 */
    volatile uint16_t hits;      /* bit per lane hit since the UI last looked (pads, key LEDs) */
    volatile uint8_t kick;       /* a kick was hit (fx.c DUCK) */
    int32_t a0, a1;              /* the drum track's mute / solo attenuation over this block (fx.c), Q15, 0 = heard */
    int32_t drv_lp[2], cmp_env, cmp_g;   /* the bus: DRIVE's tone filters, COMP's peak follower and gain (Q15) */
    int8_t tune[NDRUM];          /* the hit's tuning (TUNE + a lock), semitones */
    uint8_t swd[NDRUM], choke[NDRUM];   /* its sweep depth (semitones) and choke group */
    uint16_t swk[NDRUM];         /* its sweep decay per block, Q16 */
    int16_t pgl[NDRUM], pgr[NDRUM];   /* its pan, Q12 */
    int16_t send[NDRUM];         /* its reverb send, Q14 (16384 = the drum REV as it is) */
    uint8_t bus_tail;            /* blocks the bus still runs after the last voice (DRIVE / COMP states settle) */
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

/* MY KIT: 16 lane voices and their playing data in RAM (the click stays DX KIT's) */
typedef struct {
    uint8_t v[DRUM_LANES][156];
    dx_drum_t dd[DRUM_LANES];
    uint32_t seed;               /* a dice kit: its seed (shown), 0 = none */
} ukit_t;
static ukit_t ukit;
static uint8_t ukit_ok;                         /* MY KIT holds a kit (else: DX KIT, on first use) */
static void ukit_from(uint32_t kit);
static drum_ext_t dext;                         /* the working project's lane macros and locks */
static uint16_t drum_lock;                       /* the lock of the step being played (seq.c drum_step), 0 = none */
static volatile uint8_t drum_hand;               /* 0x80 | lane: a drum played by hand (a key, MIDI; seq.c drum_input),
                                                  * not by the sequencer: the lane page follows it */

/* drum d of a kit: its voice bytes and its playing data */
static const uint8_t *drum_vbytes(uint32_t kit, uint32_t d)
{
    if (kit == KIT_USER && d < DRUM_LANES) {
        if (!ukit_ok)
            ukit_from(0);
        return ukit.v[d];
    }
    return kit >= 1u && kit <= DX_NKITS ? DX_KIT_VOICE[kit - 1u][d] : DX_DRUM_VOICE[d];
}
static const dx_drum_t *drum_dd(uint32_t kit, uint32_t d)
{
    if (kit == KIT_USER && d < DRUM_LANES) {
        if (!ukit_ok)
            ukit_from(0);
        return &ukit.dd[d];
    }
    return kit >= 1u && kit <= DX_NKITS ? &DX_KIT[kit - 1u][d] : &DX_DRUM[d];
}
static void ukit_from(uint32_t kit)              /* MY KIT := a factory kit */
{
    uint32_t d;
    ukit_ok = 1;
    kit = kit == KIT_USER ? 0u : kit;
    for (d = 0; d < DRUM_LANES; d++) {
        memcpy(ukit.v[d], drum_vbytes(kit, d), 156);
        ukit.dd[d] = *drum_dd(kit, d);
    }
    ukit.seed = 0;
}

/* ---- lane macros ---- */
static const struct { const char *name; int8_t min, max; } DM_DESC[DM_N] = {
    {"tune", -24, 24}, {"decay", -40, 40}, {"sweep", -40, 40}, {"bright", -40, 40}, {"noise", -40, 40},
    {"level", -40, 20}, {"pan", -64, 63}, {"choke", 0, 4}, {"rev", -64, 63}};
static const uint16_t DM_GAIN[61] = {                  /* LEVEL -20 .. +10 dB in 1/2 dB, Q12 */
    410, 434, 460, 487, 516, 546, 579, 613, 649, 688, 728, 772, 817, 866, 917, 971, 1029, 1090, 1154, 1223, 1295,
    1372, 1453, 1539, 1631, 1727, 1830, 1938, 2053, 2175, 2303, 2440, 2584, 2738, 2900, 3072, 3254, 3446, 3651,
    3867, 4096, 4339, 4596, 4868, 5157, 5462, 5786, 6129, 6492, 6876, 7284, 7715, 8173, 8657, 9170, 9713, 10289,
    10898, 11544, 12228, 12953};
static int32_t dm_get(uint32_t lane, uint32_t k) { return lane < DRUM_LANES && k < DM_N ? dext.m[lane][k] : 0; }
static void dm_add(uint32_t lane, uint32_t k, int32_t s)
{
    if (lane < DRUM_LANES && k < DM_N)
        dext.m[lane][k] = (int8_t)clamp(dext.m[lane][k] + s, DM_DESC[k].min, DM_DESC[k].max);
}
static void dm_format(uint32_t k, int32_t v, char *b)   /* a macro's value, <= 6 characters */
{
    static const char *const CH[5] = {"kit", "off", "A", "B", "C"};
    char t[8];
    if (k == DM_CHOKE) {
        str_cpy(b, CH[clamp(v, 0, 4)], 6);
    } else if (k == DM_PAN) {
        if (!v) {
            str_cpy(b, "C", 6);
        } else {
            b[0] = v < 0 ? 'L' : 'R';
            fmt_int(b + 1, v < 0 ? -v : v);
        }
    } else if (k == DM_REV) {
        fmt_int(b, 64 + v);
    } else if (k == DM_LEVEL) {                     /* 1/2 dB -> dB: "-3.5", "+2", "0" */
        int32_t a = v < 0 ? -v : v;
        str_cpy(b, v > 0 ? "+" : v < 0 ? "-" : "", 6);
        fmt_int(t, a / 2);
        str_cpy(b + str_len(b), t, 6);
        if (a & 1)
            str_cpy(b + str_len(b), ".5", 6);
    } else {
        fmt_int(t, v);
        str_cpy(b, v > 0 ? "+" : "", 6);
        str_cpy(b + str_len(b), t, 6);
    }
}

/* parameter locks: one lane's TUNE (semitones -16..15) and DECAY (-48..45 in steps of 3) on a step */
static uint32_t dlock_lane(uint32_t lk) { return lk & 15u; }
static int dlock_has_tune(uint32_t lk) { return (lk >> 4) & 1u; }
static int dlock_has_decay(uint32_t lk) { return (lk >> 5) & 1u; }
static int32_t dlock_tune(uint32_t lk) { return dlock_has_tune(lk) ? (int32_t)((lk >> 6) & 31u) - (((lk >> 6) & 16u) ? 32 : 0) : 0; }
static int32_t dlock_decay(uint32_t lk) { return dlock_has_decay(lk) ? ((int32_t)((lk >> 11) & 31u) - (((lk >> 11) & 16u) ? 32 : 0)) * 3 : 0; }
static uint16_t dlock_make(uint32_t lane, int ht, int32_t tune, int hd, int32_t decay)
{
    uint32_t t = (uint32_t)clamp(tune, -16, 15) & 31u, d = (uint32_t)clamp(decay / 3, -16, 15) & 31u;
    if (!ht && !hd)
        return 0;
    return (uint16_t)((lane & 15u) | (ht ? 16u : 0u) | (hd ? 32u : 0u) | t << 6 | d << 11);
}

/* the voice of drum d in kit kit, with lane d's macros and a lock (tune, decay) on top */
static void drum_voice(uint32_t d, uint32_t kit, uint8_t *p, int32_t tune, int32_t decay)
{
    uint32_t i, op, alg, nmask = drum_dd(kit, d)->noise;
    int32_t bright = dm_get(d, DM_BRIGHT), noise = dm_get(d, DM_NOISE);
    {
        const uint8_t *src = drum_vbytes(kit, d);
        for (i = 0; i < 156u; i++)
            p[i] = src[i];
    }
    if (d >= DRUM_LANES)                            /* (the click: as it is) */
        return;
    alg = p[134] & 31u;
    for (op = 0; op < 6u; op++) {
        uint8_t *o = p + op * 21u;
        if (decay)                                  /* DECAY: the falling rates of every operator (+: longer) */
            for (i = 1; i < 4u; i++)
                o[i] = (uint8_t)dx_clampi(o[i] - decay / 2, 1, 99);
        if ((nmask >> op) & 1u) {                   /* NOISE: the noise operators' level */
            if (noise)
                o[16] = (uint8_t)(noise <= -40 ? 0 : dx_clampi(o[16] + noise / 2, 0, 99));
        } else if (bright && (DX_ALG[alg][op] & 3) && o[16]) {   /* BRIGHT: the modulators (they write a bus) */
            o[16] = (uint8_t)dx_clampi(o[16] + bright / 2, 0, 99);
        }
        if (tune && o[17]) {                        /* TUNE: a fixed frequency moves as well (a semitone: 2.51 / 100 decade) */
            int32_t f = (o[18] & 3) * 100 + o[19] + (tune * 251 + (tune < 0 ? -50 : 50)) / 100;
            f = clamp(f, 0, 399);
            o[18] = (uint8_t)(f / 100);
            o[19] = (uint8_t)(f % 100);
        }
    }
}

/* ---- MY KIT: a kit with its macros baked in, its flash image, its .syx ---- */
static void ukit_bake(uint32_t kit)              /* MY KIT := kit, with every lane's macros in it; the macros go to 0 */
{
    uint32_t d;
    for (d = 0; d < DRUM_LANES; d++) {
        uint8_t p[156];
        const dx_drum_t *dd = drum_dd(kit, d);
        dx_drum_t r = *dd;
        int32_t sw = dm_get(d, DM_SWEEP), m = dm_get(d, DM_CHOKE);
        uint32_t kk = dd->sweep_k ? dd->sweep_k : 64000u;
        drum_voice(d, kit, p, dm_get(d, DM_TUNE), dm_get(d, DM_DECAY));   /* (lane d read before it is written) */
        r.name = LANE_NAME[d];
        r.note = (uint8_t)clamp(dd->note + dm_get(d, DM_TUNE), 0, 127);
        r.level = (uint8_t)clamp((dd->level * (int32_t)DM_GAIN[dm_get(d, DM_LEVEL) + 40]) >> 12, 1, 127);
        r.sweep = (uint8_t)clamp((int32_t)dd->sweep * (40 + sw) / 40 + (sw > 0 ? sw / 4 : 0), 0, 60);
        r.sweep_k = r.sweep ? (uint16_t)(65536u - (65536u - kk) * 80u / (uint32_t)(80 + sw)) : dd->sweep_k;
        r.choke = (uint8_t)(m == 0 ? dd->choke : m == 1 ? 0 : m - 1);
        r.pan = (int8_t)clamp(dd->pan + dm_get(d, DM_PAN), -64, 63);
        r.rev = (uint8_t)clamp(dd->rev + dm_get(d, DM_REV), 0, 127);
        memcpy(ukit.v[d], p, 156);
        ukit.dd[d] = r;
    }
    if (kit != KIT_USER)
        ukit.seed = 0;
    ukit_ok = 1;
    memset(dext.m, 0, sizeof dext.m);                /* (they are in the kit now; the locks stay) */
}

#define UKIT_MAGIC 0x31544B44u                   /* "DKT1": MY KIT in flash (project.c ukit_store / ukit_boot) */
typedef struct { uint8_t note, level, sweep, burst, choke, noise, rev; int8_t pan; uint16_t sweep_k, burst_n; } ukit_row_t;
typedef struct { uint32_t magic, seed; uint8_t v[DRUM_LANES][156]; ukit_row_t row[DRUM_LANES]; } ukit_img_t;
static ukit_img_t ukit_img __attribute__((section(".pool")));
static void ukit_to_img(ukit_img_t *im)
{
    uint32_t d;
    if (!ukit_ok)
        ukit_from(0);
    memset(im, 0, sizeof *im);
    im->magic = UKIT_MAGIC;
    im->seed = ukit.seed;
    for (d = 0; d < DRUM_LANES; d++) {
        const dx_drum_t *r = &ukit.dd[d];
        ukit_row_t *w = &im->row[d];
        memcpy(im->v[d], ukit.v[d], 156);
        w->note = r->note, w->level = r->level, w->sweep = r->sweep, w->burst = r->burst, w->choke = r->choke;
        w->noise = r->noise, w->rev = r->rev, w->pan = r->pan, w->sweep_k = r->sweep_k, w->burst_n = r->burst_n;
    }
}
static int ukit_from_img(const ukit_img_t *im)   /* 0 = taken; every value inside its range */
{
    uint32_t d;
    if (im->magic != UKIT_MAGIC)
        return 1;
    for (d = 0; d < DRUM_LANES; d++) {
        const ukit_row_t *w = &im->row[d];
        dx_drum_t *r = &ukit.dd[d];
        memcpy(ukit.v[d], im->v[d], 156);
        dx_sanitize(ukit.v[d]);
        r->name = LANE_NAME[d];
        r->note = (uint8_t)clamp(w->note, 0, 127), r->level = (uint8_t)clamp(w->level, 0, 127);
        r->sweep = (uint8_t)clamp(w->sweep, 0, 60), r->sweep_k = w->sweep_k;
        r->burst = (uint8_t)clamp(w->burst, 0, 8), r->burst_n = (uint16_t)clamp(w->burst_n, 0, 9000);
        r->choke = (uint8_t)clamp(w->choke, 0, 3), r->noise = (uint8_t)(w->noise & 63u);
        r->pan = (int8_t)clamp(w->pan, -64, 63), r->rev = (uint8_t)clamp(w->rev, 0, 127);
    }
    ukit.seed = im->seed;
    ukit_ok = 1;
    return 0;
}

/* MY KIT as a DX7 32-voice bulk dump (the 4096 voice bytes): voices 1-16 the lanes (any DX7 tool reads them),
 * 17-32 the drum table, one lane each, named "KIT DATA01".. (note, level, sweep, sweep_k, burst, burst_n, choke,
 * the noise operators, pan, rev; the dice seed in the first). Not a voice: they would play as silence */
static const char UKIT_SYX_NAME[8] = {'K', 'I', 'T', ' ', 'D', 'A', 'T', 'A'};
static void ukit_syx_put(uint8_t *data)
{
    uint32_t d, k;
    ukit_to_img(&ukit_img);
    for (d = 0; d < DRUM_LANES; d++) {
        const ukit_row_t *w = &ukit_img.row[d];
        uint8_t *q = data + (DRUM_LANES + d) * 128u;
        dx_pack(ukit_img.v[d], data + d * 128u);
        memset(q, 0, 128);
        q[0] = w->note, q[1] = w->level, q[2] = w->sweep;
        q[3] = w->sweep_k & 127u, q[4] = (w->sweep_k >> 7) & 127u, q[5] = (uint8_t)(w->sweep_k >> 14);
        q[6] = w->burst, q[7] = w->burst_n & 127u, q[8] = (w->burst_n >> 7) & 127u, q[9] = (uint8_t)(w->burst_n >> 14);
        q[10] = w->choke, q[11] = w->noise & 63u, q[12] = (uint8_t)(w->pan + 64), q[13] = w->rev & 127u;
        for (k = 0; k < 5u; k++)
            q[14 + k] = d ? 0u : (uint8_t)((ukit_img.seed >> (7u * k)) & 127u);
        q[20] = 1;                                   /* (the table's version) */
        for (k = 0; k < 8u; k++)
            q[118 + k] = (uint8_t)UKIT_SYX_NAME[k];
        q[126] = (uint8_t)('0' + (d + 1u) / 10u), q[127] = (uint8_t)('0' + (d + 1u) % 10u);
    }
}
static int ukit_syx_is_kit(const uint8_t *data)  /* the 4096 voice bytes of a dump: is it a kit (names 17-32)? */
{
    uint32_t d, k;
    for (d = 0; d < DRUM_LANES; d++)
        for (k = 0; k < 8u; k++)
            if (data[(DRUM_LANES + d) * 128u + 118u + k] != (uint8_t)UKIT_SYX_NAME[k])
                return 0;
    return 1;
}
static int ukit_syx_get(const uint8_t *data)     /* a kit dump -> MY KIT (RAM); 0 = taken, 1 = not a kit */
{
    uint32_t d, k;
    if (!ukit_syx_is_kit(data))
        return 1;
    memset(&ukit_img, 0, sizeof ukit_img);
    ukit_img.magic = UKIT_MAGIC;
    for (d = 0; d < DRUM_LANES; d++) {
        const uint8_t *q = data + (DRUM_LANES + d) * 128u;
        ukit_row_t *w = &ukit_img.row[d];
        dx_unpack(data + d * 128u, ukit_img.v[d]);
        w->note = q[0], w->level = q[1], w->sweep = q[2];
        w->sweep_k = (uint16_t)(q[3] | q[4] << 7 | (q[5] & 3u) << 14);
        w->burst = q[6], w->burst_n = (uint16_t)(q[7] | q[8] << 7 | (q[9] & 3u) << 14);
        w->choke = q[10], w->noise = q[11], w->pan = (int8_t)((int32_t)q[12] - 64), w->rev = q[13];
        if (!d)
            for (k = 0; k < 5u; k++)
                ukit_img.seed |= (uint32_t)(q[14 + k] & 127u) << (7u * k);
    }
    return ukit_from_img(&ukit_img);
}

/* ---- the dice: MY KIT from rules per lane (frequency, decay, sweep, noise ranges), the same kit for the same
 * seed (1..65535, shown; the editor rolls a given one) ---- */
static uint32_t dk_s;
static int32_t dk_rnd(int32_t a, int32_t b)
{
    dk_s ^= dk_s << 13;
    dk_s ^= dk_s >> 17;
    dk_s ^= dk_s << 5;
    return a + (int32_t)(dk_s % (uint32_t)(b - a + 1));
}
static const uint16_t DK_SWK[12] = {58071, 59853, 60949, 61691, 62441, 63201, 63661, 63970, 64358, 64592, 64748, 64944};
                                                 /* (sweep decay per block for 6, 8, 10, 12, 15, 20 .. 80 ms) */
static void dk_voice(uint8_t *p, uint32_t alg, uint32_t fb, uint32_t lane)
{
    uint32_t op, k;
    memset(p, 0, 156);
    for (op = 0; op < 6u; op++) {
        uint8_t *o = p + op * 21u;
        o[0] = o[1] = o[2] = o[3] = 99;
        o[8] = 39;
        o[20] = 7;
    }
    for (k = 0; k < 4u; k++)
        p[126 + k] = 99, p[130 + k] = 50;
    p[134] = (uint8_t)(alg - 1u), p[135] = (uint8_t)fb, p[136] = 1;
    p[137] = 35, p[141] = 1, p[144] = 24;
    for (k = 0; k < 10u; k++)
        p[145 + k] = (uint8_t)(k < 5u ? "DICE "[k] : (k - 5u < str_len(LANE_SHORT[lane]) ? LANE_SHORT[lane][k - 5u] : ' '));
}
/* operator n (1..6): a decay (r2 to l2, then r3 to 0), its level, a fixed frequency code (100 log10 Hz: 300 = 1 kHz,
 * 399 = 9.8 kHz) or a ratio (coarse, fine), velocity sensitivity */
static void dk_op(uint8_t *p, uint32_t n, int32_t r2, int32_t l2, int32_t r3, int32_t out, int32_t fixed, int32_t f,
                  int32_t vel)
{
    uint8_t *o = p + (6u - n) * 21u;
    o[1] = (uint8_t)r2, o[2] = (uint8_t)r3, o[4] = 99, o[5] = (uint8_t)l2, o[6] = 0;
    o[15] = (uint8_t)vel, o[16] = (uint8_t)out, o[17] = (uint8_t)fixed;
    o[18] = (uint8_t)(fixed ? f / 100 : f >> 8), o[19] = (uint8_t)(fixed ? f % 100 : f & 255);
}
#define DK_RATIO(c, fine) ((c) << 8 | (fine))
static void dk_lane(uint32_t l, const uint32_t *hf)   /* lane l of MY KIT from the rules (dk_s goes on) */
{
    {
        uint8_t *p = ukit.v[l];
        dx_drum_t *r = &ukit.dd[l];
        int32_t a, b;
        memset(r, 0, sizeof *r);
        r->name = LANE_NAME[l], r->level = (uint8_t)dk_rnd(92, 104), r->rev = 64;
        switch (l) {
        case 0: case 1: case 9: case 10: case 14: {   /* kicks, toms, conga: a body, a modulator, a click */
            int kick = l <= 1u, conga = l == 14u;
            dk_voice(p, 1, (uint32_t)dk_rnd(0, kick ? 4 : 2), l);
            dk_op(p, 1, kick ? dk_rnd(40, 62) : dk_rnd(50, 66), 0, 99, 99, 0, DK_RATIO(dk_rnd(0, 1), 0), 2);
            dk_op(p, 2, dk_rnd(70, 92), 0, 99, dk_rnd(kick ? 40 : 28, kick ? 78 : 62), 0, DK_RATIO(dk_rnd(0, conga ? 4 : 2), dk_rnd(0, 50)), 5);
            if (dk_rnd(0, 1)) {
                dk_op(p, 3, dk_rnd(90, 97), 0, 99, dk_rnd(40, 70), 1, dk_rnd(360, 399), 4);
                r->noise = 1u << 3;                  /* (OP3) */
            }
            r->note = (uint8_t)(kick ? dk_rnd(26, 36) : conga ? dk_rnd(56, 68) : l == 9u ? dk_rnd(38, 46) : dk_rnd(46, 54));
            r->sweep = (uint8_t)(kick ? dk_rnd(12, 36) : conga ? dk_rnd(2, 8) : dk_rnd(4, 14));
            r->sweep_k = DK_SWK[kick ? dk_rnd(1, 8) : conga ? dk_rnd(0, 3) : dk_rnd(5, 10)];
            break;
        }
        case 2: case 8: {                            /* snares: a body, a tone, noise on a high carrier */
            int32_t f = l == 8u ? 6 : 0;
            dk_voice(p, 5, 0, l);
            a = dk_rnd(60, 72) + f;
            dk_op(p, 1, a, 0, 99, dk_rnd(80, 95), 0, DK_RATIO(1, 0), 3);
            dk_op(p, 2, dk_rnd(78, 90), 0, 99, dk_rnd(40, 75), 0, DK_RATIO(dk_rnd(1, 3), dk_rnd(0, 60)), 5);
            b = dk_rnd(58, 70) + f;
            dk_op(p, 5, b, 0, 99, dk_rnd(82, 96), 1, dk_rnd(340, 385), 3);
            dk_op(p, 6, b, 0, 99, 99, 1, dk_rnd(380, 399), 0);
            r->noise = 1u << 0;                      /* (OP6) */
            r->note = (uint8_t)dk_rnd(46, 60), r->sweep = (uint8_t)dk_rnd(2, 8), r->sweep_k = DK_SWK[dk_rnd(0, 3)];
            break;
        }
        case 3:                                      /* clap: noise around two tones, in a burst */
            dk_voice(p, 5, 0, l);
            a = dk_rnd(62, 70);
            dk_op(p, 3, a, 0, 99, dk_rnd(80, 92), 1, dk_rnd(295, 320), 3);
            dk_op(p, 4, a, 0, 99, 99, 1, dk_rnd(330, 360), 0);
            dk_op(p, 5, a, 0, 99, dk_rnd(80, 92), 1, dk_rnd(310, 340), 3);
            dk_op(p, 6, a, 0, 99, 99, 1, dk_rnd(340, 380), 0);
            r->noise = 1u << 2 | 1u << 0;            /* (OP4, OP6) */
            r->note = 60, r->burst = (uint8_t)dk_rnd(3, 4), r->burst_n = (uint16_t)(dk_rnd(8, 13) * 441 / 10);
            break;
        case 4: case 5: case 6: case 11: case 12: {  /* hats and cymbals: three metal pairs, noise on the third */
            int hat = l <= 6u;
            a = l == 4u ? dk_rnd(76, 84) : l == 5u ? dk_rnd(56, 64) : l == 6u ? dk_rnd(84, 90) : 0;
            dk_voice(p, 5, 0, l);
            if (hat) {
                dk_op(p, 1, a, 0, 99, dk_rnd(80, 92), 1, (int32_t)hf[0], 3);
                dk_op(p, 2, a, 0, 99, dk_rnd(85, 99), 1, (int32_t)hf[3], 0);
                dk_op(p, 3, a, 0, 99, dk_rnd(80, 92), 1, (int32_t)hf[1], 3);
                dk_op(p, 4, a, 0, 99, dk_rnd(85, 99), 1, (int32_t)hf[4], 0);
                dk_op(p, 5, a, 0, 99, dk_rnd(80, 92), 1, (int32_t)hf[2], 3);
                dk_op(p, 6, a, 0, 99, 99, 1, (int32_t)hf[5], 0);
                r->choke = 1, r->pan = (int8_t)dk_rnd(-12, 12);
            } else {
                b = l == 11u ? dk_rnd(44, 50) : dk_rnd(54, 60);
                dk_op(p, 1, 70, 80, b, dk_rnd(80, 90), 1, dk_rnd(355, 395), 2);
                dk_op(p, 2, 70, 80, b, dk_rnd(80, 95), 1, dk_rnd(370, 399), 0);
                dk_op(p, 3, 70, 80, b, dk_rnd(72, 86), 1, dk_rnd(355, 395), 2);
                dk_op(p, 4, 70, 80, b, dk_rnd(80, 95), 1, dk_rnd(370, 399), 0);
                dk_op(p, 5, 70, 80, b, dk_rnd(72, 86), 1, dk_rnd(355, 395), 2);
                dk_op(p, 6, 70, 80, b, l == 11u ? 99 : dk_rnd(60, 80), 1, dk_rnd(385, 399), 0);
                r->pan = (int8_t)dk_rnd(-16, 16);
            }
            r->noise = 1u << 0;
            r->note = 60;
            break;
        }
        case 7:                                      /* rim: two fixed tones, very short */
            dk_voice(p, 1, 0, l);
            dk_op(p, 1, dk_rnd(86, 93), 0, 99, dk_rnd(85, 95), 1, dk_rnd(300, 345), 3);
            dk_op(p, 2, 88, 0, 99, dk_rnd(50, 80), 1, dk_rnd(250, 300), 3);
            r->note = 60;
            break;
        case 13:                                     /* shaker: noise with a soft attack */
            dk_voice(p, 32, 0, l);
            dk_op(p, 6, dk_rnd(64, 74), 0, 99, 99, 1, dk_rnd(375, 399), 3);
            p[0] = (uint8_t)dk_rnd(78, 88);          /* (OP6's attack) */
            r->noise = 1u << 0, r->note = 60, r->pan = (int8_t)dk_rnd(-20, 20);
            break;
        default:                                     /* cowbell: two tones a fifth-ish apart */
            dk_voice(p, 5, 0, l);
            a = dk_rnd(265, 285), b = dk_rnd(50, 60);
            dk_op(p, 1, 88, 70, b, 92, 1, a, 3);
            dk_op(p, 2, 88, 70, b, 70, 1, a, 0);
            dk_op(p, 3, 88, 70, b, 90, 1, a + dk_rnd(15, 20), 3);
            dk_op(p, 4, 88, 70, b, 68, 1, a + dk_rnd(15, 20), 0);
            r->note = 60;
            break;
        }
        dx_sanitize(p);
    }
}
static void dk_seed(uint32_t seed, uint32_t *hf)
{
    uint32_t l;
    dk_s = (seed & 0xFFFFu) * 2654435761u | 1u;
    for (l = 0; l < 6u; l++)
        hf[l] = (uint32_t)dk_rnd(l < 3u ? 368 : 375, 399);   /* the hats share their metal */
}
static void dice_kit(uint32_t seed)              /* MY KIT := the dice kit of seed */
{
    uint32_t l, hf[6];
    dk_seed(seed, hf);
    for (l = 0; l < DRUM_LANES; l++)
        dk_lane(l, hf);
    ukit.seed = seed & 0xFFFFu;
    ukit_ok = 1;
    memset(dext.m, 0, sizeof dext.m);
}
/* one lane rolled, the rest of the kit kept: a factory kit becomes MY KIT first (its macros baked in, so it
 * sounds as before), then lane l is new and its macros 0. MY KIT is no dice kit of one seed any more */
static void dice_lane(uint32_t kit, uint32_t l, uint32_t seed)
{
    uint32_t hf[6];
    if (l >= DRUM_LANES)
        return;
    if (kit != KIT_USER)
        ukit_bake(kit);
    dk_seed(seed * 31u + l, hf);
    dk_lane(l, hf);
    memset(dext.m[l], 0, sizeof dext.m[l]);
    ukit.seed = 0;
    ukit_ok = 1;
}

static void drum_start(uint32_t i)               /* (re)start voice i: a hit, or the next hit of a burst */
{
    const dx_drum_t *dd = drum_dd(drums.kit[i], drums.drum[i]);
    int32_t note = dd->note + drums.tune[i];
    if (drums.drum[i] == DX_NDRUM - 1u && drums.v[i].note == 77u)
        note += 7;                                   /* the click's accent: a fifth up */
    dx_init(&drums.dx[i], drums.patch[i], clamp(note, 0, 127), drums.v[i].vel);
    drums.dx[i].noise = dd->noise;                   /* the noise operators (dx7_core.c dx_op_noise) */
    drums.dx[i].nseed = 0x9E3779B9u ^ (drums.v[i].age * 2654435761u);
    if (!drums.dx[i].nseed)
        drums.dx[i].nseed = 1u;
    drums.sweep[i] = drums.swd[i] * (int32_t)((1 << 24) / 12);
}

static void drum_on(uint32_t note, uint32_t vel)
{
    uint32_t i, d, lane = lane_of_note(note), kit = drum_kit(), choke;
    int32_t tune = 0, decay = 0, m, k;
    voice_t *v = 0;
    if (note == 76u || note == 77u) {                /* the click (seq.c click_tick): the clave */
        d = DX_NDRUM - 1u;
    } else {
        d = lane;
        drums.hits |= (uint16_t)(1u << lane);       /* the pads and key LEDs */
    }
    if (lane <= 1u && d == lane)
        drums.kick = 1;                             /* (DUCK) */
    choke = drum_dd(kit, d)->choke;
    if (d < DRUM_LANES) {                           /* the lane's macros, and the step's lock of this lane */
        m = dm_get(d, DM_CHOKE);
        choke = m == 0 ? choke : m == 1 ? 0u : (uint32_t)m - 1u;
        tune = dm_get(d, DM_TUNE);
        decay = dm_get(d, DM_DECAY);
        if (drum_lock && dlock_lane(drum_lock) == d) {
            tune += dlock_tune(drum_lock);
            decay = clamp(decay + dlock_decay(drum_lock), -40, 40);
        }
    }
    if (choke)                                      /* the others of its choke group stop (declicked) */
        for (i = 0; i < NDRUM; i++)
            if (drums.v[i].active && drums.drum[i] != d && drums.choke[i] == choke) {
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
    drums.kit[i] = (uint8_t)kit;
    drums.burst[i] = drum_dd(kit, d)->burst > 1u ? (uint8_t)(drum_dd(kit, d)->burst - 1u) : 0;
    drums.t[i] = 0;
    drums.next[i] = drum_dd(kit, d)->burst_n;
    drums.quiet[i] = 0;
    {
        const dx_drum_t *dd = drum_dd(kit, d);
        int32_t sw = dm_get(d, DM_SWEEP), pan = clamp(dd->pan + dm_get(d, DM_PAN), -64, 63);
        int32_t rev = clamp(dd->rev + dm_get(d, DM_REV), 0, 127);
        uint32_t kk = dd->sweep_k ? dd->sweep_k : 64000u;   /* (a lane without a sweep gets one of ~20 ms) */
        drums.gain[i] = (int32_t)((dd->level * 258 * (int32_t)DM_GAIN[dm_get(d, DM_LEVEL) + 40]) >> 12);
        drums.tune[i] = (int8_t)clamp(tune, -48, 48);
        drums.choke[i] = (uint8_t)choke;
        k = clamp((int32_t)dd->sweep * (40 + sw) / 40 + (sw > 0 ? sw / 4 : 0), 0, 60);   /* SWEEP: deeper and longer */
        drums.swd[i] = (uint8_t)k;
        drums.swk[i] = (uint16_t)(65536u - (65536u - kk) * 80u / (uint32_t)(80 + sw));
        drums.pgl[i] = (int16_t)(4096 - (pan > 0 ? pan * 64 : 0));
        drums.pgr[i] = (int16_t)(4096 + (pan < 0 ? pan * 64 : 0));
        drums.send[i] = (int16_t)(rev * 256);
    }
    drum_voice(d, kit, drums.patch[i], tune, decay);
    drum_start(i);
}

/* the drum bus (sloopDX): lane gain -> sum (stereo: the lane's pan) -> DRIVE (the drum track's P_DIST: a soft
 * clip, 1x .. 9x into it, a tone low-pass that closes with it) -> COMP (its P_CHOR: a peak follower, ~0.2 ms up,
 * ~45 ms down, 4:1 above a threshold that falls with COMP (0 .. -23 dB), make-up up to +5.4 dB; one gain for
 * both sides and for the reverb send) -> the reverb send (each lane's REV, summed before: sb). Off at 0 */
static void drums_bus(int32_t *l, int32_t *r, int32_t *sb, uint32_t n)
{
    int32_t d = TDRUM->p[P_DIST], c = TDRUM->p[P_CHOR], i, ch;
    if (d) {                                        /* (32-bit products throughout: the target has no fast 64-bit) */
        int32_t g = 4096 + d * d * 2, k = 32767 - d * 120, mk = 32767 - d * 90;
        for (ch = 0; ch < 2; ch++) {
            int32_t *b = ch ? r : l;
            for (i = 0; i < (int32_t)n; i++) {
                int32_t x = clamp(b[i], -262143, 262143);
                int32_t y = softclip(((x >> 4) * g) >> 10);   /* small signals: x * g / 8192, +-32767 */
                drums.drv_lp[ch] += mulq15(y - drums.drv_lp[ch], k);   /* (|y - lp| <= 65534: fits) */
                b[i] = mulq15(drums.drv_lp[ch], mk) * 2;      /* back to x * g / 4096 */
            }
        }
    } else {
        drums.drv_lp[0] = drums.drv_lp[1] = 0;
    }
    if (c) {
        int32_t t = 30000 - c * 220, env = drums.cmp_env, g0 = drums.cmp_g ? drums.cmp_g : 32767, g1, mk = 4096 + c * 28;
        for (i = 0; i < (int32_t)n; i++) {
            int32_t a = l[i] < 0 ? -l[i] : l[i], b = r[i] < 0 ? -r[i] : r[i];
            a = a > b ? a : b;
            env += a > env ? (a - env) >> 3 : -(env >> 11);
        }
        env = clamp(env, 0, 262143);
        g1 = env > t ? (((t + (env - t) / 4) >> 3) << 15) / (env >> 3) : 32767;
        if (g1 > 32767)
            g1 = 32767;
        for (i = 0; i < (int32_t)n; i++) {
            int32_t g = g0 + (((g1 - g0) * i) >> CTL_LOG2);
            l[i] = (mulq15(clamp(l[i], -262143, 262143) >> 3, g) * mk) >> 9;   /* (2^15 x 2^15; x 8, Q12) */
            r[i] = (mulq15(clamp(r[i], -262143, 262143) >> 3, g) * mk) >> 9;
            sb[i] = (mulq15(clamp(sb[i], -262143, 262143) >> 3, g) * mk) >> 9;
        }
        drums.cmp_env = env;
        drums.cmp_g = g1;
    } else {
        drums.cmp_env = 0;
        drums.cmp_g = 32767;
    }
}

/* adds the drums into the dry mix and the reverb send; mono != 0: into mono instead, before the
 * pan and the send (the SLICER, slicer.c slicer_drums, does those after it). The voices go into the bus
 * first (drums_bus) */
static inline void drums_mix(int32_t *ml, int32_t *mr, int32_t *rev, int32_t *mono, uint32_t n)
{
    uint32_t k, i, any = 0;
    int32_t lvl = song.g[G_DRLVL] * 200, send = song.g[G_DRREV] * 258, pk = drums.peak;   /* (200: -0 dB at LVL 100;
                                                                                           * 1.9 had 142, -3 dB) */
    int32_t pan = trk[TRK_DRUM].p[P_PAN], gl = 4096 - (pan > 0 ? pan * 64 : 0), gr = 4096 + (pan < 0 ? pan * 64 : 0);
    int32_t buf[DX_N], bl[DX_N], br[DX_N], bs[DX_N];
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
    for (i = 0; i < DX_N; i++)
        bl[i] = br[i] = bs[i] = 0;
    for (k = 0; k < NDRUM; k++) {
        voice_t *v = &drums.v[k];
        int32_t rp = 0, pl = drums.pgl[k], pr = drums.pgr[k], sd = drums.send[k];
        if (!v->active)
            continue;
        any = 1;
        if (drums.burst[k] && drums.t[k] >= drums.next[k]) {   /* the next hit of a burst */
            drums.burst[k]--;
            drums.next[k] += drum_dd(drums.kit[k], drums.drum[k])->burst_n;
            drum_start(k);
        }
        for (i = 0; i < DX_N; i++)
            buf[i] = 0;
        dx_compute(&drums.dx[k], buf, drums.sweep[k]);
        if (drums.sweep[k])
            drums.sweep[k] = mulq16(drums.sweep[k], drums.swk[k]);
        for (i = 0; i < DX_N; i++) {
            int32_t s = clamp(buf[i] >> 11, -65535, 65535);   /* one carrier at full level: 16384 (as the synth parts) */
            rp = s > rp ? s : -s > rp ? -s : rp;
            s = mulq15(mulq15(s, drums.gain[k]),
                       mulq15(lvl, 32767 - drums.a0 - (((drums.a1 - drums.a0) * (int32_t)i) >> CTL_LOG2)));
            v->s[7] = s;
            bl[i] += (s * pl) >> 12;
            br[i] += (s * pr) >> 12;
            bs[i] += (s * sd) >> 14;
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
    if (any)
        drums.bus_tail = 64;                        /* ~45 ms: the tone filters and the compressor settle */
    else if (!drums.bus_tail || !--drums.bus_tail)
        return;
    drums_bus(bl, br, bs, DX_N);
    for (i = 0; i < DX_N; i++) {
        int32_t l = clamp(bl[i], -262143, 262143), r = clamp(br[i], -262143, 262143);   /* (x gl fits 32 bits) */
        int32_t a = l < 0 ? -l : l, b = r < 0 ? -r : r;
        a = a > b ? a : b;
        if (a > pk)
            pk = a;
        if (mono) {
            mono[i] += (l + r) >> 1;
            continue;
        }
        ml[i] += (l * gl) >> 12;
        mr[i] += (r * gr) >> 12;
        if (send)
            rev[i] += mulq15(clamp(bs[i], -262143, 262143), send);
    }
    drums.peak = pk;
}
static void drums_render(int32_t *ml, int32_t *mr, int32_t *rev, uint32_t n) { drums_mix(ml, mr, rev, 0, n); }
static void drums_render_mono(int32_t *mono, uint32_t n) { drums_mix(0, 0, 0, mono, n); }
