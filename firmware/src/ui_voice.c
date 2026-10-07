/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sven Trogus */
/* DX7 voice edit: the first EDIT page of a synth part. A list, the way the Baud Girl FM-1+VA firmware edits
 * FM (its UI concept, from its public manual; no code): one row per setting, name left, value right, a bar
 * for the range. SELECT moves the highlight, ALGORITHM changes the value, EDIT (tapped) opens a group or runs
 * an action, HOME goes back, SAVE stores the bank. KNOB 1 CUT and 2 RESO (the low-pass behind the voice), 3 and 4
 * as on HOME; turning one shows its value in the top bar.
 * The parameters and their names are the DX7's. The edit buffer is a user slot U01..U32 (eng_dx7.c): the
 * first change to a factory voice copies it into the first INIT VOICE slot. Values go through
 * dx_user_param_set, so the next note plays them. */
static int dx_bank_store(void);                          /* project.c: the user bank to flash, 0 ok */
static int dx_bank_select(uint32_t k);                   /* project.c: another of the 8 banks in use */

/* the DX7's panel: dark warm grey, mint membrane keys, light blue for the operators, salmon and orange for
 * the functions, the red LED, the green LCD */
#define DX_PANEL RGB(46, 42, 43)
#define DX_TRACK RGB(70, 64, 65)
#define DX_LABEL RGB(206, 200, 194)
#define DX_MINT RGB(66, 245, 245)                /* (cyan) */
#define DX_BLUE RGB(175, 217, 244)
#define DX_PINK RGB(244, 192, 203)
#define DX_ORANGE RGB(230, 209, 185)              /* (beige) */
#define DX_LCD RGB(230, 209, 185)
#define DX_LED RGB(255, 46, 34)
enum { VL_TOP, VL_OP, VL_PEG, VL_LFO, VL_NAME };
enum { VK_PAR, VK_VOICE, VK_GROUP, VK_NAME, VK_COPY, VK_INIT, VK_STORE, VK_MORE, VK_OPON, VK_BANK, VK_OPSOLO };
enum { VF_INT, VF_ONOFF, VF_DET, VF_CURVE, VF_MODE, VF_COARSE, VF_NOTE, VF_TRANS, VF_WAVE, VF_ALG, VF_CHAR, VF_PEG };
typedef struct {
    const char *name;
    uint8_t kind, idx, fmt;      /* idx: the unpacked voice parameter (VK_PAR, op rows: the offset in the
                                  * op block), or the group's level (VK_GROUP; 6..11: OP1..OP6) */
} vrow_t;

static const vrow_t VR_TOP[] = {
    {"Voice", VK_VOICE, 0, 0},
    {"Bank", VK_BANK, 0, 0},                             /* which of the 8 user banks U01..U32 play */
    {"Algorithm", VK_PAR, 134, VF_ALG},
    {"Feedback", VK_PAR, 135, VF_INT},
    {"Osc Sync", VK_PAR, 136, VF_ONOFF},
    {"Transpose", VK_PAR, 144, VF_TRANS},
    {"OP1", VK_GROUP, 6, 0}, {"OP2", VK_GROUP, 7, 0}, {"OP3", VK_GROUP, 8, 0},
    {"OP4", VK_GROUP, 9, 0}, {"OP5", VK_GROUP, 10, 0}, {"OP6", VK_GROUP, 11, 0},
    {"Pitch Env", VK_GROUP, VL_PEG, 0},
    {"LFO", VK_GROUP, VL_LFO, 0},
    {"Name", VK_NAME, VL_NAME, 0},
    {"Copy To", VK_COPY, 0, 0},
    {"Init Voice", VK_INIT, 0, 0},
    {"Store", VK_STORE, 0, 0},
    {"More Pages", VK_MORE, 0, 0},
};
static const vrow_t VR_OP[] = {                          /* idx: the offset in the operator's 21 bytes */
    {"On", VK_OPON, 0, VF_ONOFF},                        /* (not stored: dx_opmute) */
    {"Solo", VK_OPSOLO, 0, VF_ONOFF},                    /* only this operator heard (the others muted until off) */
    {"Output Level", VK_PAR, 16, VF_INT},
    {"Coarse", VK_PAR, 18, VF_COARSE},
    {"Fine", VK_PAR, 19, VF_INT},
    {"Detune", VK_PAR, 20, VF_DET},
    {"Osc Mode", VK_PAR, 17, VF_MODE},
    {"Rate 1", VK_PAR, 0, VF_INT}, {"Rate 2", VK_PAR, 1, VF_INT}, {"Rate 3", VK_PAR, 2, VF_INT}, {"Rate 4", VK_PAR, 3, VF_INT},
    {"Level 1", VK_PAR, 4, VF_INT}, {"Level 2", VK_PAR, 5, VF_INT}, {"Level 3", VK_PAR, 6, VF_INT}, {"Level 4", VK_PAR, 7, VF_INT},
    {"Key Velocity", VK_PAR, 15, VF_INT},
    {"Amp Mod Sens", VK_PAR, 14, VF_INT},
    {"Break Point", VK_PAR, 8, VF_NOTE},
    {"Left Depth", VK_PAR, 9, VF_INT},
    {"Right Depth", VK_PAR, 10, VF_INT},
    {"Left Curve", VK_PAR, 11, VF_CURVE},
    {"Right Curve", VK_PAR, 12, VF_CURVE},
    {"Rate Scaling", VK_PAR, 13, VF_INT},
};
static const vrow_t VR_PEG[] = {
    {"Rate 1", VK_PAR, 126, VF_INT}, {"Rate 2", VK_PAR, 127, VF_INT}, {"Rate 3", VK_PAR, 128, VF_INT}, {"Rate 4", VK_PAR, 129, VF_INT},
    {"Level 1", VK_PAR, 130, VF_PEG}, {"Level 2", VK_PAR, 131, VF_PEG}, {"Level 3", VK_PAR, 132, VF_PEG}, {"Level 4", VK_PAR, 133, VF_PEG},
};
static const vrow_t VR_LFO[] = {
    {"Wave", VK_PAR, 142, VF_WAVE},
    {"Speed", VK_PAR, 137, VF_INT},
    {"Delay", VK_PAR, 138, VF_INT},
    {"Pitch Mod Depth", VK_PAR, 139, VF_INT},
    {"Amp Mod Depth", VK_PAR, 140, VF_INT},
    {"Pitch Mod Sens", VK_PAR, 143, VF_INT},
    {"Key Sync", VK_PAR, 141, VF_ONOFF},
};
static const vrow_t VR_NAME[] = {
    {"Char 1", VK_PAR, 145, VF_CHAR}, {"Char 2", VK_PAR, 146, VF_CHAR}, {"Char 3", VK_PAR, 147, VF_CHAR},
    {"Char 4", VK_PAR, 148, VF_CHAR}, {"Char 5", VK_PAR, 149, VF_CHAR}, {"Char 6", VK_PAR, 150, VF_CHAR},
    {"Char 7", VK_PAR, 151, VF_CHAR}, {"Char 8", VK_PAR, 152, VF_CHAR}, {"Char 9", VK_PAR, 153, VF_CHAR},
    {"Char 10", VK_PAR, 154, VF_CHAR},
};
#define VE_ROWS 6u                                       /* rows on screen */

