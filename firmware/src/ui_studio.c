/* SPDX-License-Identifier: GPL-3.0-only */
/* LIVE screens, 240 x 240, in a teenage-engineering-like style: black, four colours (one per
 * track and per knob: blue, green, yellow, orange), white for what you touch, red for recording,
 * big numbers, lowercase labels, four dials at the bottom that show what KNOB 1..4 do.
 * Screens: TRACKS (the performance view), DRUMS (GRID / KIT, pads that flash on each hit), the
 * LAYERS (a function button held: what the 16 white keys and the knobs do now), HOLD (a hold to
 * confirm: clear, save), REC (armed / free take). Every band is drawn into the canvas only when its
 * signature changed. */
static uint8_t drum_page, drum_lane, drum_cursor;   /* drum_page: 0 GRID, 1 KIT, 2 LANE (the lane's macros) */
static uint8_t dm_pg, kit_save_arm;                 /* LANE: the macro page (0..2); SAVE pressed once on KIT / LANE */
static uint32_t lane_t0, kit_save_ms;               /* when LANE opened; when SAVE was pressed once */
static void trk_short_name(uint32_t c, char *b);
static int ukit_store(void);                        /* project.c: MY KIT to flash, 0 ok */
static int on_drum_page(void) { return !ui.home && cur_page()->scope == SC_DRUM; }

/* ---------------------------------------------------------------- style --- */
#define TE_G1 RGB(40, 36, 37)            /* tiles (the DX7's warm panel greys) */
#define TE_G2 RGB(70, 64, 65)            /* empty steps, dial tracks */
#define TE_G3 RGB(140, 132, 128)         /* labels */
#define TE_G4 RGB(206, 200, 194)         /* secondary text */
#define TE_RED RGB(255, 44, 52)          /* recording, erasing */
/* sloopDX: the DX7's panel colours, one per track: cyan, light blue, pink, and beige for the drums */
static const uint16_t TE_COL[4] = {RGB(66, 245, 245), RGB(175, 217, 244), RGB(244, 192, 203), RGB(230, 209, 185)};
static const uint16_t TE_MID[4] = {RGB(44, 163, 163), RGB(117, 145, 163), RGB(163, 128, 135), RGB(153, 139, 123)};
static const uint16_t TE_DIM[4] = {RGB(22, 82, 82), RGB(58, 72, 81), RGB(81, 64, 68), RGB(77, 70, 62)};
#define TE_DRUM TE_COL[3]

static void te_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)     /* filled circle */
{
    int32_t y, x;
    for (y = -r; y <= r; y++)
        for (x = -r; x <= r; x++)
            if (x * x + y * y <= r * r + r)
                cv_pset(cx + x, cy + y, c);
}
/* a dial: a 270-degree ring (lit up to the value), a pointer; ratio 0..1000, -1 = a plain ring */
static void te_dial(int32_t cx, int32_t cy, int32_t r, int32_t ratio, uint16_t c, uint16_t dim)
{
    int32_t i, end = ratio < 0 ? 768 : ratio * 768 / 1000;
    for (i = 0; i <= 768; i += 8) {
        uint32_t a = (uint32_t)(384 + i) & 1023u;
        int32_t co = SINE[(a + 256u) & 1023u], si = SINE[a], k;
        uint16_t col = ratio < 0 || i <= end ? c : dim;
        for (k = r - 2; k <= r; k++)
            cv_pset(cx + ((co * k) >> 15), cy + ((si * k) >> 15), col);
    }
    if (ratio >= 0) {
        uint32_t a = (uint32_t)(384 + end) & 1023u;
        int32_t co = SINE[(a + 256u) & 1023u], si = SINE[a];
        cv_line(cx, cy, cx + ((co * (r - 4)) >> 15), cy + ((si * (r - 4)) >> 15), C_WHITE);
        te_disc(cx, cy, 2, C_WHITE);
    }
}
static void te_play_icon(int32_t x, int32_t y, int playing)
{
    int32_t i;
    if (playing)
        for (i = 0; i < 12; i++)
            cv_rect(x + i, y + i / 2, 1, 14 - i, C_WHITE);   /* a triangle */
    else
        cv_rect(x, y + 1, 12, 12, TE_G3);
}
static uint32_t studio_hash(uint32_t h, const char *p)
{ while (*p) h = h * 31u + (uint8_t)*p++; return h; }
static void te_lower(char *d, const char *s, uint32_t n)
{
    uint32_t i;
    for (i = 0; i + 1u < n && s[i]; i++)
        d[i] = s[i] >= 'A' && s[i] <= 'Z' ? (char)(s[i] + 32) : s[i];
    d[i] = 0;
}
static void te_text_c(int32_t cx, int32_t y, const char *s, uint16_t c)   /* centred on cx */
{
    cv_text(cx - text_w(&FONT_S, s) / 2, y, &FONT_S, s, c);
}

/* the dial strip: KNOB 1..4, label + value under each (a dial with no label: an empty column); a
 * message replaces it */
