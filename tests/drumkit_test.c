/* SPDX-License-Identifier: GPL-3.0-only */
/* FM drum kits (drums.c, the DX7 core): every kit x every lane is bounded, audible and ends; the
 * lanes of a kit stay within a level window; choke groups, the clap's burst, the click, voice reuse;
 * the host cost of 6 FM drum voices. argv[1]: a WAV demo (each kit plays two bars), argv[2]: a report. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>

static uint32_t active_n(void)
{
    uint32_t k, n = 0;
    for (k = 0; k < NDRUM; k++)
        n += drums.v[k].active;
    return n;
}
static void run(uint32_t blocks, int32_t *peak, uint64_t *energy)
{
    uint32_t j, k;
    int32_t l[CTL], r[CTL], rv[CTL];
    for (j = 0; j < blocks; j++) {
        memset(l, 0, sizeof l), memset(r, 0, sizeof r), memset(rv, 0, sizeof rv);
        drums_render(l, r, rv, CTL);
        for (k = 0; k < CTL; k++) {
            int32_t a = l[k] < 0 ? -l[k] : l[k];
            assert(a < 131072);
            if (peak && a > *peak)
                *peak = a;
            if (energy)
                *energy += (uint64_t)a;
        }
    }
}
static uint32_t one_hit(uint32_t kit, uint32_t note, int32_t *peak, uint64_t *energy)
{
    uint32_t blocks = 0;
    memset(&drums, 0, sizeof drums);
    TDRUM->p[P_E0] = (int16_t)kit;
    song.g[G_DRLVL] = 100;
    drum_on(note, 110);
    *peak = 0;
    *energy = 0;
    while (blocks < FS * 9u / CTL) {
        run(1, peak, energy);
        blocks++;
        if (!active_n())
            break;
    }
    return blocks;
}

int main(int argc, char **argv)
{
    uint32_t kit, lane, j;
    int32_t peak;
    uint64_t e;
    int bad = 0;
    FILE *rep = argc > 2 ? fopen(argv[2], "w") : stdout;
    host_tracks_init();

    /* every kit x lane: audible, bounded, ends within 8 s; per kit the loudest / quietest lane */
    for (kit = 0; kit < DRUM_KITS; kit++) {
        int32_t lo = 1 << 30, hi = 0;
        fprintf(rep, "%-7s", DRUM_KIT_NAMES[kit]);
        for (lane = 0; lane < DRUM_LANES; lane++) {
            uint32_t b = one_hit(kit, LANE_NOTE[lane], &peak, &e);
            double ms = b * 1000.0 * CTL / FS;
            fprintf(rep, " %s %5d %4.0fms", LANE_SHORT[lane], peak, ms);
            if (peak < 1500 || b >= FS * 8u / CTL) {
                fprintf(rep, " <- %s", peak < 1500 ? "TOO QUIET" : "NO END");
                bad = 1;
            }
            lo = peak < lo ? peak : lo;
            hi = peak > hi ? peak : hi;
        }
        fprintf(rep, "\n        loudest / quietest lane %.1f dB\n", 20.0 * log10((double)hi / lo));
        if (hi > 4 * 32767) bad = 1;
    }

    /* choke: the open hat stops when the closed hat plays (within 2 blocks: declicked) */
    memset(&drums, 0, sizeof drums);
    TDRUM->p[P_E0] = 0;
    drum_on(LANE_NOTE[5], 110);                     /* HAT OPEN */
    run(FS / 10u / CTL, 0, 0);
    drum_on(LANE_NOTE[4], 110);                     /* HAT CLOSED */
    run(2, 0, 0);
    for (j = 0; j < NDRUM; j++)
        if (drums.v[j].active && drums.drum[j] == 5u) {
            fprintf(rep, "choke: open hat still sounding\n");
            bad = 1;
        }

    /* the clap's burst: several attacks within ~30 ms on one voice */
    memset(&drums, 0, sizeof drums);
    drum_on(LANE_NOTE[3], 110);
    assert(active_n() == 1u && drums.burst[0] > 0);
    run(FS / 20u / CTL, 0, 0);
    if (drums.burst[0]) {
        fprintf(rep, "clap: burst not finished after 50 ms\n");
        bad = 1;
    }

    /* the same drum again reuses its voice; 8 different lanes fit in NDRUM by stealing the oldest */
    memset(&drums, 0, sizeof drums);
    drum_on(LANE_NOTE[0], 110);
    drum_on(LANE_NOTE[0], 110);
    if (active_n() != 1u) bad = 1;
    for (lane = 0; lane < 8u; lane++)
        drum_on(LANE_NOTE[lane + 2u], 100);
    if (active_n() != NDRUM) bad = 1;
    run(FS * 9u / CTL, 0, 0);
    if (active_n()) {
        fprintf(rep, "voices still active after 9 s\n");
        bad = 1;
    }

    /* the click (seq.c, notes 76 / 77: the clave, the accent a fifth up) */
    memset(&drums, 0, sizeof drums);
    peak = 0;
    drum_on(76, 100);
    run(FS / 10u / CTL, &peak, 0);
    if (peak < 1000) { fprintf(rep, "click inaudible\n"); bad = 1; }

    /* host cost: 6 voices sounding (the budget), one second */
    {
        struct timespec t0, t1;
        double ns;
        memset(&drums, 0, sizeof drums);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (j = 0; j < 4u; j++) {
            uint32_t b;
            for (b = 0; b < 6u; b++)
                drum_on(LANE_NOTE[(b * 3u) % DRUM_LANES], 110);
            run(FS / 4u / CTL, 0, 0);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        ns = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
        fprintf(rep, "cost: 6 FM drum voices, 1 s audio: %.1f ms host\n", ns / 1e6);
    }

    /* demo: each kit plays two bars of a beat at 120 BPM */
    if (argc > 1) {
        static const uint8_t BEAT[16] = {0x31, 0x10, 0x10, 0x10, 0x1C, 0x10, 0x31, 0x20,
                                         0x10, 0x11, 0x10, 0x10, 0x1C, 0x10, 0x30, 0x30};
        uint32_t step = FS * 60u / 120u / 4u / CTL, s, b;
        FILE *w = fopen(argv[1], "wb");
        int32_t l[CTL], r[CTL], rv[CTL];
        wav_hdr(w, DRUM_KITS * 32u * step * CTL);
        for (kit = 0; kit < DRUM_KITS; kit++) {
            memset(&drums, 0, sizeof drums);
            TDRUM->p[P_E0] = (int16_t)kit;
            for (s = 0; s < 32u; s++) {
                uint8_t m = BEAT[s % 16u];
                if (m & 0x01) drum_on(LANE_NOTE[0], 120);     /* kick */
                if (m & 0x08) drum_on(LANE_NOTE[2], 110);     /* snare */
                if (m & 0x04) drum_on(LANE_NOTE[3], 90);      /* clap */
                if (m & 0x10) drum_on(LANE_NOTE[4], s % 2u ? 70 : 100);   /* closed hat */
                if (m & 0x20) drum_on(LANE_NOTE[5], 90);      /* open hat */
                if (s == 0u) drum_on(LANE_NOTE[11], 90);      /* crash */
                for (b = 0; b < step; b++) {
                    memset(l, 0, sizeof l), memset(r, 0, sizeof r), memset(rv, 0, sizeof rv);
                    drums_render(l, r, rv, CTL);
                    for (j = 0; j < CTL; j++)
                        wav_put(w, l[j], r[j]);
                }
            }
        }
        fclose(w);
    }
    printf("FM drum kits: %u kits x %u lanes bounded, audible, end; choke, burst, voice reuse, click: %s\n",
           DRUM_KITS, DRUM_LANES, bad ? "FAIL" : "PASS");
    return bad;
}