static struct {
    uint8_t lvl, op;             /* the list shown; op 0..5 = OP1..OP6 on VL_OP */
    uint8_t row, top;            /* highlight and first row shown */
    uint8_t row0;                /* the top list's row, to go back to */
    uint8_t diag;                /* the algorithm drawn (Algorithm row, after the first ALGORITHM turn) */
    uint8_t dirty;               /* the bank changed since the last store */
    uint8_t copy;                /* Copy To: the target slot */
    uint8_t arm;                 /* an action armed (its row kind + 1), EDIT again within 1.5 s runs it */
    uint32_t arm_ms;
    uint8_t mute_was;            /* the operator switches before a Solo, put back when it goes off */
} ve;

static int on_voice_page(void) { return !ui.home && cur_page()->scope == SC_VOICE; }

static const vrow_t *ve_rows(uint32_t *n)
{
    switch (ve.lvl) {
    case VL_OP: *n = sizeof VR_OP / sizeof VR_OP[0]; return VR_OP;
    case VL_PEG: *n = sizeof VR_PEG / sizeof VR_PEG[0]; return VR_PEG;
    case VL_LFO: *n = sizeof VR_LFO / sizeof VR_LFO[0]; return VR_LFO;
    case VL_NAME: *n = sizeof VR_NAME / sizeof VR_NAME[0]; return VR_NAME;
    default: *n = sizeof VR_TOP / sizeof VR_TOP[0]; return VR_TOP;
    }
}
/* the unpacked parameter of a row: op rows count from the operator's block (OP1 = block 5) */
static uint32_t ve_idx(const vrow_t *r) { return ve.lvl == VL_OP ? (5u - ve.op) * 21u + r->idx : r->idx; }
static uint32_t ve_voice(void) { return (uint32_t)TSEL->p[P_E0] % DX_NVOICES; }
static int ve_user(void) { return ve_voice() >= DX_NSYNTH ? (int)(ve_voice() - DX_NSYNTH) : -1; }

/* the operator switches (eng_dx7.c dx_opmute): they belong to the voice they were set on */
static int ve_muted(uint32_t d)
{
    return song.sel < NPART && dx_opmute_v[song.sel] == ve_voice() && ((dx_opmute[song.sel] >> d) & 1u);
}
static void ve_mute_toggle(uint32_t d)
{
    if (song.sel >= NPART)
        return;
    if (dx_opmute_v[song.sel] != ve_voice())
        dx_opmute[song.sel] = 0;
    dx_opmute_v[song.sel] = (uint8_t)ve_voice();
    dx_opmute[song.sel] ^= (uint8_t)(1u << d);
}