static void te_dials(int32_t y0, const char *const lab[4], const char *const val[4], const int32_t ratio[4],
                     uint32_t sig, uint32_t *cache)
{
    uint32_t k;
    sig = studio_hash(sig, ui.msg_t ? ui.msg : "");
    for (k = 0; k < 4u; k++) {
        sig = studio_hash(sig * 7u + (uint32_t)ratio[k] + (ui.hot_t && ui.hot_col == k) * 5003u, lab[k]);
        sig = studio_hash(sig, val[k]);
    }
    if (!ui.force && sig == *cache)
        return;
    *cache = sig;
    cv_begin(240, ui.msg_t ? 240u - (uint32_t)y0 : 40u, C_BLACK);
    if (ui.msg_t) {                                     /* a message: a white bar */
        cv_rect(0, 10, 240, 32, C_WHITE);
        cv_text((240 - text_w(&FONT_S, ui.msg)) / 2, 18, &FONT_S, ui.msg, C_BLACK);
        cv_blit(0, (uint32_t)y0);
        return;
    }
    for (k = 0; k < 4u; k++) {
        int32_t cx = 30 + 60 * (int32_t)k;
        if (!lab[k][0])
            continue;
        te_dial(cx, 12, 11, ratio[k], TE_COL[k], TE_DIM[k]);
        te_text_c(cx, 24, lab[k], TE_G3);
    }
    cv_blit(0, (uint32_t)y0);
    cv_begin(240, 16, C_BLACK);                         /* the values: white while turned */
    for (k = 0; k < 4u; k++)
        te_text_c(30 + 60 * (int32_t)k, 0, val[k], ui.hot_t && ui.hot_col == k ? C_WHITE : TE_G4);
    cv_blit(0, (uint32_t)y0 + 40u);
}

static void studio_open(uint32_t scope)
{
    uint32_t i;
    if (scope == SC_SONG && rec_wait)
        rec_wait = 0;                                   /* (an arm does not follow into the song page) */
    for (i = 0; i < NPAGES; i++) if (PAGES[i].scope == scope) {
        ui.page = (uint8_t)i; ui.home = 0; song.seq_mode = 0; ui.force = 1;
        ui.msg_t = 0; ui.entry_open = 0; return;
    }
}
/* LIVE: REC, on any page. One record arm, on the selected track (it follows ALGORITHM). No
 * click (seq.c): playing, recording starts at once; stopped, REC arms and the first note
 * starts the loop (or, in an empty project, a free take that REC closes: seq.c ft_close).
 * REC again stops recording / cancels the arm (playback continues). */
static void rec_toggle(void)
{
    if (ft_owns_press())
        return;
    if (song.playing && arrangement_enabled) {
        ui_message("STOP THE SONG FIRST");
        return;
    }
    if (song.rec || rec_wait) {
        song.rec = 0;
        rec_wait = 0;
        ui_message("REC OFF");
        return;
    }
    arrangement_enabled = 0;
    if (song.playing)
        rec_begin();                    /* playing: record now */
    else
        rec_wait = 1;                   /* stopped: the first note starts it */
}

/* the loop position of track t: "bar.beat" in its pattern (1.1 .. ) */
static void loop_pos(const track_t *t, char *b)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), s = t->seq_idx % len, k;
    fmt_int(b, (int32_t)(s / 16u + 1u));
    k = str_len(b);
    b[k] = '.';
    b[k + 1] = (char)('1' + (s % 16u) / 4u);
    b[k + 2] = 0;
}

/* the header of the live screens: BPM, transport, the loop position, the beat lights, REC */
static void te_header(const char *title, uint16_t tc, uint32_t *cache)
{
    char b[12];
    uint32_t beat = clk_beat, k, playing = song.playing;
    uint32_t sig = studio_hash((uint32_t)song.g[G_BPM] * 7u + playing * 3u + (song.rec != 0) * 1999u +
                               (playing ? beat * 131u + TSEL->seq_idx * 7919u : 0u) + (uint32_t)song.g[G_CLOCK] * 77u +
                               arrangement_enabled * 5u + (ui.bpm_t != 0) * 104729u + song.solo * 37u, title);
    sig = sig * 31u + tc;
    if (!ui.force && sig == *cache)
        return;
    *cache = sig;
    cv_begin(240, 40, C_BLACK);
    fmt_int(b, song.g[G_BPM]);
    cv_text(4, 4, &FONT_L, b, ui.bpm_t ? C_WHITE : TE_G4);
    cv_text(4 + text_w(&FONT_L, b) + 4, 20, &FONT_S, "bpm", TE_G3);
    te_play_icon(104, 6, (int)playing);
    if (playing) {                                     /* bar.beat in the selected track's loop */
        loop_pos(TSEL, b);
        cv_text(122, 5, &FONT_S, b, C_WHITE);
    } else {
        cv_text(122, 5, &FONT_S, arrangement_enabled ? "song" : "loop", TE_G3);
    }
    for (k = 0; k < 4u; k++)                           /* the four beats of the bar */
        cv_rect(104 + (int32_t)k * 9, 26, 7, 7, playing && beat % 4u == k ? (k ? C_WHITE : TE_RED) : TE_G2);
    if (song.rec) {
        te_disc(224, 12, 8, TE_RED);
        cv_text(176, 4, &FONT_S, "rec", TE_RED);
        if (song.g[G_CLOCK] != 0)                       /* the click is on */
            cv_text(168, 22, &FONT_S, "click", TE_G3);
    } else {
        cv_text(240 - text_w(&FONT_S, title) - 4, 5, &FONT_S, title, tc);
        if (song.solo)
            cv_text(236 - text_w(&FONT_S, "solo"), 22, &FONT_S, "solo", C_WHITE);
    }
    cv_rect(0, 39, 240, 1, TE_G1);
    cv_blit(0, 0);
}

/* --------------------------------------------------------------- TRACKS --- */
/* swing, MPC style: "62%" */
static void swing_str(char *b, int32_t v)
{
    fmt_int(b, 50 + (clamp(v, 0, 100) + 2) / 4);
    str_cpy(b + str_len(b), "%", 2);
}

