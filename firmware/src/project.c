/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Projects: four slots in .noinit RAM (the song sections A..D), so they survive resets and UBOOT
 * entry. With FELUCCA_FLASH every save also goes to flash through storage.c, and an empty RAM slot
 * is filled from flash on load. The working project is also kept in flash by itself (autosave, when
 * the transport is stopped and nothing sounds) and comes back at power-on: SLOOP starts where you
 * left it.
 *
 * Formats: 8 ("FUN8", written, sloopDX 4.0): format 7, SLOOP 2.6's nine track parameters (the INSERT, ENV SYNC,
 * LFO 2: P_ITYPE .. P_LPOL, before P_E0) and GLO > TEXTURE (G_TXSRC .., in tx[]); 7 ("FUN7", sloopDX 2.7 .. 3.x):
 * PROJ_NP_V7, read and mapped by count. 7 had format 6 and the effect pages' new parameters (P_DTONE..,
 * G_CMIX..); 6 ("FUN6", sloopDX 2.2 .. 2.6): PROJ_NP_V6 / PROJ_NG_V6, the steps in one pool, read and mapped by
 * count as user presets are. 5 ("FUN5", sloopDX 2.1): format 4 and the drum track's lane macros and step locks
 * (drum_ext_t). 4 ("FUN4", SLOOP 2.0, sloopDX up to 2.0): today's P_COUNT / G_COUNT, 10-byte steps (levels and
 * ratchets; the drum track: 16 lanes); read with neutral macros. Read and converted: 3 ("FUN3", SLOOP 1.x: 8-byte steps, the
 * drum track's notes become its lanes, the swings x 0.8 for the MPC scale), 2 ("FUN2") and 1 ("FUN1"),
 * which held PROJ_NP_V2 parameters per track, mapped by count as user presets are (the first
 * PROJ_NP_V2 - 8 are P_LEVEL.. in order, the last 8 P_E0..P_E7; the parameters added since take their
 * defaults). Their engine bytes are kept: formats 1 and 2 had engines 0..7 (ANALOG .. WHEEL), and the
 * engines added since were appended, no index moved; the drum track's byte (it has no engine) becomes 0.
 *
 * Built on the host too (tests/project_test.c, -DPROJ_HOST): the part above the #ifndef
 * PROJ_HOST needs core.h, params.c (TP), drums.c (the lanes), the engines and trk_def_engine (ui.c). */
#define PROJ_MAGIC 0x46554E38u                 /* "FUN8": four tracks, P_COUNT parameters each, their steps in one pool */
#define PROJ_MAGIC_V7 0x46554E37u              /* "FUN7": the same with PROJ_NP_V7, no TEXTURE; read only */
#define PROJ_MAGIC_V6 0x46554E36u              /* "FUN6": the same with PROJ_NP_V6 / PROJ_NG_V6; read only */
#define PROJ_MAGIC_V5 0x46554E35u              /* "FUN5": 64 steps a track and the drum macros; read only */
#define PROJ_MAGIC_V4 0x46554E34u              /* "FUN4": the same without the drum macros; read only */
#define PROJ_MAGIC_V3 0x46554E33u              /* "FUN3": SLOOP 1.x; read only */
#define PROJ_MAGIC_V2 0x46554E32u              /* "FUN2": four tracks, PROJ_NP_V2 parameters; read only */
#define PROJ_MAGIC_V1 0x46554E31u              /* "FUN1": one instrument; loads into track 1 */
#define PROJ_NP_V7 61u                         /* P_COUNT of format 7 (P_E0 was 53) */
#define PROJ_NP_V6 58u                         /* P_COUNT of formats 4..6 (P_E0 was 50) */
#define PROJ_NG 34u                            /* the globals formats 7, 8 keep in g[] (G_DRDLY, 3.4: in drdly) */
#define PROJ_NTX 7u                            /* format 8: GLO > TEXTURE, G_TXSRC .. G_TXDUCK, in tx[] */
#define PROJ_NG_V6 32u                         /* G_COUNT of formats 4..6 */
#define PROJ_NP_V3 57u                         /* P_COUNT of format 3 (P_E0 was 49) */
#define PROJ_NG_V3 27u                         /* G_COUNT of formats 1..3 */
#define PROJ_NP_V2 53u                         /* P_COUNT of formats 1 and 2 (P_E0 was 45) */
#define PROJ_NG_V2 27u                         /* G_COUNT of formats 1 and 2 */
#define PROJ_DXV 2u                            /* 1: the DX7 voice numbers of sloopDX 2.0 (core.h DX_VOICE_FROM_V1);
                                                * 2: a bank voice's CUT as saved (2.2; before: DX_CUT_FIX) */
#define PROJ_NSTEP_OLD 64u                     /* the steps per track of formats 1..5 */
typedef struct {                               /* one track of format 8: its parameters; its steps are in the pool */
    int16_t p[P_COUNT];
    uint8_t engine, preset;
    uint8_t nst, rsv;                          /* how many of the pool's steps are its (in track order) */
} proj_trk_t;
typedef struct {                               /* format 8 (sloopDX 4.0): the steps of all tracks in one pool */
    uint32_t magic, size;
    int16_t g[PROJ_NG];
    uint8_t sel, dxv, drdly, rsv;              /* the selected track; dxv: PROJ_DXV (0: DX7 voices before 2.0);
                                                * drdly: G_DRDLY (3.4; 0 before, its default) */
    int8_t tx[PROJ_NTX + 1u];                  /* GLO > TEXTURE (4.0, SLOOP 2.6), G_TXSRC .. G_TXDUCK; one byte spare */
    proj_trk_t t[NTRK];
    step_t pool[STEP_POOL];                    /* track 1's steps, then track 2's ..; the drum track's are dstep_t */
    drum_ext_t dext;                           /* the drum lanes' macros and step locks (drums.c) */
    uint32_t sum;
} project_t;
_Static_assert(G_TXDUCK - G_TXSRC + 1u == PROJ_NTX, "format 8: the TEXTURE globals in a row");
typedef struct {                               /* a track of format 7, read only */
    int16_t p[PROJ_NP_V7];
    uint8_t engine, preset;
    uint8_t nst, rsv;
} proj_trk7_t;
typedef struct {                               /* format 7 (sloopDX 2.7 .. 3.x), read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG];
    uint8_t sel, dxv, drdly, rsv;
    proj_trk7_t t[NTRK];
    step_t pool[STEP_POOL];
    drum_ext_t dext;
    uint32_t sum;
} project_v7_t;
typedef struct {                               /* a track of format 6, read only */
    int16_t p[PROJ_NP_V6];
    uint8_t engine, preset;
    uint8_t nst, rsv;
} proj_trk6_t;
typedef struct {                               /* format 6 (sloopDX 2.2 .. 2.6), read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V6];
    uint8_t sel, dxv, rsv[2];
    proj_trk6_t t[NTRK];
    step_t pool[STEP_POOL];
    drum_ext_t dext;
    uint32_t sum;
} project_v6_t;
_Static_assert(sizeof(step_t) == sizeof(dstep_t), "a pool step holds either");
_Static_assert(PROJ_NG == G_DRDLY, "formats 7, 8 keep the globals before G_DRDLY in g[]");
typedef struct {                               /* a track of formats 4 and 5, read only */
    int16_t p[PROJ_NP_V6];
    uint8_t engine, preset;
    union {
        step_t step[PROJ_NSTEP_OLD];
        dstep_t dstep[PROJ_NSTEP_OLD];         /* (the drum track: 16 lanes, the same size) */
    };
} proj_trk5_t;
typedef struct {                               /* the drum macros of format 5 (64 steps of locks) */
    int8_t m[DRUM_LANES][DM_N];
    uint16_t lock[PROJ_NSTEP_OLD];
} drum_ext5_t;
typedef struct {                               /* format 5 (sloopDX 2.1), read only; every older format converts to it */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V6];
    uint8_t sel, dxv, rsv[2];
    proj_trk5_t t[NTRK];
    drum_ext5_t dext;
    uint32_t sum;
} project_v5_t;
typedef struct {                               /* format 4 (SLOOP 2.x, sloopDX up to 2.0), read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V6];
    uint8_t sel, dxv, rsv[2];
    proj_trk5_t t[NTRK];
    uint32_t sum;
} project_v4_t;
typedef struct { uint8_t note[4], n, time, flags, vel; } step8_t;   /* the steps of formats 1..3 */
typedef struct {                               /* a track of format 3, read only */
    int16_t p[PROJ_NP_V3];
    uint8_t engine, preset;
    step8_t step[PROJ_NSTEP_OLD];
} proj_trk_v3_t;
typedef struct {                               /* format 3 (SLOOP 1.x), read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V3];
    uint8_t sel, rsv[3];
    proj_trk_v3_t t[NTRK];
    uint32_t sum;
} project_v3_t;
typedef struct {                               /* a track of formats 1 and 2, read only */
    int16_t p[PROJ_NP_V2];
    uint8_t engine, preset;
    step8_t step[PROJ_NSTEP_OLD];
} proj_trk_v2_t;
typedef struct {                               /* format 2 (until 0.9), read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V2];
    uint8_t sel, rsv[3];
    proj_trk_v2_t t[NTRK];
    uint32_t sum;
} project_v2_t;
typedef struct {                               /* format 1 (until 0.5 beta), read only */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V2];
    proj_trk_v2_t t;
    uint32_t sum;
} project_v1_t;
_Static_assert(sizeof(project_v2_t) == 2552u && sizeof(project_v1_t) == 688u && sizeof(project_v3_t) == 2584u,
               "formats 1 / 2 / 3 as they were stored");
