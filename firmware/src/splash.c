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

/* the boot screen in the DX7's colours: the wordmark, then an algorithm as the voice list draws it (sharp
 * operator boxes, ui_voice.c ve_draw_alg), the version in the red LED */
static void sloopdx_alg_frame(uint32_t alg)
{
    uint32_t pass;
    char b[4];
    const char *v = FELUCCA_VERSION;
    if (v[0] == 's' && v[1] == 'l' && v[7] == ' ')
        v += 8;                                         /* "sloopDX 1.9" -> "1.9" */
    fmt_int(b, (int32_t)alg + 1);
    for (pass = 0; pass < 2u; pass++) {                 /* 240 x 180 in two halves (the canvas holds 124 rows) */
        cv_begin(240, 90, SPLASH_PANEL);
        cv_oy = -90 * (int32_t)pass;
        cv_text(4, 0, &FONT_S, "algorithm", RGB(206, 200, 194));
        cv_text(4, 16, &FONT_L, b, RGB(255, 46, 34));
        cv_text(236 - text_w(&FONT_S, v), 0, &FONT_S, v, RGB(230, 209, 185));
        ve_draw_alg(alg, RGB(244, 192, 203));
        cv_text(120 - text_w(&FONT_S, "based on sloop + felucca") / 2, 166, &FONT_S, "based on sloop + felucca", RGB(140, 132, 128));
        cv_oy = 0;
        cv_blit(0, 60u + 90u * pass);
    }
}

static void sloopdx_splash(void)
{
    lcd_fill(0, 0, 240, 240, SPLASH_PANEL);
    sloopdx_logo_draw(2);
    sloopdx_alg_frame(0);
}

/* the boot screen held longer: the 32 algorithms go by as on a DX7 being browsed, then algorithm 1 stays */
static void sloopdx_splash_run(void)
{
    uint32_t a, k;
    for (a = 0; a < 32u; a++) {
        sloopdx_alg_frame(a);
        for (k = 0; k < 7u; k++) {                      /* ~70 ms a frame */
            fm1_wdt_feed();
            fm1_delay_ms(10);
        }
    }
    sloopdx_alg_frame(0);
    for (k = 0; k < 80u; k++) {                         /* then it stays ~0.8 s */
        fm1_wdt_feed();
        fm1_delay_ms(10);
    }
}