/* Solo: only this operator sounds. It is nothing but the switches (every other operator off), so it reads true
 * from the mask alone: eng_dx7.c clears the mask on another voice, and On rows can change it meanwhile */
#define VE_SOLO_MASK(d) ((uint8_t)(0x3Fu & ~(1u << (d))))
static int ve_is_solo_mask(uint8_t m) { uint32_t d; for (d = 0; d < 6u; d++) if (m == VE_SOLO_MASK(d)) return 1; return 0; }
static int ve_soloed(uint32_t d)
{
    return song.sel < NPART && dx_opmute_v[song.sel] == ve_voice() && dx_opmute[song.sel] == VE_SOLO_MASK(d);
}
static void ve_solo_toggle(uint32_t d)
{
    uint8_t *m;
    if (song.sel >= NPART)
        return;
    m = &dx_opmute[song.sel];
    if (dx_opmute_v[song.sel] != ve_voice()) {
        *m = 0;
        ve.mute_was = 0;
    }
    dx_opmute_v[song.sel] = (uint8_t)ve_voice();
    if (*m == VE_SOLO_MASK(d)) {                          /* off: the switches as they were before the solo */
        *m = ve_is_solo_mask(ve.mute_was) ? 0 : ve.mute_was;
    } else {
        if (!ve_is_solo_mask(*m))                         /* (solo moved from another operator: keep the older state) */
            ve.mute_was = *m;
        *m = VE_SOLO_MASK(d);
    }
}

static uint32_t ve_get(uint32_t idx)
{
    int k = ve_user();
    if (k >= 0)
        return dx_user_param_get((uint32_t)k, idx);
    return DX_SYNTH[ve_voice()][idx];
}

static void ve_slot_label(char *b, uint32_t k)            /* "U07" */
{
    b[0] = 'U';
    b[1] = (char)('0' + (k + 1u) / 10u);
    b[2] = (char)('0' + (k + 1u) % 10u);
    b[3] = 0;
}
static int ve_streq(const char *a, const char *b) { while (*a && *a == *b) a++, b++; return *a == *b; }
static int ve_free_slot(void)                             /* the first INIT VOICE slot, -1 = none */
{
    uint32_t k;
    if (!dx_user_ok)
        return 0;
    for (k = 0; k < DX_NUSER; k++)
        if (ve_streq(dx_user_name[k], "INIT VOICE"))
            return (int)k;
    return -1;
}
/* the slot being edited; a factory voice is copied into a free slot first. -1: none free */
static int ve_edit_slot(void)
{
    uint8_t b[128];
    char lab[4];
    int k = ve_user();
    if (k >= 0)
        return k;
    if ((k = ve_free_slot()) < 0) {
        ui_message("NO FREE SLOT: COPY TO");
        return -1;
    }
    dx_voice_get(ve_voice(), b);
    dx_user_put((uint32_t)k, b);
    TSEL->p[P_E0] = (int16_t)(DX_NSYNTH + (uint32_t)k);
    ve_slot_label(lab, (uint32_t)k);
    ui_say("COPIED TO ", lab);
    ve.dirty = 1;
    return k;
}
static void ve_set(uint32_t idx, uint32_t v)
{
    int k = ve_edit_slot();
    if (k < 0)
        return;
    dx_user_param_set((uint32_t)k, idx, v);
    ve.dirty = 1;
}

/* a row's value as text */
static void ve_fmt(const vrow_t *r, uint32_t v, char *b)
{
    static const char *const CURVE[4] = {"-LIN", "-EXP", "+EXP", "+LIN"};
    static const char *const WAVE[6] = {"TRI", "SAW DN", "SAW UP", "SQUARE", "SINE", "S&HOLD"};
    switch (r->fmt) {
    case VF_ONOFF: str_cpy(b, v ? "ON" : "OFF", 4); break;
    case VF_DET:
        if (v > 7u) { b[0] = '+'; fmt_int(b + 1, (int32_t)v - 7); }
        else fmt_int(b, (int32_t)v - 7);
        break;
    case VF_CURVE: str_cpy(b, CURVE[v & 3u], 6); break;
    case VF_MODE: str_cpy(b, v ? "FIXED" : "RATIO", 6); break;
    case VF_COARSE: {                                     /* the ratio (fine included), or the fixed range */
        uint32_t base = ve_idx(r) - 18u;                  /* the op block */
        if (ve_get(base + 17u)) {
            static const char *const FIX[4] = {"1 Hz", "10 Hz", "100 Hz", "1000 Hz"};
            str_cpy(b, FIX[v & 3u], 8);
        } else {
            uint32_t f = ve_get(base + 19u), r100 = v ? v * 100u : 50u, n;
            r100 = r100 + r100 * f / 100u;                /* x.xx */
            fmt_int(b, (int32_t)(r100 / 100u));
            n = str_len(b);
            b[n] = '.';
            b[n + 1] = (char)('0' + (r100 / 10u) % 10u);
            b[n + 2] = (char)('0' + r100 % 10u);
            b[n + 3] = 0;
        }
        break;
    }
    case VF_NOTE: note_name(b, v + 9u); break;            /* break point as the DX7 names it: 0 = A-1, 39 = C3 */
    case VF_TRANS:                                        /* 24 = C3, as the DX7 shows it */
        if (v > 24u) { b[0] = '+'; fmt_int(b + 1, (int32_t)v - 24); }
        else fmt_int(b, (int32_t)v - 24);
        break;
    case VF_WAVE: str_cpy(b, WAVE[v < 6u ? v : 5u], 8); break;
    case VF_ALG: fmt_int(b, (int32_t)v + 1); break;
    case VF_PEG:                                          /* 50 = no shift */
        if (v > 50u) { b[0] = '+'; fmt_int(b + 1, (int32_t)v - 50); }
        else fmt_int(b, (int32_t)v - 50);
        break;
    case VF_CHAR: b[0] = (char)(v >= 32u && v < 127u ? v : ' '); b[1] = 0; if (b[0] == ' ') str_cpy(b, "space", 6); break;
    default: fmt_int(b, (int32_t)v); break;
    }
}

