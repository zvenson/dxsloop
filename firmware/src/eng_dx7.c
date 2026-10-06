/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sven Trogus */
/* DX7: the six-operator FM of the DX7 (dx7_core.c, a port of Dexed's msfa) as a SLOOP engine.
 * EDIT 1: VOICE (one of the factory voices, dx7_bank.h), BRIGHT (the modulators' output levels),
 * ATTACK (the carriers' first rate), DECAY (every operator's second and third rate); EDIT 2: RELEASE
 * (the carriers' fourth rate), FDBK (feedback). They change the voice for the notes that follow, as
 * the panel of a DX7 does. The voice brings its own envelopes, so the part's ADSR only keeps the note
 * open; the note ends when its carrier envelopes have (the amp hook). The voice's own velocity
 * sensitivity sets the level (the part's velocity scaling is taken out again in the render). */
#include "dx7_core.c"
#include "dx7_bank.h"

static dxv_t DXV[NPART][NVOICE];

/* the user bank: 32 voices from a DX7 bulk dump (.syx, 32 voices, 4104 bytes), after the factory ones in
 * VOICE. Kept packed (128 bytes a voice, as the DX7 stores them); unpacked at note-on */
#define DX_NUSER 32u
#define DX_NVOICES (DX_NSYNTH + DX_NUSER)
static uint8_t dx_user[DX_NUSER][128];
static uint8_t dx_user_ok;
static uint32_t dx_edit_gen;                 /* bumped on every change of the bank: sounding notes follow */
static char dx_user_name[DX_NUSER][11] = {"U01", "U02", "U03", "U04", "U05", "U06", "U07", "U08", "U09", "U10", "U11", "U12", "U13", "U14", "U15", "U16", "U17", "U18", "U19", "U20", "U21", "U22", "U23", "U24", "U25", "U26", "U27", "U28", "U29", "U30", "U31", "U32"};
static const char *dx_names[DX_NVOICES] = {DX_SYNTH_NAME_LIST, dx_user_name[0], dx_user_name[1], dx_user_name[2], dx_user_name[3], dx_user_name[4], dx_user_name[5], dx_user_name[6], dx_user_name[7], dx_user_name[8], dx_user_name[9], dx_user_name[10], dx_user_name[11], dx_user_name[12], dx_user_name[13], dx_user_name[14], dx_user_name[15], dx_user_name[16], dx_user_name[17], dx_user_name[18], dx_user_name[19], dx_user_name[20], dx_user_name[21], dx_user_name[22], dx_user_name[23], dx_user_name[24], dx_user_name[25], dx_user_name[26], dx_user_name[27], dx_user_name[28], dx_user_name[29], dx_user_name[30], dx_user_name[31]};

static uint8_t dx_bank_busy;                  /* an upload is on (BANK_BEGIN .. BANK_END) */

/* the voice names U01..U32: from the bank's 10-character names, or the slot labels without a bank */
static void dx_bank_names(void)
{
    uint32_t k, i, j;
    dx_edit_gen++;
    for (k = 0; k < DX_NUSER; k++) {
        if (!dx_user_ok) {
            dx_user_name[k][0] = 'U';
            dx_user_name[k][1] = (char)('0' + (k + 1u) / 10u);
            dx_user_name[k][2] = (char)('0' + (k + 1u) % 10u);
            dx_user_name[k][3] = 0;
            continue;
        }
        for (i = 0; i < 10u; i++) {
            char c = (char)dx_user[k][118 + i];
            dx_user_name[k][i] = c >= 32 && c < 127 ? c : ' ';
        }
        for (j = 10; j > 0 && dx_user_name[k][j - 1] == ' '; j--)
            ;
        dx_user_name[k][j] = 0;
    }
}
static uint32_t dx_bank_sum(void)            /* the dump's checksum byte of what is in dx_user */
{
    uint32_t k, i, sum = 0;
    for (k = 0; k < DX_NUSER; k++)
        for (i = 0; i < 128u; i++)
            sum += dx_user[k][i];
    return (128u - (sum & 127u)) & 127u;
}
static void dx_bank_clear(void)              /* no bank: U01..U32 play INIT VOICE, the USER kit the DX KIT */
{
    dx_user_ok = 0;
    dx_bank_busy = 0;
    memset(dx_user, 0, sizeof dx_user);
    dx_bank_names();
}
/* the bank in pieces (editor cmds BANK_BEGIN / WRITE / END, editor.c; tools/fm1_bank_upload.py): the voice
 * bytes of the dump (bytes 6..4101 of the .syx) land in dx_user as they come, then the checksum decides */