static void studio_tracks_draw(void)
{
    static uint32_t head, rows[NTRK], footer;
    uint32_t i, j;
    te_header("tracks", TE_G3, &head);
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        char b[24], e[16];
        uint32_t selected = song.sel == i, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
        uint32_t level = i == TRK_DRUM ? song.g[G_DRLVL] : t->p[P_LEVEL];
        uint32_t silent = trk_silent(t) || !level, pos = t->seq_idx % len, rec = (song.rec >> i) & 1u;
        uint32_t solo = (song.solo >> i) & 1u, bank = song.playing ? pos / 16u : 0u, h;
        uint16_t col = TE_COL[i], dim = TE_DIM[i];
        if (i == TRK_DRUM) {
            str_cpy(b, DRUM_KIT_NAMES[drum_kit()], sizeof b);
            te_lower(e, "drums", sizeof e);
        } else {
            trk_short_name(i, b);
            te_lower(e, ENGINES[t->eng_req % NENGINES]->name, sizeof e);
        }
        b[13] = 0;
        h = studio_hash(selected + level * 7u + silent * 997u + rec * 1999u + solo * 4999u + len * 37u +
                        song.playing * 7u + pos * 71u + bank * 13u, b);
        for (j = 0; j < len; j++) h = h * 31u + (uint32_t)trk_step_on(t, j);
        if (!ui.force && h == rows[i]) continue;
        rows[i] = h;
        cv_begin(240, 36, C_BLACK);
        cv_rect(2, 3, 26, 30, selected ? col : dim);   /* the track tile */
        {
            char n[2] = {(char)('1' + i), 0};
            cv_text(11, 10, &FONT_S, n, selected ? C_BLACK : col);
        }
        cv_text(34, 1, &FONT_S, b, selected ? C_WHITE : TE_G4);
        if (34 + text_w(&FONT_S, b) + 6 + text_w(&FONT_S, e) < 196)
            cv_text(34 + text_w(&FONT_S, b) + 6, 1, &FONT_S, e, selected ? col : TE_G3);
        if (rec) {
            cv_rect(200, 2, 36, 15, TE_RED);
            cv_text(206, 1, &FONT_S, "rec", C_WHITE);
        } else if (solo) {
            cv_rect(200, 2, 36, 15, C_WHITE);
            cv_text(202, 1, &FONT_S, "solo", C_BLACK);
        } else if (silent) {
            cv_text(204, 1, &FONT_S, "mute", TE_G3);
        }
        for (j = 0; j < 16u; j++) {                    /* the 16 steps in view */
            uint32_t p = bank * 16u + j, on = 0;
            if (len <= 16u) {
                if (p < len) on = trk_step_on(t, p) ? 2u : 1u;
            } else {                                    /* > 16 and stopped: the whole pattern, folded */
                uint32_t a = j * len / 16u, z = (j + 1u) * len / 16u, k;
                if (song.playing) {
                    if (p < len) on = trk_step_on(t, p) ? 2u : 1u;
                } else {
                    on = 1;
                    for (k = a; k < z; k++) if (trk_step_on(t, k)) on = 2;
                }
            }
            cv_rect(34 + (int32_t)j * 10, 21, 8, 9,
                    !on ? C_BLACK : on == 2u ? (silent ? TE_G3 : selected ? col : dim) : TE_G1);
            if (on == 2u && !selected && !silent)
                cv_rect(34 + (int32_t)j * 10, 21, 8, 2, col);
            if (song.playing && p == pos)
                cv_rect(34 + (int32_t)j * 10, 31, 8, 2, C_WHITE);
        }
        cv_rect(198, 23, 38, 5, TE_G1);                /* the level */
        if (!silent)
            cv_rect(198, 23, (int32_t)level * 38 / 127, 5, selected ? col : TE_G3);
        cv_blit(0, 40 + i * 36);
    }
    {   /* KNOB 1 swing (the global groove), 2 level, 3 steps, 4 pan of the selected track */
        track_t *t = TSEL;
        static char v[4][8];
        static const char *const lab[4] = {"swing", "level", "steps", "pan"};
        const char *val[4] = {v[0], v[1], v[2], v[3]};
        int32_t ratio[4];
        uint32_t lvl = is_drum(t) ? song.g[G_DRLVL] : t->p[P_LEVEL];
        swing_str(v[0], song.g[G_SWING]);
        fmt_int(v[1], t->p[P_MUTE] ? 0 : (int32_t)lvl * 100 / 127);
        fmt_int(v[2], t->p[P_SLEN]);
        fmt_int(v[3], t->p[P_PAN]);
        ratio[0] = song.g[G_SWING] * 10;
        ratio[1] = t->p[P_MUTE] ? 0 : (int32_t)lvl * 1000 / 127;
        ratio[2] = (t->p[P_SLEN] - 1) * 1000 / (NSTEP - 1);
        ratio[3] = (t->p[P_PAN] + 64) * 1000 / 127;
        te_dials(184, lab, val, ratio, song.sel, &footer);
    }
}