project_t proj_slot[4] __attribute__((section(".noinit")));

static uint32_t proj_hash(const void *p, uint32_t n)   /* FNV-1a over n bytes */
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t i, s = 0x811C9DC5u;
    for (i = 0; i < n; i++)
        s = (s ^ b[i]) * 16777619u;
    return s;
}
static uint32_t proj_sum5(const project_v5_t *p) { return proj_hash(p, sizeof *p - 4u); }
static uint32_t proj_sum(const project_t *p) { return proj_hash(p, sizeof *p - 4u); }
static int proj_ok(const project_t *q) { return q->magic == PROJ_MAGIC && q->size == sizeof *q && q->sum == proj_sum(q); }

/* ---- old formats -> format 5 (then format 6: proj_from_v5) */
/* an old step into a synth step (no level, no ratchet) */
static void step_from8(step_t *d, const step8_t *s)
{
    memcpy(d->note, s->note, 4);
    d->n = s->n;
    d->time = s->time;
    d->flags = s->flags;
    d->vel = s->vel;
    d->lvl = d->rat = 0;
}
/* an old drum step (GM notes) into the drum track's lanes; its velocity / accent -> their level */
static void dstep_from8(dstep_t *d, const step8_t *s)
{
    uint32_t i, lvl = (s->flags & SF_ACCENT) || s->vel > 115u ? LV_HARD : !s->vel ? LV_NORM : vel_lvl(s->vel);
    memset(d, 0, sizeof *d);
    if (s->time != ST_NOTE)
        return;
    for (i = 0; i < s->n && i < 4u; i++)
        dstep_set(d, lane_of_note(s->note[i] & 127u), lvl, 0);
}
static int16_t swing_from_v3(int32_t v) { return (int16_t)clamp((v * 4 + 2) / 5, 0, 100); }   /* /250 -> /200 */

/* the globals of formats 1..3 into format 5's (G_* unchanged since; any added later: their defaults) */
static void proj_g_from_old(int16_t *g, const int16_t *g2)
{
    uint32_t i;
    for (i = 0; i < PROJ_NG_V6; i++)
        g[i] = i < PROJ_NG_V3 ? g2[i] : GP[i].def;
    g[G_SWING] = swing_from_v3(g[G_SWING]);
}

/* a track of format 3 -> format 5's (by id up to P_SLDEPTH; P_E0.. moved) */
static void proj_trk_from_v3(proj_trk5_t *d, const proj_trk_v3_t *s, int drum)
{
    uint32_t k, nc = PROJ_NP_V3 - 8u;
    for (k = 0; k < PROJ_NP_V6 - 8u; k++)
        d->p[k] = k < nc ? s->p[k] : TP[k].def;
    for (k = 0; k < 8u; k++)
        d->p[PROJ_NP_V6 - 8u + k] = s->p[nc + k];
    d->p[P_SSWING] = swing_from_v3(d->p[P_SSWING]);
    d->p[P_ASWING] = swing_from_v3(d->p[P_ASWING]);
    d->engine = drum ? 0u : s->engine;
    d->preset = drum ? 0u : s->preset;
    for (k = 0; k < PROJ_NSTEP_OLD; k++) {
        if (drum)
            dstep_from8(&d->dstep[k], &s->step[k]);
        else
            step_from8(&d->step[k], &s->step[k]);
    }
}

/* a track of formats 1 and 2 -> format 3 (mapped by count, see the top) */
static void proj_trk_v2_to_v3(proj_trk_v3_t *d, const proj_trk_v2_t *s, int drum)
{
    uint32_t k, nc = PROJ_NP_V2 - 8u;
    for (k = 0; k < PROJ_NP_V3 - 8u; k++)
        d->p[k] = k < nc ? s->p[k] : TP[k].def;
    for (k = 0; k < 8u; k++)
        d->p[PROJ_NP_V3 - 8u + k] = s->p[nc + k];
    d->engine = drum ? 0u : s->engine;          /* (indices 0..7 as they were) */
    d->preset = drum ? 0u : s->preset;
    memcpy(d->step, s->step, sizeof d->step);
}

/* a format 3 project -> slot q as format 4 */
static void proj_from_v3_ok(project_v5_t *q, const project_v3_t *v3)
{
    uint32_t i;
    memset(q, 0, sizeof *q);
    q->magic = PROJ_MAGIC_V5;
    q->size = sizeof *q;
    proj_g_from_old(q->g, v3->g);
    q->sel = v3->sel;
    for (i = 0; i < NTRK; i++)
        proj_trk_from_v3(&q->t[i], &v3->t[i], i == TRK_DRUM);
    q->sum = proj_sum5(q);
}
static int proj_from_v3(project_v5_t *q, const project_v3_t *v3, int n)
{
    if (n != (int)sizeof *v3 || v3->magic != PROJ_MAGIC_V3 || v3->size != sizeof *v3 ||
        v3->sum != proj_hash(v3, sizeof *v3 - 4u))
        return 0;
    proj_from_v3_ok(q, v3);
    return 1;
}