static void dx_bank_begin(void) { dx_bank_clear(); dx_bank_busy = 1; }
static int dx_bank_write(uint32_t off, const uint8_t *d, uint32_t n)   /* 0 ok, 1 arguments */
{
    uint32_t i;
    if (!dx_bank_busy || !n || off + n > sizeof dx_user)
        return 1;
    for (i = 0; i < n; i++)
        dx_user[(off + i) / 128u][(off + i) % 128u] = d[i] & 127u;
    return 0;
}
static int dx_bank_end(uint32_t checksum)    /* 0 ok (the bank plays), 1 checksum (no bank) */
{
    if (!dx_bank_busy || dx_bank_sum() != (checksum & 127u)) {
        dx_bank_clear();
        return 1;
    }
    dx_bank_busy = 0;
    dx_user_ok = 1;
    dx_bank_names();
    return 0;
}

static void dx_unpack(const uint8_t *b, uint8_t *u);
static void dx_sanitize(uint8_t *u);
/* an unpacked voice (156) -> the DX7's packed 128 bytes (the inverse of dx_unpack) */
static void dx_pack(const uint8_t *u, uint8_t *b)
{
    uint32_t op, i;
    for (op = 0; op < 6u; op++) {
        const uint8_t *o = u + op * 21u;
        uint8_t *q = b + op * 17u;
        for (i = 0; i < 11u; i++)
            q[i] = o[i];
        q[11] = (uint8_t)(o[11] | o[12] << 2);
        q[12] = (uint8_t)(o[13] | o[20] << 3);
        q[13] = (uint8_t)(o[14] | o[15] << 2);
        q[14] = o[16];
        q[15] = (uint8_t)(o[17] | o[18] << 1);
        q[16] = o[19];
    }
    for (i = 0; i < 8u; i++)
        b[102 + i] = u[126 + i];
    b[110] = u[134];
    b[111] = (uint8_t)(u[135] | u[136] << 3);
    b[112] = u[137], b[113] = u[138], b[114] = u[139], b[115] = u[140];
    b[116] = (uint8_t)(u[141] | u[142] << 1 | u[143] << 4);
    b[117] = u[144];
    for (i = 0; i < 10u; i++)
        b[118 + i] = u[145 + i];
}

/* ---- voice editing (the device's EDIT pages and the editor's voice page): a user slot is the edit
 * buffer, as on a DX7; dx_bank_store() (project.c) is STORE. Without a bank, the first edit makes one:
 * every slot INIT VOICE. Factory voices are read-only; copy one into a slot (dx_user_put) to edit it. */
#define DX_NPARAM 155u                               /* the unpacked voice: 6 x 21 operator bytes, 29 global */
static const uint8_t DX_PMAX[29] = {99, 99, 99, 99, 99, 99, 99, 99, 31, 7, 1, 99, 99, 99, 99, 1, 5, 7, 48,
                                    127, 127, 127, 127, 127, 127, 127, 127, 127, 127};   /* 126..154: as dx_sanitize + name */