/* ---------------------------------------------------------------- DRUMS --- */
static const char *const LV_NAME[4] = {"norm", "ghost", "soft", "hard"};
static uint16_t lvl_col(uint32_t lvl)                   /* a hit's colour by its level */
{
    return lvl == LV_GHOST ? TE_DIM[3] : lvl == LV_SOFT ? TE_MID[3] : lvl == LV_HARD ? C_WHITE : TE_DRUM;
}
static uint16_t pad_lit[DRUM_LANES];                   /* pads and key LEDs: frames left lit */
static void pads_tick(void)                             /* once a frame: the hits since the last one */
{
    uint32_t i, hits;
    fm1_irq_off();
    hits = drums.hits;
    drums.hits = 0;
    fm1_irq_on();
    for (i = 0; i < DRUM_LANES; i++) {
        if ((hits >> i) & 1u) pad_lit[i] = 6;
        else if (pad_lit[i]) pad_lit[i]--;
        if (((hits >> i) & 1u) && drum_page == 2u && song.sel == TRK_DRUM && !song.playing)
            drum_lane = (uint8_t)i;                     /* LANE edits the sound last played (stopped: a key, a pad) */
    }
}

static void drum_screen_draw(void)
{
    static uint32_t head, title_sig, body_sig, footer;
    uint32_t i, j, len = (uint32_t)clamp(TDRUM->p[P_SLEN], 1, NSTEP), kit = drum_kit(), sig, bank;
    if (drum_cursor >= len) drum_cursor = (uint8_t)(len - 1u);
    bank = drum_cursor / 16u;
    {
        char st[16];
        te_lower(st, DRUM_KIT_STYLES[kit], sizeof st);
        if (kit == KIT_USER && ukit.seed) {             /* a dice kit: its seed, to roll it again */
            str_cpy(st, "#", sizeof st);
            fmt_int(st + 1, (int32_t)ukit.seed);
        }
        st[12] = 0;
        te_header(st, TE_DRUM, &head);
    }
    sig = kit * 131u + drum_page * 7u + drum_lane * 977u + bank * 31u + len + dm_pg * 7919u;   /* title band */
    if (ui.force || sig != title_sig) {
        char b[8];
        int lane = drum_page == 2u;
        title_sig = sig;
        cv_begin(240, 44, C_BLACK);
        cv_rect(2, 4, 34, 34, lane ? C_WHITE : TE_DRUM);
        fmt_int(b, lane ? (int32_t)drum_lane + 1 : (int32_t)kit + 1);
        cv_text(19 - text_w(&FONT_S, b) / 2, 13, &FONT_S, b, C_BLACK);
        cv_text(44, 6, &FONT_L, lane ? LANE_NAME[drum_lane] : DRUM_KIT_NAMES[kit], C_WHITE);
        if (lane) {                                    /* LANE: where you are, page n/3 (PRESETS turns it) */
            char pg[4] = {(char)('1' + dm_pg), '/', '3', 0};
            cv_text(198, 4, &FONT_S, "lane", C_WHITE);
            cv_text(204, 22, &FONT_S, pg, TE_DRUM);
        } else {
            cv_text(202, 4, &FONT_S, "grid", drum_page == 0 ? C_WHITE : TE_G3);
            cv_text(202, 22, &FONT_S, "kit", drum_page ? C_WHITE : TE_G3);
            cv_rect(196, drum_page ? 26 : 8, 3, 9, TE_DRUM);
        }
        cv_blit(0, 40);
    }
    sig = drum_page + drum_lane * 7u + drum_cursor * 101u + song.playing * 71u + len * 3u + dm_pg * 13u;
    if (song.playing) sig = sig * 31u + TDRUM->seq_idx;
    for (i = 0; i < DRUM_LANES; i++) sig = sig * 3u + (pad_lit[i] != 0);
    for (i = 0; i < len; i++) {
        const dstep_t *s = &TDRUM->dstep[i];
        sig = sig * 31u + dstep_mask(s);
        sig = sig * 31u + s->lvl[0] + s->lvl[1] * 7u + s->lvl[2] * 49u + s->lvl[3] * 343u;
        sig = sig * 31u + s->rat[0] + s->rat[1] * 7u + s->rat[2] * 49u + s->rat[3] * 343u;
    }
    if (ui.force || sig != body_sig) {
        body_sig = sig;
        cv_begin(240, 100, C_BLACK);
        if (!drum_page) {                              /* GRID: the 16 lanes x 16 steps of this bank */
            for (i = 0; i < DRUM_LANES; i++) {
                int32_t y = (int32_t)i * 6 + 2;
                cv_rect(2, y, 6, 5, pad_lit[i] ? C_WHITE : i == drum_lane ? TE_DRUM : TE_G2);
                for (j = 0; j < 16u; j++) {
                    uint32_t p = bank * 16u + j;
                    const dstep_t *s = &TDRUM->dstep[p < NSTEP ? p : 0];
                    uint16_t c;
                    if (p >= len) continue;
                    c = dstep_has(s, i) ? lvl_col(dstep_lvl(s, i)) : i == drum_lane ? TE_G2 : TE_G1;
                    if (song.playing && p == TDRUM->seq_idx && c == TE_G1) c = TE_G2;
                    cv_rect(12 + (int32_t)j * 14, y, 12, 5, c);
                    if (dstep_has(s, i) && dstep_rat(s, i)) {   /* a ratchet: a notch per extra hit */
                        uint32_t r;
                        for (r = 0; r < dstep_rat(s, i); r++)
                            cv_rect(13 + (int32_t)j * 14 + (int32_t)r * 3, y + 2, 2, 1, C_BLACK);
                    }
                    if (p == drum_cursor && i == drum_lane) {
                        cv_rect(11 + (int32_t)j * 14, y - 1, 14, 1, C_WHITE);
                        cv_rect(11 + (int32_t)j * 14, y + 5, 14, 1, C_WHITE);
                    }
                }
            }
        } else {                                       /* KIT: 16 pads, lit on each hit; LANE: the one edited framed */
            for (i = 0; i < DRUM_LANES; i++) {
                int32_t x = 2 + (int32_t)(i % 4u) * 60, y = (int32_t)(i / 4u) * 25;
                if (drum_page == 2u && i == drum_lane)
                    cv_rect(x - 1, y - 1, 58, 25, C_WHITE);
                cv_rect(x, y, 56, 23, pad_lit[i] ? TE_DRUM : drum_page == 2u && i == drum_lane ? TE_G2 : TE_G1);
                te_text_c(x + 28, y + 4, LANE_SHORT[i], pad_lit[i] ? C_BLACK : i == drum_lane ? C_WHITE : TE_G3);
            }
        }
        cv_blit(0, 84);
    }
    {
        static char v[4][12];
        const char *val[4] = {v[0], v[1], v[2], v[3]};
        static const char *const LG[4] = {"sound", "step", "hit", "level"}, *const LK[4] = {"kit", "level", "drive", "comp"};
        int32_t ratio[4];
        if (!drum_page) {
            const dstep_t *s = &TDRUM->dstep[drum_cursor];
            str_cpy(v[0], LANE_SHORT[drum_lane], 8);
            fmt_int(v[1], drum_cursor + 1);
            str_cpy(v[2], dstep_has(s, drum_lane) ? "on" : "--", 4);
            str_cpy(v[3], dstep_has(s, drum_lane) ? LV_NAME[dstep_lvl(s, drum_lane)] : "--", 8);
            ratio[0] = (int32_t)drum_lane * 1000 / (DRUM_LANES - 1);
            ratio[1] = (int32_t)drum_cursor * 1000 / (int32_t)(len > 1u ? len - 1u : 1u);
            ratio[2] = v[2][0] == 'o' ? 1000 : 0;
            ratio[3] = dstep_has(s, drum_lane) ? (int32_t)((dstep_lvl(s, drum_lane) + 1u) % 4u) * 333 : 0;
            te_dials(184, LG, val, ratio, 1u, &footer);
        } else if (drum_page == 2u) {                  /* LANE: four macros of the page (TUNE DECAY SWEEP BRIGHT,
                                                        * NOISE LEVEL PAN CHOKE, REV) */
            static const char *lab[4];
            uint32_t q;
            for (q = 0; q < 4u; q++) {
                uint32_t m = dm_pg * 4u + q;
                if (dm_pg == 2u && q >= 2u) {          /* page 3: the dice (GO, confirmed) */
                    lab[q] = q == 2u ? "kit" : "sound";
                    str_cpy(v[q], ui.arm == 0xD0u + q ? "again" : "dice", 8);
                    ratio[q] = ui.arm == 0xD0u + q ? 1000 : 0;
                } else if (m < DM_N) {
                    int32_t x = dm_get(drum_lane, m);
                    lab[q] = DM_DESC[m].name;
                    dm_format(m, x, v[q]);
                    ratio[q] = (x - DM_DESC[m].min) * 1000 / (DM_DESC[m].max - DM_DESC[m].min);
                } else {
                    lab[q] = "", v[q][0] = 0, ratio[q] = 0;
                }
            }
            te_dials(184, lab, val, ratio, 3u, &footer);
        } else {
            fmt_int(v[0], (int32_t)kit + 1);
            fmt_int(v[1], song.g[G_DRLVL] * 100 / 127);
            fmt_int(v[2], TDRUM->p[P_DIST] * 100 / 127);   /* the drum bus (drums.c drums_bus); reverb and pan: HOME */
            fmt_int(v[3], TDRUM->p[P_CHOR] * 100 / 127);
            ratio[0] = (int32_t)kit * 1000 / (int32_t)(DRUM_KITS - 1u);
            ratio[1] = song.g[G_DRLVL] * 1000 / 127;
            ratio[2] = TDRUM->p[P_DIST] * 1000 / 127;
            ratio[3] = TDRUM->p[P_CHOR] * 1000 / 127;
            te_dials(184, LK, val, ratio, 2u, &footer);
        }
    }
}