static project_v3_t proj_v3_tmp;               /* (formats 1, 2: through format 3) */
/* a format 2 project (n bytes in *v2) -> slot q as format 4 */
static int proj_from_v2(project_v5_t *q, const project_v2_t *v2, int n)
{
    project_v3_t *v3 = &proj_v3_tmp;
    uint32_t i;
    if (n != (int)sizeof *v2 || v2->magic != PROJ_MAGIC_V2 || v2->size != sizeof *v2 ||
        v2->sum != proj_hash(v2, sizeof *v2 - 4u))
        return 0;
    memset(v3, 0, sizeof *v3);
    memcpy(v3->g, v2->g, sizeof v3->g);
    v3->sel = v2->sel;
    for (i = 0; i < NTRK; i++)
        proj_trk_v2_to_v3(&v3->t[i], &v2->t[i], i == TRK_DRUM);
    proj_from_v3_ok(q, v3);
    return 1;
}

/* a format 1 project (n bytes in *v1) -> slot q as format 4: the instrument becomes track 1,
 * tracks 2..4 start empty (their sounds as at power-on) */
static int proj_from_v1(project_v5_t *q, const project_v1_t *v1, int n)
{
    project_v3_t *v3 = &proj_v3_tmp;
    uint32_t i;
    if (n != (int)sizeof *v1 || v1->magic != PROJ_MAGIC_V1 || v1->size != sizeof *v1 ||
        v1->sum != proj_hash(v1, sizeof *v1 - 4u))
        return 0;
    memset(v3, 0, sizeof *v3);
    memcpy(v3->g, v1->g, sizeof v3->g);
    proj_trk_v2_to_v3(&v3->t[0], &v1->t, 0);
    proj_from_v3_ok(q, v3);
    for (i = 1; i < NTRK; i++) {               /* the other tracks: their defaults, no steps */
        uint32_t k;
        for (k = 0; k < PROJ_NP_V6; k++)
            q->t[i].p[k] = k >= PROJ_NP_V6 - 8u ? ENGINES[trk_def_engine(i)]->edit[k - (PROJ_NP_V6 - 8u)].def : TP[k].def;
        q->t[i].engine = (uint8_t)trk_def_engine(i);
        q->t[i].preset = 0xFF;                 /* 0xFF: its default preset (project_load) */
        memset(q->t[i].step, 0, sizeof q->t[i].step);
        if (i != TRK_DRUM)
            for (k = 0; k < PROJ_NSTEP_OLD; k++)
                q->t[i].step[k].time = ST_REST;
    }
    q->sum = proj_sum5(q);
    return 1;
}

/* a format 4 project -> slot q (the drum macros neutral, no locks) */
static int proj_from_v4(project_v5_t *q, const project_v4_t *v4, int n)
{
    if (n != (int)sizeof *v4 || v4->magic != PROJ_MAGIC_V4 || v4->size != sizeof *v4 ||
        v4->sum != proj_hash(v4, sizeof *v4 - 4u))
        return 0;
    memset(q, 0, sizeof *q);
    q->magic = PROJ_MAGIC_V5;
    q->size = sizeof *q;
    memcpy(q->g, v4->g, sizeof q->g);
    q->sel = v4->sel;
    q->dxv = v4->dxv;
    memcpy(q->t, v4->t, sizeof q->t);
    q->sum = proj_sum5(q);
    return 1;
}

/* track k's steps in the pool of q (after the tracks before it) */
static step_t *proj_steps(project_t *q, uint32_t k)
{
    uint32_t i, o = 0;
    for (i = 0; i < k && i < NTRK; i++)
        o += q->t[i].nst;
    return &q->pool[o < STEP_POOL ? o : 0u];
}
static const step_t *proj_steps_c(const project_t *q, uint32_t k) { return proj_steps((project_t *)q, k); }

/* the parameters of a format 4..6 track (PROJ_NP_V6) and the globals -> today's: mapped by count, the
 * last 8 are P_E0..P_E7; the ones added since take their defaults */
static void proj_p_from_v6(int16_t *d, const int16_t *s)
{
    uint32_t k, nc = PROJ_NP_V6 - 8u;
    for (k = 0; k < P_E0; k++)
        d[k] = k < nc ? s[k] : TP[k].def;
    for (k = 0; k < 8u; k++)
        d[P_E0 + k] = s[nc + k];
}
static void proj_g_from_v6(int16_t *d, const int16_t *s)
{
    uint32_t i;
    for (i = 0; i < PROJ_NG; i++)
        d[i] = i < PROJ_NG_V6 ? s[i] : GP[i].def;
}

/* the parameters of a format 7 track (PROJ_NP_V7) -> today's: by id up to P_DMIX, the ones added since (before P_E0)
 * at their defaults, the last 8 are P_E0..P_E7 */
static void proj_p_from_v7(int16_t *d, const int16_t *s)
{
    uint32_t k, nc = PROJ_NP_V7 - 8u;
    for (k = 0; k < P_E0; k++)
        d[k] = k < nc ? s[k] : TP[k].def;
    for (k = 0; k < 8u; k++)
        d[P_E0 + k] = s[nc + k];
}
static void proj_tx_def(project_t *q)          /* TEXTURE as a new project has it (formats 1..7) */
{
    uint32_t k;
    for (k = 0; k < PROJ_NTX; k++)
        q->tx[k] = (int8_t)GP[G_TXSRC + k].def;
}
/* a format 7 project -> slot q as format 8 */
static void proj_from_v7(project_t *q, const project_v7_t *v)
{
    uint32_t k;
    memset(q, 0, sizeof *q);
    q->magic = PROJ_MAGIC;
    q->size = sizeof *q;
    memcpy(q->g, v->g, sizeof q->g);
    q->sel = v->sel;
    q->dxv = v->dxv;
    q->drdly = v->drdly;
    proj_tx_def(q);
    for (k = 0; k < NTRK; k++) {
        proj_p_from_v7(q->t[k].p, v->t[k].p);
        q->t[k].engine = v->t[k].engine;
        q->t[k].preset = v->t[k].preset;
        q->t[k].nst = v->t[k].nst;
    }
    memcpy(q->pool, v->pool, sizeof q->pool);
    memcpy(&q->dext, &v->dext, sizeof q->dext);
    q->sum = proj_sum(q);
}

/* a format 6 project -> slot q as format 8 */
static void proj_from_v6(project_t *q, const project_v6_t *v)
{
    uint32_t k;
    memset(q, 0, sizeof *q);
    q->magic = PROJ_MAGIC;
    q->size = sizeof *q;
    proj_g_from_v6(q->g, v->g);
    q->sel = v->sel;
    q->dxv = v->dxv;
    proj_tx_def(q);
    for (k = 0; k < NTRK; k++) {
        proj_p_from_v6(q->t[k].p, v->t[k].p);
        q->t[k].engine = v->t[k].engine;
        q->t[k].preset = v->t[k].preset;
        q->t[k].nst = v->t[k].nst;
    }
    memcpy(q->pool, v->pool, sizeof q->pool);
    memcpy(&q->dext, &v->dext, sizeof q->dext);
    q->sum = proj_sum(q);
}

/* a format 5 project -> slot q as format 8: each track's 64 steps into the pool (4 x 64 = all of it) */
static void proj_from_v5(project_t *q, const project_v5_t *v)
{
    uint32_t k;
    memset(q, 0, sizeof *q);
    q->magic = PROJ_MAGIC;
    q->size = sizeof *q;
    proj_g_from_v6(q->g, v->g);
    q->sel = v->sel;
    q->dxv = v->dxv;
    proj_tx_def(q);
    for (k = 0; k < NTRK; k++) {
        proj_p_from_v6(q->t[k].p, v->t[k].p);
        q->t[k].engine = v->t[k].engine;
        q->t[k].preset = v->t[k].preset;
        q->t[k].nst = (uint8_t)PROJ_NSTEP_OLD;
        memcpy(&q->pool[k * PROJ_NSTEP_OLD], v->t[k].step, sizeof v->t[k].step);
    }
    memcpy(q->dext.m, v->dext.m, sizeof q->dext.m);
    memcpy(q->dext.lock, v->dext.lock, sizeof v->dext.lock);
    q->sum = proj_sum(q);
}
static project_v5_t proj_v5_tmp __attribute__((section(".pool")));   /* (older formats: through format 5) */