/* the value of row r as text, and its share of the range (0..100, -1 = no bar) */
static int32_t ve_row_value(const vrow_t *r, char *b)
{
    char lab[4];
    uint32_t v, mx;
    b[0] = 0;
    switch (r->kind) {
    case VK_VOICE:
        b[0] = (char)('0' + (ve_voice() + 1u) / 10u);    /* its number in the list: 01..20 factory, 21..52 U01..U32 */
        b[1] = (char)('0' + (ve_voice() + 1u) % 10u);
        b[2] = ' ';
        b[3] = 0;
        str_cpy(b + 3, dx_names[ve_voice()], 12);
        (void)lab;
        return -1;
    case VK_GROUP:
        if (r->idx >= 6u && ve_muted(r->idx - 6u)) { str_cpy(b, "OFF >", 6); return -1; }
        str_cpy(b, ">", 2);
        return -1;
    case VK_OPON: str_cpy(b, ve_muted(ve.op) ? "OFF" : "ON", 4); return -1;
    case VK_OPSOLO: str_cpy(b, ve_soloed(ve.op) ? "ON" : "OFF", 4); return -1;
    case VK_NAME: str_cpy(b, dx_names[ve_voice()], 12); return -1;
    case VK_COPY: ve_slot_label(b, ve.copy); return -1;
    case VK_BANK:
        b[0] = (char)('1' + dx_bank_cur);
        str_cpy(b + 1, dx_user_ok ? "" : " empty", 8);
        return -1;
    case VK_INIT: case VK_STORE: case VK_MORE: str_cpy(b, ve.arm == r->kind + 1u ? "AGAIN" : ">", 6); return -1;
    default: break;
    }
    v = ve_get(ve_idx(r));
    ve_fmt(r, v, b);
    mx = dx_param_max(ve_idx(r));
    return r->fmt == VF_CHAR || !mx ? -1 : (int32_t)(v * 100u / mx);
}

/* ------------------------------------------------------------------ input --- */
static void ve_go(uint32_t lvl, uint32_t op)
{
    if (lvl == VL_TOP)
        ve.row = ve.row0;
    else {
        ve.row0 = ve.row;
        ve.row = 0;
    }
    ve.lvl = (uint8_t)lvl;
    ve.op = (uint8_t)op;
    ve.top = 0;
    ve.diag = 0;
    ve.arm = 0;
}
static int ve_armed(uint32_t kind, const char *what)      /* one tap arms, a second within 1.5 s acts */
{
    if (ve.arm == kind + 1u && fm1_ms - ve.arm_ms < 1500u) {
        ve.arm = 0;
        return 1;
    }
    ve.arm = (uint8_t)(kind + 1u);
    ve.arm_ms = fm1_ms;
    ui_say("AGAIN: ", what);
    return 0;
}
static void ve_store(void)                                /* SAVE, or Store: the bank to flash */
{
    if (!dx_user_ok) {
        ui_message("NOTHING TO STORE");
        return;
    }
    if (dx_bank_store()) {
        ui_message("STORE ERROR");
        return;
    }
    ve.dirty = 0;
    ui_message("STORED");
}
/* EDIT tapped on the list: open the group, run the action */
static void voice_tap(void)
{
    uint32_t n;
    const vrow_t *r = ve_rows(&n) + ve.row;
    uint8_t b[128];
    char lab[4];
    if (ve.lvl != VL_TOP || ve.row >= n)
        return;
    switch (r->kind) {
    case VK_GROUP:
        if (r->idx >= 6u) ve_go(VL_OP, r->idx - 6u);
        else ve_go(r->idx, 0);
        break;
    case VK_NAME:
        ve_go(VL_NAME, 0);
        break;
    case VK_COPY:
        if (!ve_armed(VK_COPY, "COPY"))
            break;
        dx_voice_get(ve_voice(), b);
        dx_user_put(ve.copy, b);
        TSEL->p[P_E0] = (int16_t)(DX_NSYNTH + ve.copy);
        ve.dirty = 1;
        ve_slot_label(lab, ve.copy);
        ui_say("COPIED TO ", lab);
        break;
    case VK_INIT: {
        int k;
        if (!ve_armed(VK_INIT, "INIT VOICE") || (k = ve_edit_slot()) < 0)
            break;
        dx_pack(DX_SYNTH[DX_NSYNTH - 1u], b);
        dx_user_put((uint32_t)k, b);
        ve.dirty = 1;
        ui_message("INIT VOICE");
        break;
    }
    case VK_STORE:
        if (ve_armed(VK_STORE, "STORE"))
            ve_store();
        break;
    case VK_MORE: {                                       /* the next EDIT pages: quick knobs, voice mode */
        uint32_t i = ui.page + 1u;
        if (i < NPAGES && PAGES[i].fam == FAM_EDIT) {
            ui.page = (uint8_t)i;
            ui.fam_last[FAM_EDIT] = ui.page;
            page_entered();
        }
        break;
    }
    default:
        break;
    }
    ui.force = 1;
}