static uint32_t dx_param_max(uint32_t idx)
{
    static const uint8_t OPMAX[21] = {99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 3, 3, 7, 3, 7, 99, 1, 31, 99, 14};
    return idx < 126u ? OPMAX[idx % 21u] : idx < DX_NPARAM ? DX_PMAX[idx - 126u] : 0u;
}
static void dx_bank_init_all(void)                   /* no bank -> a bank of INIT VOICE */
{
    uint32_t k;
    for (k = 0; k < DX_NUSER; k++)
        dx_pack(DX_SYNTH[DX_NSYNTH - 1], dx_user[k]);
    dx_user_ok = 1;
    dx_bank_busy = 0;
    dx_bank_names();
}
/* voice vi of VOICE (0..16 factory, 17..48 the user bank) as the packed 128 bytes */
static void dx_voice_get(uint32_t vi, uint8_t *b)
{
    uint32_t i;
    vi %= DX_NVOICES;
    if (vi < DX_NSYNTH) {
        dx_pack(DX_SYNTH[vi], b);
    } else if (dx_user_ok) {
        for (i = 0; i < 128u; i++)
            b[i] = dx_user[vi - DX_NSYNTH][i];
    } else {
        dx_pack(DX_SYNTH[DX_NSYNTH - 1], b);
    }
}
static void dx_user_put(uint32_t k, const uint8_t *b)   /* packed voice -> slot k (RAM; STORE = dx_bank_store) */
{
    uint32_t i;
    if (k >= DX_NUSER)
        return;
    if (!dx_user_ok)
        dx_bank_init_all();
    for (i = 0; i < 128u; i++)
        dx_user[k][i] = b[i] & 127u;
    dx_bank_names();
}
static uint32_t dx_user_param_get(uint32_t k, uint32_t idx)   /* unpacked parameter idx (0..154) of slot k */
{
    uint8_t u[156];
    if (k >= DX_NUSER || idx >= DX_NPARAM)
        return 0;
    if (!dx_user_ok)
        dx_bank_init_all();
    dx_unpack(dx_user[k], u);
    dx_sanitize(u);
    return u[idx];
}
static void dx_user_param_set(uint32_t k, uint32_t idx, uint32_t v)
{
    uint8_t u[156];
    if (k >= DX_NUSER || idx >= DX_NPARAM)
        return;
    if (!dx_user_ok)
        dx_bank_init_all();
    dx_unpack(dx_user[k], u);
    dx_sanitize(u);
    u[idx] = (uint8_t)(v > dx_param_max(idx) ? dx_param_max(idx) : v);
    dx_pack(u, dx_user[k]);
    dx_edit_gen++;
    if (idx >= 145u)
        dx_bank_names();
}

/* DX7 32-voice bulk dump (4104 bytes) -> the user bank; 0 = not one (header, length or checksum) */
static int dx_bank_load(const uint8_t *s, uint32_t n)
{
    if (n < 4104u || s[0] != 0xF0 || s[1] != 0x43 || (s[2] & 0xF0) != 0x00 || s[3] != 0x09 || s[4] != 0x20 ||
        s[5] != 0x00 || s[4103] != 0xF7)
        return 0;
    dx_bank_begin();
    dx_bank_write(0, s + 6, 4096);
    return dx_bank_end(s[4102]) == 0;
}

/* a packed voice (128 bytes, the bulk format) -> unpacked (156) */
static void dx_unpack(const uint8_t *b, uint8_t *u)
{
    uint32_t op, i;
    for (op = 0; op < 6u; op++) {
        const uint8_t *s = b + op * 17u;
        uint8_t *o = u + op * 21u;
        for (i = 0; i < 11u; i++)
            o[i] = s[i];
        o[11] = s[11] & 3u;
        o[12] = (s[11] >> 2) & 3u;
        o[13] = s[12] & 7u;
        o[20] = (s[12] >> 3) & 15u;
        o[14] = s[13] & 3u;
        o[15] = (s[13] >> 2) & 7u;
        o[16] = s[14];
        o[17] = s[15] & 1u;
        o[18] = (s[15] >> 1) & 31u;
        o[19] = s[16];
    }
    for (i = 0; i < 8u; i++)
        u[126 + i] = b[102 + i];
    u[134] = b[110] & 31u;
    u[135] = b[111] & 7u;
    u[136] = (b[111] >> 3) & 1u;
    for (i = 0; i < 4u; i++)
        u[137 + i] = b[112 + i];
    u[141] = b[116] & 1u;
    u[142] = (b[116] >> 1) & 7u;
    u[143] = (b[116] >> 4) & 7u;
    u[144] = b[117];
    for (i = 0; i < 10u; i++)
        u[145 + i] = b[118 + i];
    u[155] = 0;
}