/* n bytes of a stored project (any format) -> slot q as format 8; 0 = not a project */
static int proj_import(project_t *q, const void *b, int n)
{
    project_v5_t *v = &proj_v5_tmp;
    const project_v6_t *v6 = (const project_v6_t *)b;
    const project_v7_t *v7 = (const project_v7_t *)b;
    if (n == (int)sizeof *q && proj_ok((const project_t *)b)) {
        memcpy(q, b, sizeof *q);
        return 1;
    }
    if (n == (int)sizeof *v7 && v7->magic == PROJ_MAGIC_V7 && v7->size == sizeof *v7 &&
        v7->sum == proj_hash(v7, sizeof *v7 - 4u)) {
        proj_from_v7(q, v7);
        return 1;
    }
    if (n == (int)sizeof *v6 && v6->magic == PROJ_MAGIC_V6 && v6->size == sizeof *v6 &&
        v6->sum == proj_hash(v6, sizeof *v6 - 4u)) {
        proj_from_v6(q, v6);
        return 1;
    }
    if (n == (int)sizeof *v && ((const project_v5_t *)b)->magic == PROJ_MAGIC_V5 && ((const project_v5_t *)b)->size == sizeof *v &&
        ((const project_v5_t *)b)->sum == proj_sum5((const project_v5_t *)b))
        memcpy(v, b, sizeof *v);
    else if (!(proj_from_v4(v, (const project_v4_t *)b, n) || proj_from_v3(v, (const project_v3_t *)b, n) ||
               proj_from_v2(v, (const project_v2_t *)b, n) || proj_from_v1(v, (const project_v1_t *)b, n)))
        return 0;
    proj_from_v5(q, v);
    return 1;
}

/* ---- the working project <-> a project_t */
static void proj_capture(project_t *p)        /* what is playing now, as a project */
{
    uint32_t i;
    memset(p, 0, sizeof *p);
    p->magic = PROJ_MAGIC;
    p->size = sizeof *p;
    p->dxv = PROJ_DXV;
    p->dext = dext;
    for (i = 0; i < PROJ_NG; i++)
        p->g[i] = song.g[i];
    p->drdly = (uint8_t)song.g[G_DRDLY];
    for (i = 0; i < PROJ_NTX; i++)
        p->tx[i] = (int8_t)song.g[G_TXSRC + i];
    p->sel = song.sel;
    {
        uint32_t used = 0;
        for (i = 0; i < NTRK; i++) {                    /* each track's LEN steps, one after the other */
            uint32_t n = trk_len(&trk[i]);
            n = n > NSTEP ? NSTEP : n;
            n = used + n > STEP_POOL ? STEP_POOL - used : n;   /* (slen_room keeps them inside: a guard) */
            memcpy(p->t[i].p, trk[i].p, sizeof trk[i].p);
            p->t[i].engine = trk[i].eng_req;
            p->t[i].preset = trk[i].preset;
            p->t[i].nst = (uint8_t)n;
            memcpy(&p->pool[used], trk[i].step, n * sizeof(step_t));
            used += n;
        }
    }
    p->sum = proj_sum(p);
}

/* a project's tracks (and its globals, all: a load; or only the drum level / reverb: a song
 * section) into the working one, every value back inside its range. The audio ISR must not run
 * meanwhile (the song sections: called from it; a load: IRQ off) */
static void proj_apply(const project_t *p, int all)
{
    uint32_t i, k;
    for (i = 0; i < PROJ_NG; i++)
        if (all ? i != G_SLOT && i != G_LOAD && i != G_SAVE && i != G_SYNC && i != G_MIDI && i != G_ROUTE : i == G_DRLVL || i == G_DRREV)
            song.g[i] = (int16_t)clamp(p->g[i], GP[i].min, GP[i].max);
    song.g[G_DRDLY] = (int16_t)clamp(p->drdly, GP[G_DRDLY].min, GP[G_DRDLY].max);   /* (a section too: the drums' sends) */
    for (i = 0; i < PROJ_NTX; i++)                      /* GLO > TEXTURE: a load and a section (4.0, SLOOP 2.6) */
        song.g[G_TXSRC + i] = (int16_t)clamp(p->tx[i], GP[G_TXSRC + i].min, GP[G_TXSRC + i].max);
    for (k = 0; k < DRUM_LANES; k++)                    /* the drum lanes' macros, each inside its range; the locks */
        for (i = 0; i < DM_N; i++)
            dext.m[k][i] = (int8_t)clamp(p->dext.m[k][i], DM_DESC[i].min, DM_DESC[i].max);
    memcpy(dext.lock, p->dext.lock, sizeof dext.lock);
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        const proj_trk_t *s = &p->t[k];
        uint32_t e = k < NPART ? s->engine % NENGINES : 0u;
        t->eng_req = (uint8_t)e;
        t->user = 0;                                    /* (no user preset slot is saved) */
        for (i = 0; i < P_COUNT; i++) {                 /* every value back inside its range */
            const param_desc_t *d = k == TRK_DRUM && i == P_E0 ? &DRUM_KIT_DESC :   /* the drum kit */
                                    i >= P_E0 && i <= P_E7 ? &ENGINES[e]->edit[i - P_E0] : &TP[i];
            int32_t v = s->p[i];
            if (p->dxv < 1u && k < NPART && i == P_E0)
                v = DX_VOICE_FROM_V1(v);                /* (sloopDX has one engine: the DX7) */
            if (p->dxv < 1u && k < NPART && i == P_E6)
                v = DX_CUT_OPEN;
            else if (p->dxv < 2u && k < NPART && i == P_E6)
                v = DX_CUT_FIX(t->p[P_E0], v);          /* (P_E0 is set by now: it comes first) */
            t->p[i] = (int16_t)clamp(v, d->min, d->max);
        }
        {
            uint32_t pr = s->preset == 0xFFu ? 0u : s->preset;
            if (p->dxv < 1u)
                pr = DX_VOICE_FROM_V1(pr);              /* INIT VOICE's preset moved like its voice */
            t->preset = (uint8_t)(ENGINES[e]->npresets ? pr % ENGINES[e]->npresets : 0u);
        }
        {                                               /* its steps from the pool, the rest empty */
            uint32_t n = s->nst > NSTEP ? NSTEP : s->nst;
            memset(t->step, 0, sizeof t->step);
            memcpy(t->step, proj_steps_c(p, k), n * sizeof(step_t));
            if (k != TRK_DRUM)
                for (i = n; i < NSTEP; i++)
                    t->step[i].time = ST_REST;
        }
        if (k != TRK_DRUM)
            for (i = 0; i < NSTEP; i++) {
                step_t *st = &t->step[i];
                uint32_t j;
                if (st->n > 4u)
                    st->n = 4;
                if (st->time > ST_REST)
                    st->time = ST_REST;
                for (j = 0; j < 4u; j++)
                    st->note[j] &= 127u;
            }
    }
    slen_fit_all();                                     /* (a project made elsewhere: the pool still holds) */
}