static void voice_screen_input(uint32_t pressed, uint32_t home)
{
    uint32_t n, k;
    int32_t s;
    const vrow_t *rows;
    if (is_drum(TSEL)) {                                  /* the drum track has its own EDIT: the kit */
        studio_open(SC_DRUM);
        return;
    }
    for (k = 0; k < NB; k++) {
        if (!((pressed >> panel.btn[k]) & 1u))
            continue;
        if (k == B_PLAY) {
            if (!ft_owns_press())
                transport_req = song.playing ? 2 : 1;
        } else if (k == B_OCTDN || k == B_OCTUP) {        /* the octave, so the keys play where they did */
            uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
            if ((fm1_in.buttons & both) == both) song.octave = 0;
            else song.octave += k == B_OCTDN ? (song.octave > -3 ? -1 : 0) : (song.octave < 3 ? 1 : 0);
        } else if (k == B_ENV) {
            open_family(FAM_ENV);
            return;
        } else if (k == B_LFO) {
            open_family(FAM_LFO);
            return;
        }
    }
    if (home == 1u) {                                     /* HOME tapped (BT_TAP, ui_input.c) */
        if (ve.lvl != VL_TOP) {
            ve_go(VL_TOP, 0);
            ui.force = 1;
        } else {
            go_home();
        }
        return;
    }
    rows = ve_rows(&n);
    if ((s = panel_enc(EN_SELECT)) != 0) {                /* the highlight */
        ve.row = (uint8_t)clamp((int32_t)ve.row + s, 0, (int32_t)n - 1);
        if (ve.row < ve.top) ve.top = ve.row;
        if (ve.row >= ve.top + VE_ROWS) ve.top = (uint8_t)(ve.row - VE_ROWS + 1u);
        ve.diag = 0;
        ve.arm = 0;
    }
    if ((s = panel_enc(EN_ALGO)) != 0 && ve.row < n) {    /* the value */
        const vrow_t *r = rows + ve.row;
        if (r->kind == VK_BANK) {
            int32_t k = clamp((int32_t)dx_bank_cur + (s > 0 ? 1 : -1), 0, DX_NBANKS - 1);
            if ((uint32_t)k != dx_bank_cur) {
                if (ve.dirty && dx_user_ok && !dx_bank_store())   /* (edits of the bank left are kept) */
                    ve.dirty = 0;
                dx_bank_select((uint32_t)k);
                ve.dirty = 0;
                ui.force = 1;
            }
        } else if (r->kind == VK_OPON) {
            ve_mute_toggle(ve.op);
        } else if (r->kind == VK_OPSOLO) {
            ve_solo_toggle(ve.op);
        } else if (r->kind == VK_VOICE) {
            TSEL->p[P_E0] = (int16_t)clamp(TSEL->p[P_E0] + s, 0, DX_NVOICES - 1);
            /* the level trim follows the voice: a factory voice its preset's (preset k plays voice k), a bank voice
             * the bank's */
            TSEL->p[P_ED_FX] = TSEL->p[P_E0] < (int16_t)DX_NSYNTH ? preset_trim(0, (uint32_t)TSEL->p[P_E0]) : DX_BANK_TRIM;
        } else if (r->kind == VK_COPY) {
            ve.copy = (uint8_t)clamp((int32_t)ve.copy + s, 0, DX_NUSER - 1);
            ve.arm = 0;
        } else if (r->kind == VK_PAR) {
            uint32_t idx = ve_idx(r), mx = dx_param_max(idx);
            int32_t v = (int32_t)ve_get(idx);
            if (r->fmt == VF_ALG && !ve.diag) {
                ve.diag = 1;                              /* the first turn only draws the algorithm */
            } else {
                if (r->fmt == VF_CHAR) v = clamp(v + s, 32, 126);
                else v = clamp(v + accel(EN_ALGO, s, (int32_t)mx), 0, (int32_t)mx);
                if ((uint32_t)v != ve_get(idx))
                    ve_set(idx, (uint32_t)v);
            }
        }
    }
    if ((s = panel_enc(EN_PRESET)) != 0) {                /* PRESETS: straight to the next / previous operator */
        int32_t d = ve.lvl == VL_OP ? (int32_t)ve.op + (s > 0 ? 1 : -1) : (s > 0 ? 0 : 5);
        uint32_t row = ve.lvl == VL_OP ? ve.row : 0u, top = ve.lvl == VL_OP ? ve.top : 0u;
        d = (d + 6) % 6;
        ve_go(VL_OP, (uint32_t)d);
        ve.row = (uint8_t)row;                            /* (the same row on the next operator) */
        ve.top = (uint8_t)top;
        ve.row0 = (uint8_t)(6u + (uint32_t)d);            /* HOME comes back to that OP row (VR_TOP: OP1 is row 6) */
        ui.force = 1;
    }
    for (k = 0; k < 4u; k++) {                            /* KNOB 1 CUT, 2 RESO (the filter behind the voice),
                                                           * 3 / 4 as on HOME; the value in the top bar */
        int16_t *vp;
        const param_desc_t *d;
        char val[8], msg[16];
        const char *unit;
        if ((s = panel_enc(EN_K1 + k)) == 0)
            continue;
        if (k < 2u) {
            vp = &TSEL->p[P_E6 + k];
            d = track_desc(TSEL, P_E6 + k);
        } else {
            d = home_param(k, &vp);
        }
        *vp = (int16_t)clamp(*vp + accel(EN_K1 + k, s, d->max - d->min), d->min, d->max);
        param_format(d, *vp, val, &unit);
        str_cpy(msg, d->label, 8);
        str_cpy(msg + str_len(msg), " ", 2);
        ui_say(msg, val);
        str_cpy(ui.msg + str_len(ui.msg), unit, sizeof ui.msg - str_len(ui.msg));
        ui.hot_col = (uint8_t)k;
        ui.hot_t = 40;
    }
    if (ve.arm && fm1_ms - ve.arm_ms >= 1500u)
        ve.arm = 0;
}

