/* SPDX-License-Identifier: GPL-3.0-only */
/* zvenFM boot splash: the logo (tools/gen_logo.py -> zvenfm_logo.h: 16 colours, RLE runs of
 * (run - 1) << 4 | colour), drawn into the canvas in bands of at most 120 rows. */
#include "zvenfm_logo.h"
static void zven_logo_draw(uint32_t y0)
{
    uint32_t r0;
    for (r0 = 0; r0 < ZVEN_SPLASH_H; r0 += 120u) {
        uint32_t rows = ZVEN_SPLASH_H - r0 > 120u ? 120u : ZVEN_SPLASH_H - r0;
        uint32_t lo = r0 * ZVEN_SPLASH_W, hi = (r0 + rows) * ZVEN_SPLASH_W, pos = 0, i;
        cv_begin(ZVEN_SPLASH_W, rows, C_BLACK);
        for (i = 0; i < sizeof ZVEN_SPLASH_RLE && pos < hi; i++) {
            uint32_t run = (ZVEN_SPLASH_RLE[i] >> 4) + 1u, c = ZVEN_SPLASH_RLE[i] & 15u, p;
            if (c && pos + run > lo)
                for (p = pos < lo ? lo : pos; p < pos + run && p < hi; p++)
                    cv_px[p - lo] = swap16(ZVEN_SPLASH_PAL[c]);
            pos += run;
        }
        cv_blit((240u - ZVEN_SPLASH_W) / 2u, y0 + r0);
    }
}

/* the boot screen: the logo, the version, where it comes from */
static void zven_splash(void)
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
    zven_logo_draw(10);
    const char *v = FELUCCA_VERSION;
    if (v[0] == 'z' && v[1] == 'v' && v[6] == ' ')
        v += 7;                                         /* "zvenFM 1.0" -> "1.0" under the wordmark */
    cv_begin(240, 36, C_BLACK);
    cv_text(120 - text_w(&FONT_S, v) / 2, 0, &FONT_S, v, RGB(196, 196, 204));
    cv_text(120 - text_w(&FONT_S, "fm synth - sloop / felucca") / 2, 18, &FONT_S, "fm synth - sloop / felucca", RGB(96, 96, 104));
    cv_blit(0, 202);
}
