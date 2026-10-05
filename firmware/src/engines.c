/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; zvenFM: (C) 2026 Sven Trogus */
/* Engine table. zvenFM has one: the DX7 (eng_dx7.c). */
#include "dsp.c"
#include "eng_dx7.c"

static const engine_t *const ENGINES[NENGINES] = {&ENG_DX7};

/* every factory sound as loud as the others: a level trim per preset, 1/2 dB (tools/level_presets.py
 * writes preset_trim.h); a track keeps it in P_ED_FX */
#include "preset_trim.h"
static int16_t preset_trim(uint32_t e, uint32_t pi)
{
    return e < PT_ENGINES && pi < PT_MAX ? PRESET_TRIM[e][pi] : 0;
}