/* ------------------------------------------------------------------- draw --- */
/* the algorithm as the DX7 prints it: carriers on the bottom row, each modulator above the operator it
 * feeds, feedback as a loop on the right. DX_ALG (dx7_core.c): op 0 = OP6 .. 5 = OP1; bits 0-1 the bus it
 * writes (0 = the output: a carrier), bit 2 adds to the bus, bits 4-5 the bus it reads, 6 / 7 feedback */
/* x of op c in half columns: a leaf takes the next column, a parent sits over its modulators */
static int32_t ve_place(int32_t c, const int32_t *tgt, int32_t *x2, int32_t *ncol)
{
    int32_t j, sum = 0, nk = 0;
    for (j = 5; j >= 0; j--)                              /* (high index = low OP number: OP order) */
        if (tgt[j] == c) {
            sum += ve_place(j, tgt, x2, ncol);
            nk++;
        }
    x2[c] = nk ? sum / nk : 2 * (*ncol)++ + 1;
    return x2[c];
}
/* how loud operator block i (0 = OP6) of the newest note is now, 0..1000 of its full scale; 0 = no note */
static int32_t ve_op_live(uint32_t i)
{
    const track_t *t = TSEL;
    uint32_t v, best = NVOICE, p = song.sel;
    const dx_env_t *e;
    int32_t top, lo;
    if (p >= NPART)
        return 0;
    for (v = 0; v < NVOICE; v++)
        if (t->v[v].active && t->v[v].stage != 4u && (best == NVOICE || t->v[v].age > t->v[best].age))
            best = v;
    if (best == NVOICE)
        return 0;
    e = &DXV[p][best].env[i];
    top = (((dx_scaleoutlevel(99) >> 1) << 6) + e->outlevel - 4256) << 4;
    lo = 16 << 4;
    return top <= lo ? 0 : clamp(((e->level >> 12) - lo) * 1000 / (top - lo), 0, 1000);
}