/* the level order of the knobs: ghost < soft < norm < hard */
static const uint8_t LV_UP[4] = {LV_GHOST, LV_SOFT, LV_NORM, LV_HARD};
static uint32_t lvl_rank(uint32_t lvl) { return lvl == LV_GHOST ? 0u : lvl == LV_SOFT ? 1u : lvl == LV_NORM ? 2u : 3u; }

/* the drum screen's buttons (tapped: ui_input.c layer_tap) */
static void drum_seq_tap(void)                      /* GRID <-> KIT (LANE -> GRID) */
{
    drum_page = drum_page ? 0u : 1u;
    ui.msg_t = 0;
    ui.force = 1;
}
static void drum_edit_tap(void)                     /* GRID -> KIT -> LANE -> KIT (the dice: LANE page 3, confirmed) */
{
    drum_page = drum_page == 1u ? 2u : 1u;
    if (drum_page == 2u)
        lane_t0 = fm1_ms;
    ui.msg_t = 0;
    ui.force = 1;
}
static int drum_save_tap(void)                      /* 1: GRID, the song page opened; KIT / LANE: SAVE twice stores MY KIT */
{
    if (!drum_page) {
        studio_open(SC_SONG);
        return 1;
    }
    if (!kit_save_arm || fm1_ms - kit_save_ms > 2500u) {
        kit_save_arm = 1, kit_save_ms = fm1_ms;
        ui_message("SAVE KIT: SAVE AGAIN");
    } else {
        int rc;
        kit_save_arm = 0;
        fm1_irq_off();
        ukit_bake(drum_kit());                      /* the kit with its macros -> MY KIT, the macros to 0 */
        TDRUM->p[P_E0] = KIT_USER;
        fm1_irq_on();
        rc = ukit_store();
        ui_message(rc ? "KIT SAVED (RAM)" : "KIT SAVED: MY KIT");
    }
    ui.force = 1;
    return 0;
}

