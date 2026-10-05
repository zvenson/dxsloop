/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Sven Trogus
 * Tables for the DX7 engine (firmware/src/eng_dx7.c), computed on the build machine with the
 * same double / float expressions as Dexed's msfa core (Apache-2.0, (C) 2012 Google, 2016-2025
 * Pascal Gauthier), so the target needs no floating point and renders what Dexed renders.
 *   cc -O2 -o build/gen_dx7_tables tools/gen_dx7_tables.c -lm
 *   build/gen_dx7_tables > firmware/src/dx7_tables.h
 * FS 44100 Hz. Every table is independent of the block size except DX_LFO_* and DX_PENV_UNIT,
 * which are written for both block sizes (32 = SLOOP's CTL, 64 = Dexed's N, for the tests). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define FS 44100.0

static void arr32(const char *name, const int32_t *v, int n)
{
    int i;
    printf("static const int32_t %s[%d] = {", name, n);
    for (i = 0; i < n; i++)
        printf("%s%d,", i % 8 ? " " : "\n    ", v[i]);
    printf("\n};\n");
}

static void arru32(const char *name, const uint32_t *v, int n)
{
    int i;
    printf("static const uint32_t %s[%d] = {", name, n);
    for (i = 0; i < n; i++)
        printf("%s%uu,", i % 8 ? " " : "\n    ", v[i]);
    printf("\n};\n");
}