static void ve_draw_alg(uint32_t alg, uint16_t col)
{
    uint32_t i, j, bus[3] = {0, 0, 0}, mods[6] = {0}, carriers = 0, depth[6] = {0}, maxd = 0;
    int32_t tgt[6], x2[6], y[6], ncol = 0, colw, rowh, fbin = -1, fbout = -1;
    for (i = 0; i < 6u; i++) {
        uint32_t fl = DX_ALG[alg & 31u][i], inb = (fl >> 4) & 3u, outb = fl & 3u;
        if (inb)
            mods[i] = bus[inb];
        if (!outb)
            carriers |= 1u << i;
        else if (fl & 4u)
            bus[outb] |= 1u << i;
        else
            bus[outb] = 1u << i;
        if (fl & 0x40u) fbin = (int32_t)i;
        if (fl & 0x80u) fbout = (int32_t)i;
        tgt[i] = -1;
    }
    for (i = 0; i < 6u; i++)                              /* each modulator under its first reader */
        for (j = i + 1u; j < 6u && tgt[i] < 0; j++)
            if ((mods[j] >> i) & 1u)
                tgt[i] = (int32_t)j;
    for (i = 6u; i-- > 0;) {                              /* depth from the bottom */
        depth[i] = tgt[i] < 0 ? 0u : depth[tgt[i]] + 1u;
        if (depth[i] > maxd) maxd = depth[i];
    }
    for (i = 6u; i-- > 0;)                                /* columns: carriers in OP order, OP1 first */
        if ((carriers >> i) & 1u)
            ve_place((int32_t)i, tgt, x2, &ncol);
    colw = ncol ? 200 / ncol : 40;
    if (colw > 40) colw = 40;
    rowh = 140 / (int32_t)(maxd + 1u);
    if (rowh > 34) rowh = 34;
    for (i = 0; i < 6u; i++) {
        x2[i] = 120 - (ncol * colw) / 2 + x2[i] * colw / 2;
        y[i] = 140 - (int32_t)depth[i] * rowh;
    }
    for (i = 0; i < 6u; i++) {                            /* links: modulator -> every op it feeds */
        for (j = 0; j < 6u; j++)
            if ((mods[j] >> i) & 1u)
                cv_line(x2[i], y[i] + 10, x2[j], y[j] - 10, DX_LABEL);
        if ((carriers >> i) & 1u)
            cv_line(x2[i], y[i] + 10, x2[i], 162, DX_LABEL);
    }
    cv_rect(10, 162, 220, 2, DX_LABEL);                   /* the output */
    if (fbin >= 0 && fbout >= 0) {                        /* feedback: out of fbout, back into fbin */
        int32_t xr = (x2[fbin] > x2[fbout] ? x2[fbin] : x2[fbout]) + 18;
        cv_line(x2[fbout] + 12, y[fbout], xr, y[fbout], col);
        cv_line(xr, y[fbout], xr, y[fbin] - 14, col);
        cv_line(xr, y[fbin] - 14, x2[fbin], y[fbin] - 14, col);
        cv_line(x2[fbin], y[fbin] - 14, x2[fbin], y[fbin] - 10, col);
    }
    for (i = 0; i < 6u; i++) {
        char b[2] = {(char)('6' - (int32_t)i), 0};
        int carrier = (carriers >> i) & 1u, off = ve_muted(5u - i);
        int32_t lv = (int32_t)ve_get(i * 21u + 16u), live = off ? 0 : ve_op_live(i);
        cv_rect(x2[i] - 12, y[i] - 10, 24, 20, off ? DX_TRACK : carrier ? DX_MINT : DX_BLUE);
        cv_text(x2[i] - 4, y[i] - 8, &FONT_S, b, off ? DX_LABEL : C_BLACK);
        if (live)                                         /* the note: how loud this operator is now */
            cv_rect(x2[i] - 12, y[i] + 7, 24 * live / 1000, 3, DX_LED);
        cv_rect(x2[i] - 12, y[i] + 11, 24, 2, DX_TRACK);  /* its Output Level */
        cv_rect(x2[i] - 12, y[i] + 11, 24 * lv / 99, 2, off ? DX_LABEL : DX_ORANGE);
    }
}

static uint16_t ve_kind_col(const vrow_t *r)              /* the colour of a row's key, as on the panel */
{
    switch (r->kind) {
    case VK_GROUP: return r->idx >= 6u ? DX_BLUE : DX_MINT;
    case VK_OPON: case VK_OPSOLO: return DX_BLUE;
    case VK_COPY: case VK_INIT: return DX_PINK;
    case VK_STORE: case VK_MORE: return DX_ORANGE;
    default: return DX_MINT;
    }
}
/* the list (or the algorithm), 240 x 180, drawn through cv_oy in two passes */
static void ve_body(const vrow_t *rows, uint32_t n, uint16_t col)
{
    uint32_t i;
    char b[24];
    if (ve.diag) {
        uint32_t a = ve_get(134);
        fmt_int(b, (int32_t)a + 1);
        cv_text(4, 0, &FONT_S, "algorithm", DX_LABEL);
        cv_text(4, 16, &FONT_L, b, DX_LED);              /* the DX7's red LED */
        cv_text(176, 0, &FONT_S, "fb", DX_LABEL);
        fmt_int(b, (int32_t)ve_get(135));
        cv_text(200, 0, &FONT_S, b, DX_LCD);
        ve_draw_alg(a, DX_ORANGE);                       /* (the feedback loop) */
        return;
    }
    for (i = ve.top; i < ve.top + VE_ROWS && i < n; i++) {
        int32_t y0 = (int32_t)(i - ve.top) * 30, pct = ve_row_value(&rows[i], b);
        int hl = i == ve.row;
        uint16_t kc = ve_kind_col(&rows[i]);
        if (hl)                                           /* the highlight: a lit membrane key */
            cv_rect(0, y0 + 1, 236, 27, kc);
        else
            cv_rect(0, y0 + 4, 4, 20, kc);                /* the key's colour at the edge */
        cv_text(10, y0 + 4, &FONT_S, rows[i].name, hl ? C_BLACK : DX_LABEL);
        cv_text(230 - text_w(&FONT_S, b), y0 + 4, &FONT_S, b, hl ? C_BLACK : DX_LCD);
        if (pct >= 0) {                                   /* the range */
            cv_rect(10, y0 + 23, 100, 2, hl ? DX_PANEL : DX_TRACK);
            cv_rect(10, y0 + 23, pct, 2, hl ? C_BLACK : kc);
        }
    }
    if (n > VE_ROWS) {                                    /* the scroll bar */
        int32_t h = 180 * (int32_t)VE_ROWS / (int32_t)n, y = 180 * (int32_t)ve.top / (int32_t)n;
        cv_rect(237, 0, 3, 180, DX_TRACK);
        cv_rect(237, y, 3, h, DX_LABEL);
    }
}