#ifndef PROJ_HOST
#if FELUCCA_ARRANGER
#include "arranger_scene.c"
#endif
static uint8_t sec_dirty, song_dirty;           /* live sections / the song: in RAM, not yet in flash */
#if FELUCCA_FLASH
/* slot from flash into RAM (format 7, or an old one converted) */
static union {
    project_t v4;                                   /* (today's format: the name stayed) */
    project_v7_t v7;
    project_v6_t v6;
    project_v5_t v5;
    project_v3_t v3;
    project_v2_t v2;
    project_v1_t v1;
} proj_tmp;
/* upreset.c: a user preset bank staged for a write (4.0, after sloop-lab: the banks are read in the XIP window, no RAM
 * mirror; the main loop only, as every user of proj_tmp) */
UI_SMALL static void *up_stage(void) { return &proj_tmp; }
_Static_assert(sizeof proj_tmp >= sizeof(up_bank_t), "a user preset bank fits the project buffer");
static void proj_fetch(uint32_t slot)
{
    project_t *q = &proj_slot[slot & 3u];
    int n = st_load(OBJ_PROJECT0 + (slot & 3u), &proj_tmp, sizeof proj_tmp);
    if (!proj_import(q, &proj_tmp, n))
        q->magic = 0;
}
#endif

static const char *const SEC_NAME[4] = {"A", "B", "C", "D"};   /* (4.0, SLOOP 2.7: the slots named as the song's sections) */
static void project_save(uint32_t slot)
{
    project_t *p = &proj_slot[slot & 3u];
#if FELUCCA_ARRANGER
    if (song.playing || transport_req) { ui_message("STOP BEFORE SAVE"); return; }
#endif
    proj_capture(p);
#if FELUCCA_FLASH
    if (flash_ok) {
        if (st_save(OBJ_PROJECT0 + (slot & 3u), p, sizeof *p))
            ui_message("SAVE ERROR");
        else
            ui_say("SAVED SECTION ", SEC_NAME[slot & 3u]);
        return;
    }
#endif
    ui_message("SAVED (RAM)");
}

/* SAVE > SECTION, KNOB 2 ERASE (4.0, SLOOP 2.6): the section empty (RAM and flash), its steps out of the song order */
UI_SMALL static void project_erase(uint32_t slot)
{
    slot &= 3u;
#if FELUCCA_ARRANGER
    if (song.playing || transport_req) { ui_message("STOP BEFORE ERASE"); return; }
#endif
    if (!proj_ok(&proj_slot[slot])) {
        ui_say("EMPTY SECTION ", SEC_NAME[slot]);
        return;
    }
#if FELUCCA_FLASH
    if (flash_ok && st_save(OBJ_PROJECT0 + slot, &proj_slot[slot], 0)) {
        ui_message("ERASE ERROR");
        return;
    }
#endif
    fm1_irq_off();
    memset(&proj_slot[slot], 0, sizeof proj_slot[slot]);
    sec_dirty &= (uint8_t)~(1u << slot);
    if (live_sec == (int8_t)slot)
        live_sec = -1;
#if FELUCCA_ARRANGER
    if (arr_drop_scene(&arrangement, slot))
        song_dirty = 1;                                     /* (saved with the settings, when quiet) */
#endif
    fm1_irq_on();
    ui_say("ERASED SECTION ", SEC_NAME[slot]);
}

/* a project into the working one: the transport stops, everything sounding is released */
static void project_apply(const project_t *p)
{
    uint32_t k;
    transport_req = 2;
    panic_req = (1u << NTRK) - 1u;
    drum_mute = 0;                                      /* (the live mutes: a performance's, not the project's) */
    fm1_irq_off();                                      /* the audio ISR must not see half a project */
    proj_apply(p, 1);
    song.sel = (uint8_t)(p->sel < NTRK ? p->sel : 0u);
    fm1_irq_on();
    for (k = 0; k < NPART; k++)                         /* a format 1 project: the default sounds of tracks 2, 3 */
        if (p->t[k].preset == 0xFFu) {
            apply_preset_to(&trk[k], TRK_DEF[k][1]);
            steps_clear(&trk[k]);
        }
    sync_reload = 1;
    ui.force = 1;
}

static void project_load(uint32_t slot)
{
    project_t *p = &proj_slot[slot & 3u];
#if FELUCCA_ARRANGER
    if (song.playing || transport_req) { ui_message("STOP BEFORE LOAD"); return; }
#endif
#if FELUCCA_FLASH
    if (flash_ok && !proj_ok(p))
        proj_fetch(slot);
#endif
    if (!proj_ok(p)) {
        ui_say("EMPTY SECTION ", SEC_NAME[slot & 3u]);
        return;
    }
    project_apply(p);
    ui_say("LOADED SECTION ", SEC_NAME[slot & 3u]);
}

/* ---- the working project, kept in flash by itself: saved when it changed, the transport is stopped,
 * nothing sounds and the panel was not touched for AUTOSAVE_IDLE (a flash erase stops the audio for
 * ~50 ms: never while something plays); loaded at power-on (autosave_resume) */
#define AUTOSAVE_IDLE 2500u                    /* ms without input */
#define AUTOSAVE_GAP 20000u                    /* ms between two saves at least */
static project_t autosave_buf __attribute__((section(".pool")));
static uint32_t autosave_hash, autosave_ms, autosave_checked;

static int audio_quiet(void)
{
    uint32_t p, i;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++)
            if (trk[p].v[i].active)
                return 0;
    for (i = 0; i < NDRUM; i++)
        if (drums.v[i].active)
            return 0;
    return 1;
}

static void autosave_tick(void)                /* main loop */
{
#if FELUCCA_FLASH
    uint32_t h, now = fm1_ms;
    if (!flash_ok || song.playing || transport_req || rec_wait || ft_on || ui.menu ||
        now - ui_input_ms < AUTOSAVE_IDLE || now - autosave_ms < AUTOSAVE_GAP || now - autosave_checked < 1000u)
        return;
    autosave_checked = now;
    proj_capture(&autosave_buf);
    h = autosave_buf.sum;
    if (h == autosave_hash || !audio_quiet())
        return;
    if (st_save(OBJ_AUTOSAVE, &autosave_buf, sizeof autosave_buf) == 0)
        autosave_hash = h;
    autosave_ms = fm1_ms;
#endif
}

static void autosave_resume(void)              /* power-on: the project as it was left (felucca_init) */
{
#if FELUCCA_FLASH
    project_t *q = &autosave_buf;
    int n;
    if (!flash_ok)
        return;
    n = st_load(OBJ_AUTOSAVE, &proj_tmp, sizeof proj_tmp);
    if (!proj_import(q, &proj_tmp, n))
        return;
    autosave_hash = q->sum;
    proj_apply(q, 1);
    song.g[G_DUST] = 0;                        /* (3.4: the master's hiss and crackle never come back by themselves,
                                                * e.g. after a firmware install; a project loaded keeps its DUST) */
    song.sel = (uint8_t)(q->sel < NTRK ? q->sel : 0u);
    for (n = 0; n < NPART; n++)
        trk[n].engine = trk[n].eng_req;        /* (nothing sounds yet: no fade) */
#endif
}

/* settings + learned panel table: one flash object. The flash copy wins at
 * boot (the .noinit copies are garbage after a power-off). */
typedef struct {
    uint32_t magic, palette, lowcut, zoom;
    panel_t panel;
#if FELUCCA_ARRANGER
    arr_config_t arrangement;
#endif
    uint32_t lights;                               /* SLOOP 2.3: the backlight (panel.c lights_word); appended,
                                                    * so 2.2 still reads its part (st_load cuts at its size) */
} persist_t;
#define PERSIST_SIZE_V22 __builtin_offsetof(persist_t, lights)   /* the settings as 2.2 wrote them (no lights) */
_Static_assert(sizeof(persist_t) == PERSIST_SIZE_V22 + 4u, "lights: the last word, no padding before it");
#if FELUCCA_ARRANGER
#define PERSIST_MAGIC 0x50455233u                  /* "PER3": includes the song order */
#else
#define PERSIST_MAGIC 0x50455232u
#endif
#if FELUCCA_FLASH
static persist_t persist_saved;
#endif