/* every value inside its DX7 range (a damaged dump must not index past a table) */
static void dx_sanitize(uint8_t *u)
{
    static const uint8_t OPMAX[21] = {99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 3, 3, 7, 3, 7, 99, 1, 31, 99, 14};
    static const uint8_t GMAX[19] = {99, 99, 99, 99, 99, 99, 99, 99, 31, 7, 1, 99, 99, 99, 99, 1, 5, 7, 48};
    uint32_t op, i;
    for (op = 0; op < 6u; op++)
        for (i = 0; i < 21u; i++)
            if (u[op * 21u + i] > OPMAX[i])
                u[op * 21u + i] = OPMAX[i];
    for (i = 0; i < 19u; i++)
        if (u[126 + i] > GMAX[i])
            u[126 + i] = GMAX[i];
}
static uint8_t dx_note0[NPART][NVOICE], dx_rel[NPART][NVOICE];
/* operators switched off per part (voice edit, ui_voice.c): bit d = OP(d+1). Not part of the voice, not
 * stored: as on Baud Girl's FM-1+VA they come back with another voice */
static uint8_t dx_opmute[NPART], dx_opmute_v[NPART];   /* the switches, and the VOICE they were set on */
static int32_t dx_dc_x[NPART][NVOICE], dx_dc_y[NPART][NVOICE];   /* DC blocker state */

static dxv_t *dx_of(const track_t *t, const voice_t *v, uint32_t *pi, uint32_t *vi)
{
    uint32_t p = (uint32_t)(t - trk), i = (uint32_t)(v - t->v);
    if (p >= NPART || i >= NVOICE)
        return 0;
    *pi = p;
    *vi = i;
    return &DXV[p][i];
}

static inline int dx_clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

/* voice vi with the part's knobs applied: the quick knobs (EDIT: BRITE ATK DEC REL FDBK, -40..40, 0 = the
 * voice as programmed), the ENV page (ATK DEC REL 0 = as programmed, more = slower / longer; SUS 127 = as
 * programmed, less = a lower sustain on the carriers), the operators switched off in the voice list */
static void dx_voice_build(const track_t *t, uint32_t vi, uint8_t *p)
{
    int bright = t->p[P_E1], atk = t->p[P_E2], dec = t->p[P_E3], rel = t->p[P_E4], fb = t->p[P_E5];
    int eatk = t->p[P_ATK] * 60 / 127, edec = t->p[P_DEC] * 50 / 127, erel = t->p[P_REL] * 60 / 127;
    int sus = dx_clampi(t->p[P_SUS], 0, 127);
    uint32_t i, op, alg, pi = (uint32_t)(t - trk);
    vi %= DX_NVOICES;
    if (vi < DX_NSYNTH) {
        for (i = 0; i < 156u; i++)
            p[i] = DX_SYNTH[vi][i];
    } else if (dx_user_ok) {
        dx_unpack(dx_user[vi - DX_NSYNTH], p);
        dx_sanitize(p);
    } else {                                      /* an empty user slot: the init voice */
        for (i = 0; i < 156u; i++)
            p[i] = DX_SYNTH[DX_NSYNTH - 1][i];
    }
    alg = p[134] & 31u;
    for (op = 0; op < 6u; op++) {
        uint8_t *o = p + op * 21u;
        int carrier = (DX_ALG[alg][op] & 3) == 0;  /* (writes the output, not a bus) */
        if (!carrier && o[16])
            o[16] = (uint8_t)dx_clampi(o[16] + bright, 0, 99);
        if (carrier) {
            o[0] = (uint8_t)dx_clampi(o[0] - atk - eatk, 1, 99);
            o[3] = (uint8_t)dx_clampi(o[3] - rel - erel, 1, 99);
            o[6] = (uint8_t)(o[6] * sus / 127);
        }
        o[1] = (uint8_t)dx_clampi(o[1] - dec - edec, 1, 99);
        o[2] = (uint8_t)dx_clampi(o[2] - dec - edec, 1, 99);
    }
    p[135] = (uint8_t)dx_clampi(p[135] + fb, 0, 7);
    if (pi < NPART && dx_opmute[pi] && dx_opmute_v[pi] != vi)
        dx_opmute[pi] = 0;                        /* another voice: every operator on again */
    if (pi < NPART && dx_opmute[pi])
        for (op = 0; op < 6u; op++)               /* op block 0 = OP6 */
            if ((dx_opmute[pi] >> (5u - op)) & 1u)
                p[op * 21u + 16u] = 0;
}
static void dx_voice_for(const track_t *t, uint8_t *p) { dx_voice_build(t, (uint32_t)t->p[P_E0], p); }