static void drum_screen_input(uint32_t pressed, uint32_t home)
{
    uint32_t k, b;
    int32_t s;
    if (song.sel != TRK_DRUM || home == 1u) {
        go_home();
        return;
    }
    for (k = 0; k < NB; k++) if ((pressed >> panel.btn[k]) & 1u) {
        b = k;
        if (b == B_PLAY) {
            if (ft_owns_press()) ;
            else if (!song.playing && arrangement_enabled && !arr_valid(&arrangement, arrangement_ready())) ui_message("EMPTY SECTION: REC");
            else transport_req = song.playing ? 2 : 1;
        }                                               /* (SEQ, EDIT, SAVE: tapped, ui_input.c layer_tap) */
    }
    if ((s = panel_enc(EN_SELECT))) {
        song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + accel(EN_SELECT, s, 200), 40, 240);
        ui.bpm_t = 40;
    }
    if ((s = panel_enc(EN_ALGO)) && !ft_on) {
        track_select((uint32_t)clamp((int32_t)song.sel + s, 0, 3));
        if (song.sel != TRK_DRUM) {
            go_home();
            return;
        }
    }
    if ((s = panel_enc(EN_PRESET))) {
        if (drum_page == 2u)                            /* LANE: the macro page */
            dm_pg = (uint8_t)clamp((int32_t)dm_pg + s, 0, (DM_N + 3) / 4 - 1);
        else
            TDRUM->p[P_E0] = (int16_t)clamp(TDRUM->p[P_E0] + s, 0, DRUM_KITS - 1);
    }
    for (k = 0; k < 4u; k++) if ((s = panel_enc(EN_K1 + k))) {
        ui.hot_col = (uint8_t)k;
        ui.hot_t = 40;
        if (!drum_page) {
            dstep_t *st = &TDRUM->dstep[drum_cursor];
            if (k == 0) drum_lane = (uint8_t)clamp(drum_lane + s, 0, DRUM_LANES - 1);
            if (k == 1) drum_cursor = (uint8_t)clamp(drum_cursor + s, 0, TDRUM->p[P_SLEN] - 1);
            if (k >= 2) {
                if (song.playing && arrangement_enabled) { ui_message("STOP THE SONG FIRST"); continue; }
                undo_mark(TDRUM, ui.step_sess ? ui.step_sess : (ui.step_sess = (undo_sess += 4u) | 3u));
                fm1_irq_off();
                if (k == 2) {
                    if (s > 0) dstep_set(st, drum_lane, LV_NORM, 0);
                    else dstep_clr(st, drum_lane);
                } else if (dstep_has(st, drum_lane)) {
                    uint32_t r = (uint32_t)clamp((int32_t)lvl_rank(dstep_lvl(st, drum_lane)) + (s > 0 ? 1 : -1), 0, 3);
                    dstep_set(st, drum_lane, LV_UP[r], dstep_rat(st, drum_lane));
                }
                fm1_irq_on();
                sync_reload = 1;
            }
        } else if (drum_page == 2u) {                  /* LANE: the macro, "SNARE DECAY +8" in the message bar */
            uint32_t m = dm_pg * 4u + k;
            char b2[8], b3[24];
            if (dm_pg == 2u && k >= 2u) {              /* page 3: KNOB 3 the kit's dice, KNOB 4 this lane's: */
                uint32_t arm = 0xD0u + k;              /* one detent right arms, a second one within ~1.5 s rolls */
                uint32_t seed = (fm1_ms * 2654435761u >> 16) % 65535u + 1u;
                if (s <= 0)
                    continue;
                if (ui.arm != arm) {
                    ui.arm = (uint8_t)arm;
                    ui.arm_t = 90;
                    ui_message(k == 2u ? "AGAIN: DICE THE KIT" : "AGAIN: DICE THIS SOUND");
                    continue;
                }
                ui.arm = 0;
                fm1_irq_off();
                if (k == 2u)
                    dice_kit(seed);
                else
                    dice_lane(drum_kit(), drum_lane, seed);
                TDRUM->p[P_E0] = KIT_USER;
                fm1_irq_on();
                if (k == 2u) {
                    fmt_int(b2, (int32_t)seed);
                    ui_say("DICE #", b2);
                } else {
                    str_cpy(b3, LANE_NAME[drum_lane], sizeof b3);
                    ui_say(b3, ": DICE (MY KIT)");
                }
                continue;
            }
            if (m >= DM_N) continue;
            fm1_irq_off();
            dm_add(drum_lane, m, m == DM_CHOKE ? (s > 0 ? 1 : -1) : accel(EN_K1 + k, s, DM_DESC[m].max - DM_DESC[m].min));
            fm1_irq_on();
            dm_format(m, dm_get(drum_lane, m), b2);
            str_cpy(b3, LANE_NAME[drum_lane], sizeof b3);   /* the sound, the macro (upper case), the value */
            str_cpy(b3 + str_len(b3), " ", 2);
            {
                uint32_t q = str_len(b3), c;
                for (c = 0; DM_DESC[m].name[c] && q + c < sizeof b3 - 2u; c++)
                    b3[q + c] = (char)(DM_DESC[m].name[c] >= 'a' && DM_DESC[m].name[c] <= 'z' ? DM_DESC[m].name[c] - 32 : DM_DESC[m].name[c]);
                b3[q + c] = ' ';
                b3[q + c + 1u] = 0;
            }
            ui_say(b3, b2);
        } else {
            if (k == 0) TDRUM->p[P_E0] = (int16_t)clamp(TDRUM->p[P_E0] + s, 0, DRUM_KITS - 1);
            if (k == 1) song.g[G_DRLVL] = (int16_t)clamp(song.g[G_DRLVL] + s, 0, 127);
            if (k == 2) TDRUM->p[P_DIST] = (int16_t)clamp(TDRUM->p[P_DIST] + accel(EN_K1 + k, s, 127), 0, 127);   /* DRIVE */
            if (k == 3) TDRUM->p[P_CHOR] = (int16_t)clamp(TDRUM->p[P_CHOR] + accel(EN_K1 + k, s, 127), 0, 127);   /* COMP */
        }
    }
}