int main(void)
{
    static int32_t sintab[2048], exp2tab[2048], flut[1025], det[128 * 15], fine[100];
    static uint32_t lfod32[100], lfod64[100], ampk[1025];
    static const double lfoSource[100] = {
        0.062541, 0.125031, 0.312393, 0.437120, 0.624610, 0.750694, 0.936330, 1.125302, 1.249609, 1.436782,
        1.560915, 1.752081, 1.875117, 2.062494, 2.247191, 2.374451, 2.560492, 2.686728, 2.873976, 2.998950,
        3.188013, 3.369840, 3.500175, 3.682224, 3.812065, 4.000800, 4.186202, 4.310716, 4.501260, 4.623209,
        4.814636, 4.930480, 5.121901, 5.315191, 5.434783, 5.617346, 5.750431, 5.946717, 6.062811, 6.248438,
        6.431695, 6.564264, 6.749460, 6.868132, 7.052186, 7.250580, 7.375719, 7.556294, 7.687577, 7.877738,
        7.993605, 8.181967, 8.372405, 8.504848, 8.685079, 8.810573, 8.986341, 9.122423, 9.300595, 9.500285,
        9.607994, 9.798158, 9.950249, 10.117361, 11.251125, 11.384335, 12.562814, 13.676149, 13.904338, 15.092062,
        16.366612, 16.638935, 17.869907, 19.193858, 19.425019, 20.833333, 21.034918, 22.502250, 24.003841, 24.260068,
        25.746653, 27.173913, 27.578599, 29.052876, 30.693677, 31.191516, 32.658393, 34.317090, 34.674064, 36.416606,
        38.197097, 38.550501, 40.387722, 40.749796, 42.625746, 44.326241, 44.883303, 46.772685, 48.590865, 49.261084};
    int i, n;

    /* Sin::init (SIN_DELTA): pairs (delta, value), 1024 points, Q24 */
    {
        double dphase = 2 * M_PI / 1024;
        int32_t c = (int32_t)floor(cos(dphase) * (1 << 30) + 0.5), s = (int32_t)floor(sin(dphase) * (1 << 30) + 0.5);
        int32_t u = 1 << 30, v = 0;
        const int32_t R = 1 << 29;
        for (i = 0; i < 512; i++) {
            int32_t t;
            sintab[(i << 1) + 1] = (v + 32) >> 6;
            sintab[((i + 512) << 1) + 1] = -((v + 32) >> 6);
            t = (int32_t)(((int64_t)u * s + (int64_t)v * c + R) >> 30);
            u = (int32_t)(((int64_t)u * c - (int64_t)v * s + R) >> 30);
            v = t;
        }
        for (i = 0; i < 1023; i++)
            sintab[i << 1] = sintab[(i << 1) + 3] - sintab[(i << 1) + 1];
        sintab[2046] = -sintab[2047];
    }
    /* Exp2::init: pairs (delta, value), Q30 */
    {
        double inc = exp2(1.0 / 1024), y = 1 << 30;
        for (i = 0; i < 1024; i++) {
            exp2tab[(i << 1) + 1] = (int32_t)floor(y + 0.5);
            y *= inc;
        }
        for (i = 0; i < 1023; i++)
            exp2tab[i << 1] = exp2tab[(i << 1) + 3] - exp2tab[(i << 1) + 1];
        exp2tab[2046] = (int32_t)((1U << 31) - (uint32_t)exp2tab[2047]);
    }
    /* Freqlut::init(44100) */
    {
        double y = (1LL << (24 + 20)) / FS, inc = pow(2, 1.0 / 1024);
        for (i = 0; i < 1025; i++) {
            flut[i] = (int32_t)floor(y + 0.5);
            y *= inc;
        }
    }
    /* osc_freq, ratio mode: the detuned log frequency for each note (standard tuning) and DETUNE 0..14 */
    for (n = 0; n < 128; n++) {
        int32_t base = 50857777 + ((1 << 24) / 12) * n;
        for (i = 0; i < 15; i++) {
            int32_t logfreq = base;
            double detuneRatio = 0.0209 * exp(-0.396 * (((float)logfreq) / (1 << 24))) / 7;
            logfreq += detuneRatio * logfreq * (i - 7);
            det[n * 15 + i] = logfreq;
        }
    }
    for (i = 0; i < 100; i++)                                  /* osc_freq: FINE */
        fine[i] = i ? (int32_t)floor(24204406.323123 * log(1 + 0.01 * i) + 0.5) : 0;
    for (i = 0; i < 100; i++) {                                /* Lfo::reset: delta = source * ratio */
        uint32_t r32 = (uint32_t)(4437500000.0 * 32 / FS), r64 = (uint32_t)(4437500000.0 * 64 / FS);
        lfod32[i] = (uint32_t)(lfoSource[i] * r32);
        lfod64[i] = (uint32_t)(lfoSource[i] * r64);
    }
    /* amp mod depth (Dx7Note::compute): pt = exp(sensamp / 262144 * 0.07 + 12.2), sensamp 0..2^24 in 1024 steps */
    for (i = 0; i <= 1024; i++) {
        uint32_t sensamp = (uint32_t)i << 14;
        ampk[i] = (uint32_t)exp(((float)sensamp) / 262144 * 0.07 + 12.2);
    }

    printf("/* generated by tools/gen_dx7_tables.c: the DX7 engine's tables (from Dexed's msfa) */\n#pragma once\n");
    arr32("DX_SINTAB", sintab, 2048);
    arr32("DX_EXP2TAB", exp2tab, 2048);
    arr32("DX_FREQLUT", flut, 1025);
    arr32("DX_DETUNED", det, 128 * 15);
    arr32("DX_FINE", fine, 100);
    arru32("DX_LFO_DELTA32", lfod32, 100);
    arru32("DX_LFO_DELTA64", lfod64, 100);
    arru32("DX_AMS_PT", ampk, 1025);
    printf("#define DX_LFO_UNIT32 %du\n#define DX_LFO_UNIT64 %du\n", (int32_t)(32 * 25190424 / FS + 0.5),
           (int32_t)(64 * 25190424 / FS + 0.5));
    printf("#define DX_PENV_UNIT32 %d\n#define DX_PENV_UNIT64 %d\n", (int)(32 * (1 << 24) / (21.3 * FS) + 0.5),
           (int)(64 * (1 << 24) / (21.3 * FS) + 0.5));
    return 0;
}