/* the DX7 user banks: 8 in flash, each two storage objects of 16 voices (both valid = a bank); the one in use is
 * dx_user (eng_dx7.c), its number in OBJ_DXMETA. Like a DX7's cartridges: switching loads 4 KB */
#define DX_BANK_HALF (sizeof dx_user / 2u)
#define DX_META_MAGIC 0x31424458u                 /* "DXB1" */
#if FELUCCA_FLASH
static int dx_bank_read(uint32_t k)              /* bank k of flash -> dx_user: 1 a bank, 0 none */
{
    uint8_t *b = &dx_user[0][0];
    uint32_t o = OBJ_DXBANK0 + 2u * (k % DX_NBANKS);
    if (flash_ok && st_load(o, b, DX_BANK_HALF) == (int)DX_BANK_HALF &&
        st_load(o + 1u, b + DX_BANK_HALF, DX_BANK_HALF) == (int)DX_BANK_HALF) {
        dx_user_ok = 1;
        dx_bank_names();
        return 1;
    }
    dx_bank_clear();
    return 0;
}
static void ukit_boot(void)                       /* MY KIT from flash (else DX KIT) */
{
    if (!(flash_ok && st_load(OBJ_DXKIT, &ukit_img, sizeof ukit_img) == (int)sizeof ukit_img && !ukit_from_img(&ukit_img)))
        ukit_from(0);
}
static void dx_bank_boot(void)
{
    uint32_t m[2] = {0, 0};
    dx_bank_cur = 0;
    if (flash_ok && st_load(OBJ_DXMETA, m, sizeof m) == (int)sizeof m && m[0] == DX_META_MAGIC && m[1] < DX_NBANKS)
        dx_bank_cur = (uint8_t)m[1];
    dx_bank_read(dx_bank_cur);
}
#endif
static int dx_bank_store(void)                    /* the bank in RAM -> its flash slot: 0 ok, 2 flash error / no flash */
{
#if FELUCCA_FLASH
    const uint8_t *b = &dx_user[0][0];
    uint32_t o = OBJ_DXBANK0 + 2u * dx_bank_cur;
    if (!flash_ok)
        return 2;
    if (st_save(o, b, DX_BANK_HALF) || st_save(o + 1u, b + DX_BANK_HALF, DX_BANK_HALF))
        return 2;
    return 0;
#else
    return 2;
#endif
}
static int ukit_store(void)                       /* MY KIT (drums.c ukit) -> flash: 0 ok, 2 flash error / no flash */
{
#if FELUCCA_FLASH
    if (!flash_ok)
        return 2;
    ukit_to_img(&ukit_img);
    return st_save(OBJ_DXKIT, &ukit_img, sizeof ukit_img) ? 2 : 0;
#else
    return 2;
#endif
}
static int dx_bank_erase(void)                    /* no bank in the slot in use, in RAM and in flash: 0 ok, 2 flash error */
{
    dx_bank_clear();
#if FELUCCA_FLASH
    if (flash_ok) {
        static const uint8_t none[4] = {0, 0, 0, 0};
        uint32_t o = OBJ_DXBANK0 + 2u * dx_bank_cur;
        return st_save(o, none, 4) || st_save(o + 1u, none, 4) ? 2 : 0;
    }
#endif
    return 0;
}
/* another bank in use (the voice list's Bank row, the editor's BANK_SELECT): U01..U32 play it from now on.
 * Unstored edits of the bank left are dropped (as switching a DX7's cartridge). 0 ok, 1 no such bank */
static int dx_bank_select(uint32_t k)
{
    if (k >= DX_NBANKS)
        return 1;
    if (k == dx_bank_cur)
        return 0;
    dx_bank_cur = (uint8_t)k;
#if FELUCCA_FLASH
    dx_bank_read(k);
    if (flash_ok) {
        uint32_t m[2] = {DX_META_MAGIC, k};
        st_save(OBJ_DXMETA, m, sizeof m);
    }
#else
    dx_bank_clear();
#endif
    return 0;
}

_Static_assert(sizeof(side_rec_t) == 80u && sizeof(side_rec_t) <= ST_SIDE_MAX, "the side record: theme, favourites");
static side_rec_t side_out;
#if FELUCCA_FLASH
/* 4.0 (SLOOP 2.6 / 2.7): the settings object holds, after persist_t, the MIDI part: the MIDI LEARN map (seq.c ml_tab,
 * ML_N entries, 2 bytes each, low first), then GLO > MIDI: CH1 CH2 CH3, the drum channel | OTHER << 7, MIDI 2's BEND,
 * MIDI 3's FX CH, DR MAP | CLK OUT << 1, REMOTE. Written only while it is not the default (the object is then 3.x's).
 * The theme and the favourites ride in the side record of the same copy (storage.c st_side, ui.c side_rec_t). (SLOOP
 * keeps its SYN drum kits between the two: not here) */
