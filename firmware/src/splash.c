/* SPDX-License-Identifier: GPL-3.0-only */
/* sloopDX boot splash: the logo (tools/gen_logo.py -> sloopdx_logo.h: 16 colours, RLE runs of
 * (run - 1) << 4 | colour), drawn into the canvas in bands of at most 120 rows. */
#include "sloopdx_logo.h"
#define SPLASH_PANEL RGB(46, 42, 43)            /* the DX7's panel (tools/gen_logo.py DX_PANEL) */
static void sloopdx_logo_draw(uint32_t y0)
{
    uint32_t r0;
    for (r0 = 0; r0 < SLOOPDX_SPLASH_H; r0 += 120u) {
        uint32_t rows = SLOOPDX_SPLASH_H - r0 > 120u ? 120u : SLOOPDX_SPLASH_H - r0;
        uint32_t lo = r0 * SLOOPDX_SPLASH_W, hi = (r0 + rows) * SLOOPDX_SPLASH_W, pos = 0, i;
        cv_begin(SLOOPDX_SPLASH_W, rows, SPLASH_PANEL);   /* (colour 0 is the panel) */
        for (i = 0; i < sizeof SLOOPDX_SPLASH_RLE && pos < hi; i++) {
            uint32_t run = (SLOOPDX_SPLASH_RLE[i] >> 4) + 1u, c = SLOOPDX_SPLASH_RLE[i] & 15u, p;
            if (c && pos + run > lo)
                for (p = pos < lo ? lo : pos; p < pos + run && p < hi; p++)
                    cv_px[p - lo] = swap16(SLOOPDX_SPLASH_PAL[c]);
            pos += run;
        }
        cv_blit((240u - SLOOPDX_SPLASH_W) / 2u, y0 + r0);
    }
}

/* the boot screen in the DX7's colours: the wordmark and the 32 algorithms, the version in the red LED,
 * where it comes from in the LCD's green */
static void sloopdx_splash(void)
{
    lcd_fill(0, 0, 240, 240, SPLASH_PANEL);
    sloopdx_logo_draw(4);
    const char *v = FELUCCA_VERSION;
    if (v[0] == 's' && v[1] == 'l' && v[7] == ' ')
        v += 8;                                         /* "sloopDX 1.3" -> "1.3" under the wordmark */
    cv_begin(240, 44, SPLASH_PANEL);
    cv_text(120 - text_w(&FONT_S, v) / 2, 4, &FONT_S, v, RGB(255, 46, 34));
    cv_text(120 - text_w(&FONT_S, "dx7 synth - based on felucca") / 2, 22, &FONT_S, "dx7 synth - based on felucca", RGB(186, 222, 112));
    cv_blit(0, 194);
}