/* ---------------------------------------------------------------- REC --- */
/* a big seven-segment digit, TE style: w x h, segments t thick */
static void te_digit(int32_t x, int32_t y, int32_t w, int32_t h, int32_t t, uint32_t d, uint16_t c)
{
    static const uint8_t SEG[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};   /* gfedcba */
    uint32_t m = SEG[d % 10u];
    int32_t hh = h / 2;
    if (m & 0x01) cv_rect(x + t, y, w - 2 * t, t, c);                       /* a */
    if (m & 0x02) cv_rect(x + w - t, y + t, t, hh - t, c);                  /* b */
    if (m & 0x04) cv_rect(x + w - t, y + hh, t, hh - t, c);                 /* c */
    if (m & 0x08) cv_rect(x + t, y + h - t, w - 2 * t, t, c);               /* d */
    if (m & 0x10) cv_rect(x, y + hh, t, hh - t, c);                         /* e */
    if (m & 0x20) cv_rect(x, y + t, t, hh - t, c);                          /* f */
    if (m & 0x40) cv_rect(x + t, y + hh - t / 2, w - 2 * t, t, c);          /* g */
}

static uint8_t rec_shown;
/* the four tracks, compact, from y0: the one that records is framed in red */
static void rec_rows(uint32_t rt, uint32_t take, uint32_t y0, uint32_t *cache)
{
    uint32_t sig = rt * 3u + rec_wait + take * 5u + drum_kit() * 977u + y0, i, j;
    for (i = 0; i < NTRK; i++) {
        char b[16];
        if (i == TRK_DRUM) str_cpy(b, DRUM_KIT_NAMES[drum_kit()], sizeof b);
        else trk_short_name(i, b);
        sig = studio_hash(sig, b) + (uint32_t)trk[i].p[P_SLEN] * 31u;
        for (j = 0; j < NSTEP; j++) sig = sig * 3u + (uint32_t)trk_step_on(&trk[i], j);
    }
    if (!ui.force && sig == *cache)
        return;
    *cache = sig;
    cv_begin(240, 76, C_BLACK);
    for (i = 0; i < NTRK; i++) {
        const track_t *t = &trk[i];
        uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), armed = i == rt;
        int32_t y = (int32_t)i * 19;
        char b[16];
        if (armed) cv_rect(0, y, 240, 18, TE_RED), cv_rect(1, y + 1, 238, 16, C_BLACK);
        cv_rect(4, y + 3, 12, 12, armed ? TE_COL[i] : TE_DIM[i]);
        if (i == TRK_DRUM) str_cpy(b, DRUM_KIT_NAMES[drum_kit()], sizeof b);
        else trk_short_name(i, b);
        b[10] = 0;
        cv_text(22, y + 1, &FONT_S, b, armed ? C_WHITE : TE_G3);
        for (j = 0; j < 16u; j++) {
            uint32_t a = j * len / 16u, z = (j + 1u) * len / 16u, k, on = 0;
            if (z == a) z = a + 1u;
            for (k = a; k < z && k < NSTEP; k++) if (trk_step_on(t, k)) on = 1;
            cv_rect(106 + (int32_t)j * 8, y + 5, 6, 8, on ? (armed ? TE_COL[i] : TE_DIM[i]) : TE_G1);
        }
    }
    cv_blit(0, y0);
}

/* REC armed (stopped: waiting for the first note, or PLAY), its count-in, or a free take running
 * (seq.c). Armed, the dials say how it records: KNOB 1 MODE free / tempo (an empty project),
 * KNOB 2 LENGTH (1, 2 or 4 bars), KNOB 3 START note / count (ui_input.c rec_knobs). The four tracks
 * stay in view; the REC LED blinks while armed, is lit during the take. */