#define MIDI_PART (2u * ML_N + 8u)
_Static_assert(sizeof(persist_t) + MIDI_PART <= ST_PAYLOAD_MAX, "the settings object fits its sector");
static uint8_t midi_saved[MIDI_PART];               /* the part as flash has it (settings_save: written when it differs) */
UI_SMALL static void midi_part_fill(uint8_t *b)
{
    uint32_t k;
    for (k = 0; k < ML_N; k++)
        b[2u * k] = (uint8_t)ml_tab[k], b[2u * k + 1u] = (uint8_t)(ml_tab[k] >> 8);
    b[2u * ML_N] = midi_set[0], b[2u * ML_N + 1u] = midi_set[1], b[2u * ML_N + 2u] = midi_set[2];
    b[2u * ML_N + 3u] = (uint8_t)(midi_set[3] | midi_set[4] << 7);
    b[2u * ML_N + 4u] = midi_set[5];
    b[2u * ML_N + 5u] = midi_set[6];
    b[2u * ML_N + 6u] = (uint8_t)(midi_set[7] | midi_set[9] << 1);
    b[2u * ML_N + 7u] = midi_set[8];
}
UI_SMALL static int midi_part_default(const uint8_t *b)
{
    static const uint8_t def[10] = MIDI_SET_DEF;
    uint32_t k;
    for (k = 0; k < 2u * ML_N; k++)
        if (b[k])
            return 0;
    return b[2u * ML_N] == def[0] && b[2u * ML_N + 1u] == def[1] && b[2u * ML_N + 2u] == def[2] &&
           b[2u * ML_N + 3u] == (uint8_t)(def[3] | def[4] << 7) && b[2u * ML_N + 4u] == def[5] && b[2u * ML_N + 5u] == def[6] &&
           b[2u * ML_N + 6u] == (uint8_t)(def[7] | def[9] << 1) && b[2u * ML_N + 7u] == def[8];
}
/* every entry checked (a CC that learns, a parameter this knows, a channel 0..16, a range 1..12) */
UI_SMALL static void midi_part_read(const uint8_t *b)
{
    uint32_t k, e;
    fm1_irq_off();                                  /* (the ISR reads the map and the channels: never half of them) */
    for (k = 0; k < ML_N; k++) {
        e = (uint32_t)b[2u * k] | (uint32_t)b[2u * k + 1u] << 8;
        ml_tab[k] = (uint16_t)((e >> 9) && ml_free_cc(e & 127u) && ml_id(e >> 9) < P_COUNT ? e : 0u);
    }
    for (k = 0; k < 3u; k++)
        midi_set[k] = (uint8_t)(b[2u * ML_N + k] <= 16u ? b[2u * ML_N + k] : k + 1u);
    midi_set[3] = (uint8_t)((b[2u * ML_N + 3u] & 31u) <= 16u ? b[2u * ML_N + 3u] & 31u : 10u);
    midi_set[4] = (uint8_t)(b[2u * ML_N + 3u] >> 7);
    midi_set[5] = (uint8_t)(b[2u * ML_N + 4u] >= 1u && b[2u * ML_N + 4u] <= 12u ? b[2u * ML_N + 4u] : 2u);
    midi_set[6] = (uint8_t)(b[2u * ML_N + 5u] <= 16u ? b[2u * ML_N + 5u] : 0u);
    midi_set[7] = (uint8_t)(b[2u * ML_N + 6u] <= 3u ? b[2u * ML_N + 6u] & 1u : 0u);
    midi_set[9] = (uint8_t)(b[2u * ML_N + 6u] <= 3u ? b[2u * ML_N + 6u] >> 1 : 0u);
    midi_set[8] = (uint8_t)(b[2u * ML_N + 7u] <= 16u ? b[2u * ML_N + 7u] : 0u);
    song.g[G_CH1] = midi_set[0], song.g[G_CH2] = midi_set[1], song.g[G_CH3] = midi_set[2];
    song.g[G_DRCH] = midi_set[3], song.g[G_CHOTH] = midi_set[4], song.g[G_BEND] = midi_set[5];
    song.g[G_FXCH] = midi_set[6], song.g[G_DRMAP] = midi_set[7], song.g[G_CLKOUT] = midi_set[9];
    song.g[G_RMCH] = midi_set[8];
    fm1_irq_on();
}
UI_SMALL static int settings_write(const persist_t *p)
{
    uint8_t mp[MIDI_PART];
    int rc;
    midi_part_fill(mp);
    side_fill(&side_out);
    st_side = &side_out;
    st_side_len = sizeof side_out;
    rc = st_save2(OBJ_SETTINGS, p, sizeof *p, mp, midi_part_default(mp) ? 0u : sizeof mp);
    st_side = 0;
    if (rc)
        return -1;
    fav_dirty = 0;
    memcpy(midi_saved, mp, sizeof midi_saved);
    return 0;
}
static void midi_part_boot(void)                    /* the MIDI part of the settings object, else the defaults */
{
    st_hdr_t h;
    uint32_t k;
    for (k = 0; k < ML_N; k++)
        ml_tab[k] = 0;
    if (st_current(OBJ_SETTINGS, &h) >= 0 && h.len == sizeof(persist_t) + MIDI_PART)
        midi_part_read(st_buf + sizeof(persist_t));
    midi_part_fill(midi_saved);
}
#endif

static void persist_boot(void)                    /* before settings_init / panel_init */
{
#if FELUCCA_ARRANGER
    arr_defaults(&arrangement);
#endif
#if FELUCCA_FLASH
    persist_t p;
    uint32_t f = irq_save();
    flash_ok = FL_JEDEC_OK(FL_FAR(fl_jedec_ram)());       /* the expected 1 MiB part, else stay RAM-only */
    irq_restore(f);
    if (!flash_ok)
        return;
    fl_plain_window_init();                        /* flash above 0x93000 reads as plaintext through XIP
                                                    * (user sample sets are played from there) */
    {
        int n = st_load(OBJ_SETTINGS, &p, sizeof p);
        if (n == (int)PERSIST_SIZE_V22 && p.magic == PERSIST_MAGIC)
            p.lights = 0;                          /* from 2.2: backlight off */
        if (((n == (int)sizeof p || n == (int)PERSIST_SIZE_V22) && p.magic == PERSIST_MAGIC)
#if FELUCCA_ARRANGER
            || (n == (int)(16u + sizeof(panel_t)) && p.magic == 0x50455232u)
#endif
            ) {
            settings.magic = SETTINGS_MAGIC;
            settings.palette = p.palette;
            settings.lowcut = p.lowcut;
            settings.zoom = p.zoom;
            if (p.panel.magic == PANEL_MAGIC)
                panel = p.panel;
            if (p.magic == PERSIST_MAGIC)
                lights_from_word(p.lights);
            else
                p.lights = 0;
#if FELUCCA_ARRANGER
            if (p.magic == PERSIST_MAGIC && arr_valid(&p.arrangement, 15u))
                arrangement = p.arrangement;
            else
                p.arrangement = arrangement;
#endif
            persist_saved = p;
            {   /* 4.0 (SLOOP 2.7): the theme and the favourites, in the side record of the same copy */
                side_rec_t r;
                if (st_side_load(OBJ_SETTINGS, &r, sizeof r) == 0)
                    side_take(&r);
            }
        } else if (n == (int)(8u + sizeof(panel_t)) && p.magic == 0x50455231u) {   /* "PER1": palette, panel */
            const uint32_t *w = (const uint32_t *)&p;
            panel_t old;
            memcpy(&old, w + 2, sizeof old);
            settings.magic = SETTINGS_MAGIC;
            settings.palette = w[1];
            settings.lowcut = 0;
            settings.zoom = 0;
            if (old.magic == PANEL_MAGIC)
                panel = old;
        }
    }
    {   /* projects: fill empty RAM slots from flash, so the slot list is right after power-on. A slot
         * still valid in RAM (a warm reset: an update, UPDATE MODE, a crash) may never have reached
         * flash (a live section stored while playing): marked to be written when quiet */
        uint32_t i;
        for (i = 0; i < 4u; i++)
            if (!proj_ok(&proj_slot[i])) {
                proj_fetch(i);
            } else {
                int n = st_load(OBJ_PROJECT0 + i, &proj_tmp, sizeof proj_tmp);
                if (n != (int)sizeof proj_slot[i] || memcmp(&proj_tmp.v4, &proj_slot[i], sizeof proj_slot[i]))
                    sec_dirty |= (uint8_t)(1u << i);
            }
    }
    midi_part_boot();                              /* 4.0: GLO > MIDI and MIDI LEARN (after the settings) */
    up_boot();                                     /* user presets */
    dx_bank_boot();                                /* the DX7 user bank */
    ukit_boot();                                   /* MY KIT */
#endif
}

static int project_used(uint32_t slot) { return proj_ok(&proj_slot[slot & 3u]); }

static void persist_fill(persist_t *p)              /* the settings as they are now */
{
    memset(p, 0, sizeof *p);
    p->magic = PERSIST_MAGIC;
    p->palette = settings.palette;
    p->lowcut = settings.lowcut;
    p->zoom = settings.zoom;
    p->panel = panel;
    p->lights = lights_word();
#if FELUCCA_ARRANGER
    p->arrangement = arrangement;
#endif
}

static void settings_save(void)
{
#if FELUCCA_FLASH
    persist_t p;
    if (!flash_ok)
        return;
    persist_fill(&p);
    {
        uint8_t mp[MIDI_PART];
        midi_part_fill(mp);
        if (!memcmp(&p, &persist_saved, sizeof p) && !memcmp(mp, midi_saved, sizeof mp) && !fav_dirty)
            return;                                /* unchanged: no erase cycle */
    }
    if (settings_write(&p) == 0)
        persist_saved = p;
#endif
}

