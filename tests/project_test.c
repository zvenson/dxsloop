/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the project formats (firmware/src/project.c, -DPROJ_HOST part). Format 4 ("FUN4",
 * SLOOP 2.0: 10-byte steps with levels and ratchets, the drum track's 16 lanes, P_CHORD) is written;
 * format 3 ("FUN3", SLOOP 1.x), format 2 ("FUN2", 53 parameters per track) and format 1 ("FUN1"), built
 * byte for byte as the firmware stored them, convert: every old value at its parameter, the parameters
 * added since at their defaults, the swings onto the MPC scale (x 0.8), synth steps as they were, the
 * drum track's notes onto its lanes (accent: hard), globals, selection, the engine bytes (kept; the
 * drum track's 0); damaged ones are refused. Run by tests/run_tests.sh (needs build/gen). */
#define main hostsim_main
#include "hostsim.c"
#undef main
#define PROJ_HOST 1
static uint32_t trk_def_engine(uint32_t i)       /* ui.c TRK_DEF: the DX7 on every part */
{
    static const uint8_t E[NPART] = {0, 0, 0};
    return i < NPART ? E[i] : 0u;
}
#include "../firmware/src/project.c"

static int check(const char *what, int ok)
{
    printf("%-66s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* the value parameter k (old id) of track t had in the old project */
static int16_t oldv(uint32_t t, uint32_t k) { return (int16_t)(t * 100u + k * 3u + 1u); }

static const uint8_t OLD_ENG[NTRK] = {7, 0, 6, 8};   /* WHEEL, ANALOG, TRIO; the drum track: 8 (none) */
static void fill_old_steps(step8_t *st, uint32_t t)
{
    uint32_t k;
    for (k = 0; k < NSTEP; k++) {
        step8_t *s = &st[k];
        s->note[0] = (uint8_t)(36u + (k + t) % 40u);
        s->note[1] = (uint8_t)(38u + k % 5u);
        s->n = (uint8_t)(k % 3u);
        s->time = (uint8_t)(k % 3u);
        s->flags = (uint8_t)(k & 3u);
        s->vel = (uint8_t)(64u + t);
    }
}
static void fill_v2_track(proj_trk_v2_t *d, uint32_t t)
{
    uint32_t k;
    for (k = 0; k < PROJ_NP_V2; k++)
        d->p[k] = oldv(t, k);
    d->engine = OLD_ENG[t];
    d->preset = (uint8_t)(t + 5u);
    fill_old_steps(d->step, t);
}
static void fill_v3_track(proj_trk_v3_t *d, uint32_t t)
{
    uint32_t k;
    for (k = 0; k < PROJ_NP_V3; k++)
        d->p[k] = oldv(t, k);
    d->p[P_SSWING] = 50;                           /* (swings: within 0..100) */
    d->p[P_ASWING] = 100;
    d->engine = OLD_ENG[t];
    d->preset = (uint8_t)(t + 5u);
    fill_old_steps(d->step, t);
}

/* the steps of a converted track against the old ones: synth as they were, drums onto lanes */
static int steps_ok(const step_t *n, const step8_t *o, int drum)   /* (n: the track's steps in the pool) */
{
    uint32_t k, i;
    for (k = 0; k < PROJ_NSTEP_OLD; k++) {
        if (drum) {
            const dstep_t *d = &((const dstep_t *)n)[k];
            uint32_t want = 0;
            if (o[k].time == ST_NOTE)
                for (i = 0; i < o[k].n; i++)
                    want |= 1u << lane_of_note(o[k].note[i]);
            if (dstep_mask(d) != want)
                return 0;
            for (i = 0; i < DRUM_LANES; i++)
                if ((want >> i) & 1u && dstep_lvl(d, i) != ((o[k].flags & SF_ACCENT) ? LV_HARD : vel_lvl(o[k].vel)))
                    return 0;
        } else {
            const step_t *s = &n[k];
            if (memcmp(s->note, o[k].note, 4) || s->n != o[k].n || s->time != o[k].time || s->flags != o[k].flags ||
                s->vel != o[k].vel || s->lvl || s->rat)
                return 0;
        }
    }
    return 1;
}

/* track t converted from format 2 / 1 has the old values where they belong */
static int track_ok_v2(const project_t *q, const proj_trk_v2_t *o, uint32_t t)
{
    uint32_t k;
    const proj_trk_t *n = &q->t[t];
    int ok = (t == TRK_DRUM ? n->engine == 0 && n->preset == 0 : n->engine == o->engine && n->preset == o->preset) &&
             n->nst == PROJ_NSTEP_OLD && steps_ok(proj_steps_c(q, t), o->step, t == TRK_DRUM);
    for (k = 0; k <= P_DETUNE; k++)
        if (k != P_SSWING && k != P_ASWING)
            ok &= n->p[k] == oldv(t, k);
    ok &= n->p[P_SLCR] == 0 && n->p[P_SLPAT] == TP[P_SLPAT].def && n->p[P_SLRATE] == TP[P_SLRATE].def &&
          n->p[P_SLDEPTH] == TP[P_SLDEPTH].def && n->p[P_CHORD] == 0;
    for (k = 0; k < 8u; k++)
        ok &= n->p[P_E0 + k] == oldv(t, 45u + k);
    return ok;
}

/* today's parameters -> a format 4..6 track's (the inverse of proj_p_from_v6, for building old projects) */
static void p_to_v6(int16_t *d, const int16_t *s)
{
    uint32_t k;
    for (k = 0; k < PROJ_NP_V6 - 8u; k++)
        d[k] = s[k];
    for (k = 0; k < 8u; k++)
        d[PROJ_NP_V6 - 8u + k] = s[P_E0 + k];
}

int main(void)
{
    static project_v3_t v3;
    static project_v2_t v2;
    static project_v1_t v1;
    static project_t q, q2;
    static union {
        project_t v4;
        project_v3_t v3;
        project_v2_t v2;
        project_v1_t v1;
    } buf;
    uint32_t i, t;
    int bad = 0, ok;

    bad += check("layout: P_CHORD after P_SLDEPTH, then the DIST page's three before P_E0 (53); format 6 = format 3's + 1",
                 P_SLDEPTH + 1 == P_CHORD && P_DMIX + 1 == P_E0 && P_E0 == 53 && PROJ_NP_V6 == PROJ_NP_V3 + 1u &&
                 P_COUNT == PROJ_NP_V6 + 3u && G_COUNT == PROJ_NG_V6 + 2u);
    bad += check("format 5 fits one flash object; 4 slots fit .noinit", sizeof(project_t) <= 4096u - 256u &&
                 4u * sizeof(project_t) < 0x3D50u - 1024u);

    /* format 3 (SLOOP 1.x) */
    memset(&v3, 0, sizeof v3);
    v3.magic = PROJ_MAGIC_V3;
    v3.size = sizeof v3;
    for (i = 0; i < PROJ_NG_V3; i++)
        v3.g[i] = (int16_t)(300 + i);
    v3.g[G_SWING] = 50;
    v3.sel = 3;
    for (t = 0; t < NTRK; t++)
        fill_v3_track(&v3.t[t], t);
    v3.sum = proj_hash(&v3, sizeof v3 - 4u);
    memcpy(&buf, &v3, sizeof v3);
    ok = proj_import(&q, &buf, (int)sizeof v3);
    bad += check("FUN3 -> FUN4: converted, valid format 4 slot", ok && proj_ok(&q) && q.magic == PROJ_MAGIC);
    ok = q.sel == 3 && q.g[G_SWING] == 40;
    for (i = 0; i < PROJ_NG_V3; i++)
        ok &= i == G_SWING || q.g[i] == (int16_t)(300 + i);
    for (i = PROJ_NG_V3; i < G_COUNT; i++)
        ok &= q.g[i] == GP[i].def;
    bad += check("FUN3 -> FUN4: globals (swing 50 -> 40: the MPC scale), the new ones default", ok);
    ok = 1;
    for (t = 0; t < NTRK; t++) {
        const proj_trk_t *n = &q.t[t];
        uint32_t k;
        ok &= (t == TRK_DRUM ? n->engine == 0 : n->engine == OLD_ENG[t]) && steps_ok(proj_steps_c(&q, t), v3.t[t].step, t == TRK_DRUM);
        for (k = 0; k < PROJ_NP_V3 - 8u; k++)
            if (k != P_SSWING && k != P_ASWING)
                ok &= n->p[k] == oldv(t, k);
        ok &= n->p[P_SSWING] == 40 && n->p[P_ASWING] == 80 && n->p[P_CHORD] == 0;
        for (k = 0; k < 8u; k++)
            ok &= n->p[P_E0 + k] == oldv(t, PROJ_NP_V3 - 8u + k);
    }
    bad += check("FUN3 -> FUN4: parameters (P_E0.. moved), steps, drum notes -> lanes", ok);

    /* format 2, as written before the SLICER */
    memset(&v2, 0, sizeof v2);
    v2.magic = PROJ_MAGIC_V2;
    v2.size = sizeof v2;
    for (i = 0; i < PROJ_NG_V2; i++)
        v2.g[i] = (int16_t)(500 + i);
    v2.sel = 2;
    for (t = 0; t < NTRK; t++)
        fill_v2_track(&v2.t[t], t);
    v2.sum = proj_hash(&v2, sizeof v2 - 4u);
    bad += check("FUN2 image is 2552 bytes (as stored)", sizeof v2 == 2552u);
    memcpy(&buf, &v2, sizeof v2);
    ok = proj_import(&q, &buf, (int)sizeof v2);
    bad += check("FUN2 -> FUN4: converted, valid format 4 slot", ok && proj_ok(&q) && q.magic == PROJ_MAGIC);
    ok = q.sel == 2;
    for (i = 0; i < PROJ_NG_V2; i++)
        ok &= i == G_SWING || q.g[i] == (int16_t)(500 + i);
    bad += check("FUN2 -> FUN4: globals and selected track", ok);
    ok = 1;
    for (t = 0; t < NTRK; t++)
        ok &= track_ok_v2(&q, &v2.t[t], t);
    bad += check("FUN2 -> FUN4: every parameter mapped, SLICER OFF, CHORD OFF (4 tracks)", ok);
    bad += check("FUN2 -> FUN4: engine bytes kept (WHEEL 7, ANALOG 0, TRIO 6), drum 0",
                 q.t[0].engine == 7 && q.t[1].engine == 0 && q.t[2].engine == 6 && q.t[3].engine == 0);
    /* (sloopDX has the DX7 only: an old engine byte loads as engine % NENGINES, see proj_apply) */

    /* a FUN4 round trip: stored as is (an engine added since: 8) */
    q.t[1].engine = 8;
    proj_steps(&q, 0)[3].lvl = 0x9C;
    proj_steps(&q, 0)[3].rat = 0x27;
    dstep_set(&((dstep_t *)proj_steps(&q, TRK_DRUM))[5], 13, LV_GHOST, 2);
    q.sum = proj_sum(&q);
    memcpy(&buf, &q, sizeof q);
    bad += check("FUN6 -> FUN6: as stored (levels, ratchets, lanes, engine 8)",
                 proj_import(&q2, &buf, (int)sizeof q) && !memcmp(&q, &q2, sizeof q) && q2.t[1].engine == 8);

    /* damaged / wrong size */
    v2.t[1].p[3]++;
    memcpy(&buf, &v2, sizeof v2);
    bad += check("FUN2 with a bad checksum: refused", !proj_import(&q2, &buf, (int)sizeof v2));
    v2.t[1].p[3]--;
    memcpy(&buf, &v2, sizeof v2);
    bad += check("FUN2 with a wrong length: refused", !proj_import(&q2, &buf, (int)sizeof v2 - 2));
    memcpy(&buf, &q, sizeof q);
    buf.v4.magic = PROJ_MAGIC_V3;
    bad += check("FUN4 size with a FUN3 magic: refused", !proj_import(&q2, &buf, (int)sizeof q));
    memcpy(&buf, &v3, sizeof v3);
    buf.v3.t[2].step[7].vel ^= 1u;
    bad += check("FUN3 with a bad checksum: refused", !proj_import(&q2, &buf, (int)sizeof v3));

    /* format 1: one instrument -> track 1, the others their defaults */
    memset(&v1, 0, sizeof v1);
    v1.magic = PROJ_MAGIC_V1;
    v1.size = sizeof v1;
    for (i = 0; i < PROJ_NG_V2; i++)
        v1.g[i] = (int16_t)(700 + i);
    fill_v2_track(&v1.t, 0);
    v1.sum = proj_hash(&v1, sizeof v1 - 4u);
    memcpy(&buf, &v1, sizeof v1);
    ok = proj_import(&q, &buf, (int)sizeof v1) && proj_ok(&q) && track_ok_v2(&q, &v1.t, 0) && q.g[5] == 705;
    for (t = 1; t < NTRK; t++)
        ok &= q.t[t].preset == 0xFF && q.t[t].p[P_SLCR] == 0 && q.t[t].p[P_LEVEL] == TP[P_LEVEL].def &&
              q.t[t].p[P_E0] == ENGINES[trk_def_engine(t)]->edit[0].def &&
              (t == TRK_DRUM ? dstep_mask((const dstep_t *)proj_steps_c(&q, t)) == 0u : proj_steps_c(&q, t)[0].time == ST_REST);
    bad += check("FUN1 -> FUN4: track 1 mapped, tracks 2..4 defaults", ok);

    /* capture / apply: the working project round trip */
    host_tracks_init();
    for (t = 0; t < NTRK; t++)
        trk[t].p[P_SLEN] = (int16_t)(5 + t);
    trk[1].step[2].n = 2, trk[1].step[2].note[0] = 60, trk[1].step[2].note[1] = 64, trk[1].step[2].time = ST_NOTE;
    trk[1].step[2].lvl = 0x0D;
    dstep_set(&TDRUM->dstep[6], 4, LV_SOFT, 1);         /* (inside its LEN, 8: format 6 keeps LEN steps) */
    song.g[G_DUST] = 33;
    proj_capture(&q);
    host_tracks_init();
    proj_apply(&q, 1);
    ok = trk[2].p[P_SLEN] == 7 && trk[1].step[2].n == 2 && trk[1].step[2].lvl == 0x0D && song.g[G_DUST] == 33 &&
         dstep_has(&TDRUM->dstep[6], 4) && dstep_lvl(&TDRUM->dstep[6], 4) == LV_SOFT && dstep_rat(&TDRUM->dstep[6], 4) == 1u;
    bad += check("the working project: capture -> apply round trip (levels, lanes, DUST)", ok);

    /* sloopDX 2.0 added three factory voices before INIT VOICE: a project saved before (dxv 0) has its DX7
     * voices from 16 on (INIT VOICE, the bank) moved up by three; one saved now keeps them */
    host_tracks_init();
    trk[0].p[P_E0] = 5, trk[1].p[P_E0] = 16, trk[2].p[P_E0] = 20;
    trk[0].preset = 5, trk[1].preset = 16;
    proj_capture(&q);
    ok = q.dxv == PROJ_DXV;
    proj_apply(&q, 1);
    ok &= trk[0].p[P_E0] == 5 && trk[1].p[P_E0] == 16 && trk[2].p[P_E0] == 20 && trk[1].preset == 16u;
    q.dxv = 0;
    q.t[0].p[P_E6] = q.t[2].p[P_E6] = 0;                /* (CUT did not exist: saved as 0) */
    q.t[3].p[P_E0] = 2;                                 /* the drum track: its kit, never moved */
    proj_apply(&q, 1);
    ok &= trk[0].p[P_E0] == 5 && trk[1].p[P_E0] == 19 && trk[2].p[P_E0] == 23 && trk[0].preset == 5u &&
          trk[1].preset == 19u && trk[3].p[P_E0] == 2 && trk[0].p[P_E6] == DX_CUT_OPEN && trk[2].p[P_E6] == DX_CUT_OPEN;
    bad += check("a project from before 2.0: DX7 voices 16.. up by three, CUT open", ok);
    q.dxv = 1;                                          /* 2.0 / 2.1: a bank voice saved with CUT 0 opens, a factory one stays */
    q.t[0].p[P_E0] = 25, q.t[0].p[P_E6] = 0, q.t[1].p[P_E0] = 3, q.t[1].p[P_E6] = 0;
    proj_apply(&q, 1);
    ok = trk[0].p[P_E6] == DX_CUT_OPEN && trk[1].p[P_E6] == 0 && trk[0].p[P_E0] == 25;
    bad += check("a project of 2.0 / 2.1: a bank voice's shut CUT (the PRESETS bug) opens, a factory voice's stays", ok);

    {   /* format 5: the drum lanes' macros and locks go with the project; a format 4 project (sloopDX 2.0, SLOOP
         * 2.x) loads with neutral macros and no locks; out-of-range macros come back inside their range */
        static project_t q5;
        static project_v4_t o4;
        host_tracks_init();
        memset(&dext, 0, sizeof dext);
        dext.m[2][DM_TUNE] = -5, dext.m[5][DM_DECAY] = 30, dext.m[9][DM_PAN] = -20, dext.lock[7] = dlock_make(2, 1, 3, 0, 0);
        proj_capture(&q5);
        memset(&dext, 0, sizeof dext);
        proj_apply(&q5, 1);
        ok = q5.magic == PROJ_MAGIC && dext.m[2][DM_TUNE] == -5 && dext.m[5][DM_DECAY] == 30 && dext.m[9][DM_PAN] == -20 &&
             dlock_tune(dext.lock[7]) == 3;
        q5.dext.m[0][DM_TUNE] = 100;
        proj_apply(&q5, 1);
        ok &= dext.m[0][DM_TUNE] == 24;
        memset(&o4, 0, sizeof o4);
        o4.magic = PROJ_MAGIC_V4, o4.size = sizeof o4, o4.sel = 2, o4.dxv = PROJ_DXV;
        memcpy(o4.g, q5.g, sizeof o4.g);                 /* (the first PROJ_NG_V6: the same ids) */
        for (t = 0; t < NTRK; t++) {
            p_to_v6(o4.t[t].p, q5.t[t].p);
            o4.t[t].step[PROJ_NSTEP_OLD - 1u].n = (uint8_t)(t + 1u);   /* (the last of its 64: kept in the pool) */
        }
        o4.sum = proj_hash(&o4, sizeof o4 - 4u);
        ok &= proj_import(&q, &o4, (int)sizeof o4) && proj_ok(&q) && q.sel == 2 && q.dxv == PROJ_DXV &&
              !memcmp(q.t[1].p, q5.t[1].p, sizeof q.t[1].p) && q.t[3].nst == 64u && proj_steps_c(&q, 2)[63].n == 3u &&
              !q.dext.m[2][DM_TUNE] && !q.dext.lock[7];
        o4.sum ^= 1u;
        ok &= !proj_import(&q, &o4, (int)sizeof o4);
        memset(&dext, 0, sizeof dext);
        bad += check("format 5: drum macros and locks kept; FUN4 loads with neutral ones", ok);
    }

    {   /* format 6 (2.2 .. 2.6) -> 7: the parameters mapped by count (E0..E7 moved up by three), the DIST page's
         * and the CHORUS MIX / REVERB PRE take their defaults; the pool and the drum macros as they were */
        static project_t q7;
        static project_v6_t o6;
        host_tracks_init();
        trk[0].p[P_E0 + 3] = 77, trk[1].p[P_SLDEPTH] = 33, trk[2].p[P_CHORD] = 2;
        trk[0].p[P_DTYPE] = 3, song.g[G_RPRE] = 40;     /* (format 6 has no room for these: defaults) */
        dext.m[4][DM_TUNE] = 7;
        proj_capture(&q7);
        memset(&o6, 0, sizeof o6);
        o6.magic = PROJ_MAGIC_V6, o6.size = sizeof o6, o6.sel = q7.sel, o6.dxv = q7.dxv;
        memcpy(o6.g, q7.g, sizeof o6.g);
        for (t = 0; t < NTRK; t++) {
            p_to_v6(o6.t[t].p, q7.t[t].p);
            o6.t[t].engine = q7.t[t].engine, o6.t[t].preset = q7.t[t].preset, o6.t[t].nst = q7.t[t].nst;
        }
        memcpy(o6.pool, q7.pool, sizeof o6.pool);
        memcpy(&o6.dext, &q7.dext, sizeof o6.dext);
        o6.sum = proj_hash(&o6, sizeof o6 - 4u);
        ok = proj_import(&q, &o6, (int)sizeof o6) && proj_ok(&q) && q.t[0].p[P_E0 + 3] == 77 && q.t[1].p[P_SLDEPTH] == 33 &&
             q.t[2].p[P_CHORD] == 2 && q.t[0].p[P_DTYPE] == TP[P_DTYPE].def && q.t[3].p[P_DMIX] == TP[P_DMIX].def &&
             q.g[G_RPRE] == GP[G_RPRE].def && q.g[G_CMIX] == GP[G_CMIX].def && q.g[G_BPM] == q7.g[G_BPM] &&
             !memcmp(q.pool, q7.pool, sizeof q.pool) && q.dext.m[4][DM_TUNE] == 7 && q.t[2].nst == q7.t[2].nst;
        o6.sum ^= 1u;
        ok &= !proj_import(&q, &o6, (int)sizeof o6);
        memset(&dext, 0, sizeof dext);
        song.g[G_RPRE] = 0;
        bad += check("format 6 -> 7: parameters mapped by count, the new ones at their defaults", ok);
    }

    {   /* format 6: one pool of 256 steps. A track up to 128, the others what is left (slen_room / slen_set);
         * capture stores LEN steps each, in track order; apply puts them back and empties the rest; a format 5
         * project (64 + 64 + 64 + 64) fills the pool exactly */
        static project_t q6;
        uint32_t k;
        host_tracks_init();
        for (t = 0; t < NTRK; t++) trk[t].p[P_SLEN] = 16;
        ok = slen_room(&trk[0]) == 128u && !slen_set(&trk[0], 128) && trk[0].p[P_SLEN] == 128;
        ok &= slen_room(&trk[1]) == 256u - 128u - 32u && slen_set(&trk[1], 120) && trk[1].p[P_SLEN] == 96;
        ok &= slen_room(&trk[2]) == 16u && slen_set(&trk[2], 64) && trk[2].p[P_SLEN] == 16;
        for (k = 0; k < 128u; k++) trk[0].step[k].n = (uint8_t)(k & 3u), trk[0].step[k].time = ST_NOTE, trk[0].step[k].note[0] = (uint8_t)k;
        dstep_set(&TDRUM->dstep[15], 2, LV_HARD, 0);
        trk[1].step[95].note[0] = 99, trk[1].step[95].n = 1, trk[1].step[95].time = ST_NOTE;
        proj_capture(&q6);
        ok &= q6.t[0].nst == 128u && q6.t[1].nst == 96u && q6.t[2].nst == 16u && q6.t[3].nst == 16u &&
              proj_steps_c(&q6, 1)[95].note[0] == 99u && sizeof q6 <= 4096u - 256u;
        host_tracks_init();
        proj_apply(&q6, 1);
        ok &= trk[0].p[P_SLEN] == 128 && trk[0].step[127].note[0] == 127u && trk[1].step[95].note[0] == 99u &&
              dstep_has(&TDRUM->dstep[15], 2) && trk[2].step[20].time == ST_REST && !dstep_mask(&TDRUM->dstep[16]);
        q6.t[3].p[P_SLEN] = 128;                            /* a project made elsewhere, over the pool: cut */
        q6.sum = proj_sum(&q6);
        proj_apply(&q6, 1);
        ok &= trk[3].p[P_SLEN] == 16 && trk[0].p[P_SLEN] + trk[1].p[P_SLEN] + trk[2].p[P_SLEN] + trk[3].p[P_SLEN] <= 256;
        bad += check("format 6: a 256-step pool, a track up to 128; LEN steps stored in track order", ok);
    }

    printf("%s\n", bad ? "PROJECT FORMAT TEST FAILED" : "project format test passed");
    return bad != 0;
}