static void voice_screen_draw(void)
{
    static uint32_t head, body_sig, foot_sig;
    uint32_t n, i, sig;
    const vrow_t *rows = ve_rows(&n);
    uint16_t col = TE_COL[song.sel % 4u];
    char b[24], title[12];
    if (ve.lvl == VL_OP) {
        str_cpy(title, "op", 3);
        title[2] = (char)('1' + ve.op);
        title[3] = 0;
    } else {
        static const char *const T[5] = {"dx7 voice", "", "pitch env", "lfo", "name"};
        str_cpy(title, T[ve.lvl], 12);
    }
    {   /* the header, and under its title two small dials: CUT and RESO of the low-pass, lit when they are set
         * (CUT below 127: closing; RESO above 0), grey when the filter is out of the way */
        static uint32_t fsig;
        uint32_t h0 = head, cut = (uint32_t)TSEL->p[P_E6], res = (uint32_t)TSEL->p[P_E7], f = cut * 131u + res + 1u;
        te_header(title, DX_MINT, &head);
        if (!song.rec && (ui.force || head != h0 || f != fsig)) {
            fsig = f;
            cv_begin(94, 19, C_BLACK);
            cv_text(0, 1, &FONT_S, "cut", cut < 127u ? C_WHITE : TE_G3);
            te_dial(36, 9, 8, (int32_t)(cut * 1000u / 127u), cut < 127u ? TE_COL[0] : TE_G3, TE_G2);
            cv_text(50, 1, &FONT_S, "res", res ? C_WHITE : TE_G3);
            te_dial(85, 9, 8, (int32_t)(res * 1000u / 127u), res ? TE_COL[2] : TE_G3, TE_G2);
            cv_blit(146, 20);
        }
    }
    if (ve.row >= n) ve.row = 0;
    sig = ve.lvl * 7919u + ve.op * 131u + ve.row * 31u + ve.top * 17u + ve.diag * 3u + ve.arm * 5u + ve_voice() * 104729u;
    for (i = ve.top; i < ve.top + VE_ROWS && i < n; i++) {
        int32_t pct = ve_row_value(&rows[i], b);
        sig = studio_hash(sig * 31u + (uint32_t)(pct + 1), b);
    }
    if (ve.diag) {
        uint32_t k;
        sig = sig * 31u + ve_get(134) * 7u + ve_get(135);
        for (k = 0; k < 6u; k++)                          /* the operators' levels and how loud they are now */
            sig = sig * 31u + ve_get(k * 21u + 16u) * 7u + (uint32_t)ve_op_live(k) / 40u + (uint32_t)ve_muted(5u - k);
    }
    if (ui.force || sig != body_sig) {
        uint32_t pass;
        body_sig = sig;
        for (pass = 0; pass < 2u; pass++) {               /* the canvas holds 124 rows: two halves */
            cv_begin(240, 90, DX_PANEL);
            cv_oy = -90 * (int32_t)pass;
            ve_body(rows, n, col);
            cv_oy = 0;
            cv_blit(0, 40u + 90u * pass);
        }
    }
    /* the footer: the slot and its name, a dot while the bank is not stored; or a message */
    if (ve_user() >= 0) {
        ve_slot_label(b, (uint32_t)ve_user());
        str_cpy(b + 3, " ", 2);
    } else {
        str_cpy(b, "factory ", 9);
    }
    str_cpy(b + str_len(b), dx_names[ve_voice()], 12);
    sig = studio_hash(ve.dirty * 7u + 1u, ui.msg_t ? ui.msg : b);
    if (ui.force || sig != foot_sig) {
        foot_sig = sig;
        cv_begin(240, 20, C_BLACK);                       /* the LCD line */
        if (ui.msg_t) {
            cv_text(4, 3, &FONT_S, ui.msg, DX_LCD);
        } else {
            int32_t x = cv_text(4, 3, &FONT_S, b, DX_LCD);
            if (ve.dirty)
                te_disc(x + 6, 11, 3, DX_LED);           /* not stored */
        }
        cv_blit(0, 220);
    }
}