/* what a sounding note follows: the knobs, the operator switches and the bank (not VOICE: a new voice is
 * for the next note, as on a DX7) */
static uint32_t dx_part_sig(const track_t *t)
{
    uint32_t pi = (uint32_t)(t - trk), h = dx_edit_gen * 2654435761u, i;
    static const uint8_t IDS[9] = {P_E1, P_E2, P_E3, P_E4, P_E5, P_ATK, P_DEC, P_SUS, P_REL};
    for (i = 0; i < 9u; i++)
        h = (h ^ (uint16_t)t->p[IDS[i]]) * 16777619u;
    return pi < NPART ? (h ^ dx_opmute[pi]) * 16777619u : h;
}
static uint8_t dx_vi[NPART][NVOICE];             /* the voice each note was started with */
static uint32_t dx_sig[NPART][NVOICE];

static void dx7_note_on(track_t *t, voice_t *v)
{
    uint32_t pi, vi, k;
    dxv_t *d = dx_of(t, v, &pi, &vi);
    uint8_t patch[156];
    int32_t ph[6], g[6];
    int keep;
    if (!d)
        return;
    /* a retrigger of a sounding note: the operators go on from where they are (no click); a voice that was
     * fading out for another part starts clean (its old levels under a rising gain would jump) */
    keep = d->on && dx_playing(d) && v->env_out > 8192;
    for (k = 0; k < 6u; k++) {
        ph[k] = d->phase[k];
        g[k] = d->gain[k];
    }
    dx_vi[pi][vi] = (uint8_t)((uint32_t)t->p[P_E0] % DX_NVOICES);
    dx_voice_build(t, dx_vi[pi][vi], patch);
    dx_init(d, patch, v->note, v->vel ? v->vel : 1);
    dx_sig[pi][vi] = dx_part_sig(t);
    if (keep)
        for (k = 0; k < 6u; k++) {
            d->phase[k] = ph[k];
            d->gain[k] = g[k];
        }
    dx_note0[pi][vi] = v->note;
    dx_rel[pi][vi] = 0;
    if (!keep)
        dx_dc_x[pi][vi] = dx_dc_y[pi][vi] = 0;
}

/* the part's envelope as a gate: open while held, the release left to the voice's own envelopes */
static int32_t dx7_amp(track_t *t, voice_t *v, int32_t adsr)
{
    uint32_t pi, vi;
    dxv_t *d = dx_of(t, v, &pi, &vi);
    if (!d || v->stage == 4u)                     /* given up for another part: the part's fade */
        return adsr;
    if (v->stage == 3u || !v->gate) {
        if (!dx_rel[pi][vi]) {
            dx_keyup(d);
            dx_rel[pi][vi] = 1;
        }
        if (dx_playing(d)) {
            v->env = 1 << 24;                     /* keeps the voice (env_tick frees it below 1 << 12) */
            return 32767;
        }
        v->env = 0;                               /* the carriers are done: freed on the next tick */
        d->on = 0;
        return 0;
    }
    return 32767;
}

/* a sounding note follows the part's knobs and the bank (rare: kept out of the render loop) */
__attribute__((noinline)) static void dx_follow(const track_t *t, dxv_t *d, uint32_t pi, uint32_t vi, int32_t vel)
{
    uint32_t sig = dx_part_sig(t);
    uint8_t patch[156];
    if (sig == dx_sig[pi][vi])
        return;
    dx_sig[pi][vi] = sig;
    dx_voice_build(t, dx_vi[pi][vi], patch);
    dx_update(d, patch, dx_note0[pi][vi], vel);
}