#if FELUCCA_FLASH
_Static_assert(sizeof(project_t) <= ST_PAYLOAD_MAX, "project does not fit one flash sector");

/* ---- backup restore (editor.c BK_PUT): each object checked as a load checks it, then written through the
 * same A/B commit as a save. rc: 0 ok, 2 not a valid object, 3 stop the song first, 4 flash */
static int panel_valid(const panel_t *q)           /* a permutation of the buttons and of the knobs */
{
    uint32_t i, b = 0, e = 0;
    if (q->magic != PANEL_MAGIC)
        return 0;
    for (i = 0; i < NB; i++) {
        if (q->btn[i] >= 14u || (b >> q->btn[i]) & 1u)
            return 0;
        b |= 1u << q->btn[i];
    }
    for (i = 0; i < NE; i++) {
        if (q->enc[i] >= 7u || (e >> q->enc[i]) & 1u || (q->dir[i] != 1 && q->dir[i] != -1))
            return 0;
        e |= 1u << q->enc[i];
    }
    return 1;
}

static uint32_t settings_restore(const void *raw, uint32_t n)
{
    persist_t p;
    if (n != sizeof p && n != PERSIST_SIZE_V22)
        return 2;
    memset(&p, 0, sizeof p);
    memcpy(&p, raw, n);
    if (p.magic != PERSIST_MAGIC || p.palette >= NPALETTES || p.lowcut > 1u || p.zoom > 1u || !panel_valid(&p.panel))
        return 2;
#if FELUCCA_ARRANGER
    if (!arr_valid(&p.arrangement, 15u))
        return 2;
#endif
    if (!flash_ok || settings_write(&p))
        return 4;
    persist_saved = p;
    settings.palette = p.palette;
    settings.lowcut = p.lowcut;
    settings.zoom = p.zoom;
    panel = p.panel;
#if FELUCCA_ARRANGER
    arrangement = p.arrangement;
#endif
    lights_from_word(p.lights);
    song.g[G_SYNC] = (int16_t)lights_sync;
    song.g[G_MIDI] = (int16_t)lights_mout;
    song.g[G_ROUTE] = (int16_t)lights_min;
    palette_set(settings.palette);
    fx_lowcut = (uint8_t)(settings.lowcut != 0);
    ui.force = 1;
    return 0;
}

/* slot 0..3 (n 0: empty), or 4: the working project (loaded now) */
static uint32_t project_restore(uint32_t slot, const void *raw, uint32_t n)
{
    if (song.playing || transport_req)
        return 3;
    if (slot < 4u && !n) {
        if (!flash_ok || st_save(OBJ_PROJECT0 + slot, raw, 0))
            return 4;
        memset(&proj_slot[slot], 0, sizeof proj_slot[slot]);
        sec_dirty &= (uint8_t)~(1u << slot);
        return 0;
    }
    if (!proj_import(&autosave_buf, raw, (int)n))
        return 2;
    if (slot == 4u) {
        project_apply(&autosave_buf);
        return 0;
    }
    if (!flash_ok || st_save(OBJ_PROJECT0 + slot, &autosave_buf, sizeof autosave_buf))
        return 4;
    memcpy(&proj_slot[slot], &autosave_buf, sizeof proj_slot[slot]);
    sec_dirty &= (uint8_t)~(1u << slot);
    return 0;
}
#endif
#if FELUCCA_ARRANGER
static void arrangement_save(void)
{
    if (song.playing || transport_req) { ui_message("STOP BEFORE SAVE"); return; }
    settings_save();
#if FELUCCA_FLASH
    if (flash_ok) {
        ui_message(memcmp(&persist_saved.arrangement, &arrangement, sizeof arrangement) ? "SAVE ERROR" : "SONG SAVED");
        return;
    }
#endif
    ui_message("SONG IN RAM ONLY");
}

/* ---- live sections (SAVE + key, ui_layers.c). A section is a project slot (A..D = 1..4): stored into RAM
 * at once (playing too), written to flash once the transport is stopped and nothing sounds (an erase
 * stops the audio for ~50 ms); a song recorded with SONG REC is saved the same way. */
static void section_store(uint32_t s)
{
    s &= 3u;
    fm1_irq_off();                                      /* (the audio ISR may be applying a section) */
    proj_capture(&proj_slot[s]);
    live_sec = (int8_t)s;
    fm1_irq_on();
    sec_dirty |= (uint8_t)(1u << s);
}
static void section_load(uint32_t s)                    /* stopped: the section is the loop now */
{
    s &= 3u;
    project_apply(&proj_slot[s]);
    live_sec = (int8_t)s;
}
static void sections_write(void)                        /* the dirty sections and song into flash */
{
    uint32_t i;
#if FELUCCA_FLASH
    if (flash_ok)
        for (i = 0; i < 4u; i++)
            if (((sec_dirty >> i) & 1u) && st_save(OBJ_PROJECT0 + i, &proj_slot[i], sizeof proj_slot[i]) == 0)
                sec_dirty &= (uint8_t)~(1u << i);       /* (a failed write stays dirty: tried again later) */
    if (!flash_ok)
#endif
        sec_dirty = 0;
    (void)i;
    if (song_dirty) {
        song_dirty = 0;
        settings_save();
    }
}
/* before an intentional reset (an update, UPDATE MODE, UBOOT from the host): the audio is stopped, so
 * whatever is only in RAM goes to flash now: the live sections, the song, the working project */
static void persist_flush_now(void)
{
    sections_write();
#if FELUCCA_FLASH
    if (flash_ok && !arrangement_clock.running) {     /* (a song playing: the tracks hold a section) */
        proj_capture(&autosave_buf);
        if (autosave_buf.sum != autosave_hash && st_save(OBJ_AUTOSAVE, &autosave_buf, sizeof autosave_buf) == 0)
            autosave_hash = autosave_buf.sum;
    }
#endif
}
static void sections_flush(void)                        /* main loop */
{
    static uint32_t tried;
    if (srec_done) {
        song_dirty = srec_done != 0xFFu;
        if (song_dirty) {
            char b[8];
            fmt_int(b, srec_done);
            ui_say("SONG PARTS ", b);
        } else {
            ui_message("NO SONG");
        }
        srec_done = 0;
    }
    if ((uint32_t)song.g[G_SYNC] != lights_sync) {      /* GLO > SYSTEM > SYNC: kept with the settings */
        lights_sync = (uint8_t)song.g[G_SYNC];
        settings_later = 1;
    }
    if ((uint32_t)(song.g[G_MIDI] != 0) != lights_mout) {   /* GLO > SYSTEM > MIDI: the same (SLOOP 2.4) */
        lights_mout = (uint8_t)(song.g[G_MIDI] != 0);
        settings_later = 1;
    }
    if ((uint32_t)(song.g[G_ROUTE] != 0) != lights_min) {   /* GLO > SYSTEM > IN: the same */
        lights_min = (uint8_t)(song.g[G_ROUTE] != 0);
        settings_later = 1;
    }
    if (settings_later) {                               /* the menu closed while playing */
        settings_later = 0;
        song_dirty = 1;                                 /* (settings_save when quiet, with the song) */
    }
    if ((!sec_dirty && !song_dirty) || song.playing || transport_req || !audio_quiet() || fm1_ms - ui_input_ms < 1500u ||
        fm1_ms - tried < 5000u)
        return;
    tried = fm1_ms;                                     /* (a failed write: again in 5 s, not every frame) */
    sections_write();
    if (sec_dirty)
        ui_message("SAVE ERROR: RETRYING");
}
#endif
#endif /* PROJ_HOST */