static void rec_screen_draw(void)
{
    static uint32_t head, body, rows, foot, dials;
    static uint8_t layout;
    uint32_t sig, take = ft_on, empty = take || project_empty(), rt = take ? ft_trk % NTRK : song.sel;
    uint32_t blink = (fm1_ms / 250u) & 1u, secs = take ? ft_t * CTL / FS : 0u, bars = 0, bpm = 0;
    uint32_t lay = take ? 1u : 2u, free = empty && !rec_tempo && !take, count = ci_on;
    if (take)
        bars = ft_fit(ft_t, &bpm);
    if (!rec_shown || lay != layout) {
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.force = 1;
    }
    rec_shown = 1;
    layout = (uint8_t)lay;
    te_header(take ? "free take" : count ? "count-in" : "rec ready", TE_RED, &head);
    if (take) {                                         /* free take: the time, the loop it makes */
        sig = 1000003u + rt * 7919u + blink * 31u + secs * 131u + bars * 17u + bpm * 3u;
        if (ui.force || sig != body) {
            char b[24];
            uint32_t n = secs > 99u ? 99u : secs;
            body = sig;
            cv_begin(240, 106, C_BLACK);
            te_disc(18, 30, 9, blink ? TE_RED : TE_DIM[3]);
            if (n >= 10u)
                te_digit(36, 4, 32, 56, 7, n / 10u, C_WHITE);
            te_digit(76, 4, 32, 56, 7, n % 10u, C_WHITE);
            cv_text(112, 44, &FONT_S, "s", TE_G3);
            cv_rect(132, 6, 1, 54, TE_G1);
            if (bars) {
                fmt_int(b, (int32_t)bars);
                cv_text(146, 4, &FONT_L, b, TE_COL[rt & 3u]);
                cv_text(146 + text_w(&FONT_L, b) + 6, 18, &FONT_S, bars == 1u ? "bar" : "bars", TE_G3);
                fmt_int(b, (int32_t)bpm);
                cv_text(146, 42, &FONT_S, b, C_WHITE);
                cv_text(146 + text_w(&FONT_S, b) + 4, 42, &FONT_S, "bpm", TE_G3);
            } else {
                cv_text(146, 4, &FONT_L, "-", TE_G3);
            }
            te_text_c(120, 80, "press rec on the 1", TE_RED);
            cv_blit(0, 42);
        }
        rec_rows(rt, take, 148u, &rows);
        if (ui.force || foot != 1u) {
            foot = 1u;
            cv_begin(240, 16, C_BLACK);
            cv_text(4, 0, &FONT_S, "rec: close", TE_G3);
            cv_text(236 - text_w(&FONT_S, "play: drop"), 0, &FONT_S, "play: drop", TE_G3);
            cv_blit(0, 224);
        }
        return;
    }
    foot = 0;
    /* armed / counting in: what happens next */
    sig = 2000003u + empty * 7u + free * 11u + rec_count * 13u + count * 17u + ci_beat * 19u + blink * 31u;
    if (ui.force || sig != body) {
        static const char *const L1[3] = {"play freely", "play a note", "press play"};
        static const char *const L2[3] = {"then rec on the 1", "it starts the loop", "4 clicks, then rec"};
        static const char *const L3[3] = {"the tempo follows you", "play: go  rec: cancel", "rec: cancel"};
        uint32_t m = free ? 0u : rec_count ? 2u : 1u;
        body = sig;
        cv_begin(240, 64, C_BLACK);
        if (count) {                                    /* the count-in: 4, 3, 2, 1 */
            te_digit(96, 4, 30, 54, 6, 4u - (ci_beat > 3u ? 3u : ci_beat), C_WHITE);
            te_disc(60, 31, 12, TE_RED);
            cv_text(144, 14, &FONT_S, "count-in", TE_G4);
            cv_text(144, 34, &FONT_S, "rec: cancel", TE_G3);
        } else {
            te_disc(28, 30, 18, blink ? TE_RED : TE_DIM[3]);
            te_disc(28, 30, 7, C_BLACK);
            cv_text(60, 6, &FONT_S, L1[m], C_WHITE);
            cv_text(60, 24, &FONT_S, L2[m], TE_G4);
            cv_text(60, 42, &FONT_S, L3[m], TE_G3);
        }
        cv_blit(0, 42);
    }
    rec_rows(rt, 0u, 106u, &rows);
    {   /* the dials: how it records */
        static char v[3][10];
        static const char *const LAB_E[4] = {"mode", "length", "start", ""};
        static const char *const LAB_F[4] = {"mode", "", "", ""};
        static const char *const LAB_N[4] = {"", "length", "start", ""};
        static const char *const LAB_0[4] = {"", "", "", ""};
        const char *val[4] = {v[0], v[1], v[2], ""};
        int32_t ratio[4] = {0, 0, 0, 0};
        uint32_t len = (uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP);
        str_cpy(v[0], rec_tempo ? "tempo" : "free", sizeof v[0]);
        if (len % 16u == 0u) {
            fmt_int(v[1], (int32_t)(len / 16u));
            str_cpy(v[1] + str_len(v[1]), len == 16u ? " bar" : " bars", 6);
        } else {
            fmt_int(v[1], (int32_t)len);
            str_cpy(v[1] + str_len(v[1]), " st", 4);
        }
        str_cpy(v[2], rec_count ? "count" : "note", sizeof v[2]);
        ratio[0] = rec_tempo ? 1000 : 0;
        ratio[1] = (int32_t)(len - 1u) * 1000 / (NSTEP - 1);
        ratio[2] = rec_count ? 1000 : 0;
        {
            const char *const *lab = count ? LAB_0 : !empty ? LAB_N : free ? LAB_F : LAB_E;
            uint32_t k;
            for (k = 0; k < 4u; k++)
                if (!lab[k][0])
                    val[k] = "";                        /* (a dial not shown: no value either) */
            te_dials(184, lab, val, ratio, rt * 7u + count * 3u, &dials);
        }
    }
}