static void dx7_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t pi, vi, i;
    dxv_t *d = dx_of(t, v, &pi, &vi);
    int32_t buf[DX_N], a0, a1, vel = v->vel ? v->vel : 1;
    /* the part's pitch (glide, LFO, tuning, pitch envelope) against the note the voice was started on;
     * 1/16 semitone -> Q24 log2 */
    int32_t pitch = ((m->pitch16 - (int32_t)dx_note0[pi][vi] * 16) * 349525) >> 2;
    if (!d || n != DX_N)
        return;
    for (i = 0; i < DX_N; i++)
        buf[i] = 0;
    if (d->on)                                    /* a knob, the voice list or the editor changed the voice */
        dx_follow(t, d, pi, vi, vel);
    /* ENV / LFO -> FLT and SHP (no filter, no shape here): the modulators' level, +-2 octaves of gain. The
     * cutoff only when a FLT destination is set: voice.c also opens it on accents, which a DX7 voice does with
     * its own velocity sensitivity */
    d->bright = ((m->shape - (64 << 8)) + (t->p[P_LD_FLT] || t->p[P_ED_FLT] ? m->cutoff : 0)) * 2048;
    dx_compute(d, buf, pitch);
    a0 = clamp(m->amp0 * 127 / vel, 0, 32767);    /* the voice's own velocity curve, not the part's */
    a1 = clamp(m->amp1 * 127 / vel, 0, 32767);
    for (i = 0; i < DX_N; i++) {
        int32_t x = (a1 - a0) * (int32_t)i;
        int32_t a = a0 + ((x + ((x >> 31) & (CTL - 1))) >> CTL_LOG2);
        int32_t s = clamp(buf[i] >> 11, -65535, 65535);   /* one carrier at full level: 16384 (one voice near
                                                       * -6 dBFS, as SLOOP's; the presets' trims level them) */
        int32_t y = s - dx_dc_x[pi][vi] + mulq15(dx_dc_y[pi][vi], 32610);   /* DC blocker, ~10 Hz: FM with */
        dx_dc_x[pi][vi] = s;                      /* feedback is not symmetric (a DX7 has the same offset) */
        dx_dc_y[pi][vi] = y = clamp(y, -131071, 131071);
        out[i] += mulq15(mulq15(clamp(y, -65535, 65535), a), VOICE_FS);
    }
}

static const preset_t DX7_PRESETS[] = {
    /* VOICE BRIGHT ATTACK DECAY RELEASE FDBK - - ; ADSR: a gate (the voice has its own envelopes) */
    {"EPIANO 1", {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 0, FX(0, 30, 7, 15)},
    {"EPIANO 2", {1, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 0, FX(0, 40, 9, 18)},
    {"FM BASS", {2, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 1, FX(0, 0, 4, 6)},
    {"SLAP BASS", {3, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 1, FX(0, 0, 4, 6)},
    {"SUB BASS", {4, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 1, FX(0, 0, 0, 3)},
    {"BRASS", {5, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 0, FX(0, 20, 9, 21)},
    {"STRINGS", {6, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 0, FX(0, 50, 12, 33)},
    {"GLASS PAD", {7, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 0, FX(0, 50, 15, 36)},
    {"BELLS", {8, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 0, FX(0, 20, 18, 33)},
    {"MARIMBA", {9, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 0, FX(0, 0, 12, 18)},
    {"ORGAN", {10, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 0, FX(10, 40, 0, 18)},
    {"CLAV", {11, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 0, FX(0, 20, 9, 12)},
    {"PLUCK", {12, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 0, FX(0, 20, 18, 21)},
    {"FLUTE", {13, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 1, FX(0, 20, 12, 27)},
    {"SAW LEAD", {14, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 1, FX(0, 20, 18, 18)},
    {"KOTO", {15, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 0, FX(0, 10, 15, 24)},
    {"INIT VOICE", {16, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 0, FX(0, 0, 0, 0)},
};

static const engine_t ENG_DX7 = {
    "DX7", {"VOICE", "SHAPE"},
    {
        {"VOICE", F_ENUM, 0, DX_NVOICES - 1, 0, dx_names, 0},
        {"BRITE", F_INT, -40, 40, 0, 0, 0},
        {"ATK", F_INT, -40, 40, 0, 0, 0},
        {"DEC", F_INT, -40, 40, 0, 0, 0},
        {"REL", F_INT, -40, 40, 0, 0, 0},
        {"FDBK", F_INT, -7, 7, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
    },
    DX7_PRESETS, sizeof(DX7_PRESETS) / sizeof(DX7_PRESETS[0]), -1, dx7_note_on, dx7_render,
    0x05DF, {P_E1, P_E2, P_E4, P_E5}, 0, dx7_amp,
};
