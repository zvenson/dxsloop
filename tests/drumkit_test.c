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
    int32_t l[CTL], r[CTL], rv[CTL], dl[CTL] = {0};
    for (j = 0; j < blocks; j++) {
        memset(l, 0, sizeof l), memset(r, 0, sizeof r), memset(rv, 0, sizeof rv);
        drums_render(l, r, rv, dl, CTL);
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

/* one hit of lane l in kit kit with the macros of dext as set: blocks until it ends, peaks of L / R and the
 * reverb send, the zero crossings of L in its first 100 ms (the pitch / the noise) */
typedef struct { uint32_t blocks, zc; int32_t pl, pr, ps; uint64_t e; } hitst_t;
static hitst_t lane_hit(uint32_t kit, uint32_t l)
{
    hitst_t h = {0};
    int32_t L[CTL], R[CTL], V[CTL], DL[CTL] = {0}, last = 0;
    uint32_t k;
    memset(&drums, 0, sizeof drums);
    TDRUM->p[P_E0] = (int16_t)kit;
    song.g[G_DRLVL] = 100;
    song.g[G_DRREV] = 64;
    drum_on(LANE_NOTE[l], 110);
    while (h.blocks < FS * 9u / CTL) {
        memset(L, 0, sizeof L), memset(R, 0, sizeof R), memset(V, 0, sizeof V);
        drums_render(L, R, V, DL, CTL);
        for (k = 0; k < CTL; k++) {
            h.pl = abs(L[k]) > h.pl ? abs(L[k]) : h.pl;
            h.pr = abs(R[k]) > h.pr ? abs(R[k]) : h.pr;
            h.ps = abs(V[k]) > h.ps ? abs(V[k]) : h.ps;
            h.e += (uint64_t)abs(L[k]) + (uint64_t)abs(R[k]);
            if (h.blocks * CTL + k < FS / 10u) h.zc += (L[k] < 0) != (last < 0);
            last = L[k];
        }
        h.blocks++;
        if (!active_n() && !drums.tail)
            break;
    }
    return h;
}

int main(int argc, char **argv)
{
    uint32_t kit, lane, j;
    int32_t peak;
    uint64_t e;
    int bad = 0;
    FILE *rep = argc > 2 ? fopen(argv[2], "w") : stdout;
    {   /* the noise operator (dx7_core.c dx_op_noise): one carrier (alg 32 OP1, fixed 9.7 kHz, a decay) as noise:
         * bounded, broadband (many zero crossings), the same for the same seed, it ends; a sine at its place is
         * tonal; dx_init never leaves noise on (a synth voice) */
        static dxv_t nv;
        uint8_t p[156];
        int32_t b[DX_N], pk = 0;
        uint32_t zc = 0, n = 0, blk, sum1 = 0, sum2 = 0, round;
        int32_t tail = 0;
        memcpy(p, DX_SYNTH[DX_NSYNTH - 1], 156);     /* INIT VOICE: alg 1, OP1 alone */
        p[5 * 21 + 17] = 1, p[5 * 21 + 18] = 3, p[5 * 21 + 19] = 99;   /* OP1 fixed ~9.7 kHz */
        p[5 * 21 + 1] = 60, p[5 * 21 + 5] = 0, p[5 * 21 + 6] = 0;      /* R2 60 down to L2 0 */
        for (round = 0; round < 2u; round++) {
            int32_t last = 0;
            dx_init(&nv, p, 60, 100);
            if (nv.noise) bad = 1, printf("noise: dx_init left noise on\n");
            nv.noise = 1u << 5, nv.nseed = 12345u;
            for (blk = 0; blk < 44100u / DX_N; blk++) {
                memset(b, 0, sizeof b);
                dx_compute(&nv, b, 0);
                for (j = 0; j < DX_N; j++) {
                    int32_t x = b[j] >> 11;
                    pk = abs(x) > pk ? abs(x) : pk;
                    if (blk < 200u) { zc += (x < 0) != (last < 0); n++; }
                    last = x;
                    if (blk >= 44100u / DX_N - 20u) tail = abs(x) > tail ? abs(x) : tail;
                    if (round) sum2 = sum2 * 31u + (uint32_t)x; else sum1 = sum1 * 31u + (uint32_t)x;
                }
            }
        }
        if (!(pk > 2000 && pk < 70000 && zc > n / 5u && sum1 == sum2 && tail < 16)) {
            printf("noise operator: peak %d, crossings %u of %u, repeatable %d, tail %d\n", pk, zc, n, sum1 == sum2, tail);
            bad = 1;
        } else {
            printf("noise operator: bounded (peak %d), broadband (%u%% crossings), repeatable, ends\n", pk, zc * 100u / n);
        }
    }
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

    {   /* the drum bus (drums_bus): DRIVE and COMP off: as before (golden renders); COMP: denser, the body comes up
         * more than the peaks (crest factor) with its make-up; DRIVE: louder and bounded; never a stuck state */
        static const uint8_t BEAT[8] = {1, 0, 4, 0, 1, 1, 4, 0};   /* 1 kick, 4 snare */
        uint32_t step = FS * 60u / 120u / 4u / CTL, s, b, mode;
        double peak[3], rms[3];
        for (mode = 0; mode < 3u; mode++) {
            double sq = 0, pk = 0;
            uint32_t ns = 0;
            int32_t l[CTL], r[CTL], rv[CTL], dl[CTL] = {0};
            memset(&drums, 0, sizeof drums);
            TDRUM->p[P_E0] = 0;
            TDRUM->p[P_DIST] = mode == 2u ? 90 : 0;
            TDRUM->p[P_CHOR] = mode == 1u ? 110 : 0;
            for (s = 0; s < 32u; s++) {
                if (BEAT[s % 8u] & 1u) drum_on(LANE_NOTE[0], 120);
                if (BEAT[s % 8u] & 4u) drum_on(LANE_NOTE[2], 110);
                drum_on(LANE_NOTE[4], 80);
                for (b = 0; b < step; b++) {
                    memset(l, 0, sizeof l), memset(r, 0, sizeof r), memset(rv, 0, sizeof rv);
                    drums_render(l, r, rv, dl, CTL);
                    for (j = 0; j < CTL; j++) {
                        double x = l[j];
                        sq += x * x, ns++;
                        pk = fabs(x) > pk ? fabs(x) : pk;
                    }
                }
            }
            peak[mode] = pk, rms[mode] = sqrt(sq / ns);
        }
        TDRUM->p[P_DIST] = TDRUM->p[P_CHOR] = 0;
        printf("drum bus: off peak %.0f rms %.0f, COMP peak %.0f rms %.0f, DRIVE peak %.0f rms %.0f\n",
               peak[0], rms[0], peak[1], rms[1], peak[2], rms[2]);
        if (!(peak[1] / rms[1] < 0.95 * peak[0] / rms[0] && rms[1] > rms[0])) {   /* denser: the body up more than the peaks */
            printf("drum bus: COMP does not tame the peaks (crest %.2f -> %.2f)\n", peak[0] / rms[0], peak[1] / rms[1]);
            bad = 1;
        }
        if (!(rms[2] > rms[0] && peak[2] < 140000.0)) {
            printf("drum bus: DRIVE not louder or not bounded\n");
            bad = 1;
        }
    }

    {   /* the lane macros (drums.c dext.m) and a step lock (drum_lock): each does what it says, 0 is the kit */
        hitst_t k0, k1, s0, s1, h0, h1, o0;
        uint32_t fails0 = (uint32_t)bad;
        memset(&dext, 0, sizeof dext);
        k0 = lane_hit(0, 0);                                   /* DX KIT kick as the kit */
        dext.m[0][DM_TUNE] = 12;
        k1 = lane_hit(0, 0);
        if (!(k1.zc > k0.zc * 3u / 2u)) printf("macro TUNE +12: crossings %u -> %u\n", k0.zc, k1.zc), bad = 1;
        dext.m[0][DM_TUNE] = 0, dext.m[0][DM_LEVEL] = -40;
        k1 = lane_hit(0, 0);
        if (!(k1.pl * 6 < k0.pl && k1.pl * 14 > k0.pl)) printf("macro LEVEL -20 dB: peak %d -> %d\n", k0.pl, k1.pl), bad = 1;
        dext.m[0][DM_LEVEL] = 0, dext.m[0][DM_PAN] = -64;
        k1 = lane_hit(0, 0);
        if (!(k1.pr < k0.pr / 50 && k1.pl >= k0.pl * 9 / 10)) printf("macro PAN L64: right %d left %d\n", k1.pr, k1.pl), bad = 1;
        dext.m[0][DM_PAN] = 0, dext.m[0][DM_REV] = -64;
        k1 = lane_hit(0, 0);
        if (!(k1.ps == 0 && k0.ps > 0)) printf("macro REV 0: send %d (kit %d)\n", k1.ps, k0.ps), bad = 1;
        dext.m[0][DM_REV] = 0, dext.m[0][DM_SWEEP] = 40;
        k1 = lane_hit(0, 0);
        if (!(k1.zc > k0.zc)) printf("macro SWEEP +40: crossings %u -> %u\n", k0.zc, k1.zc), bad = 1;
        dext.m[0][DM_SWEEP] = 0;
        h0 = lane_hit(0, 5);                                   /* the open hat: DECAY */
        dext.m[5][DM_DECAY] = 40;
        h1 = lane_hit(0, 5);
        if (!(h1.blocks > h0.blocks * 3u / 2u)) printf("macro DECAY +40: %u -> %u blocks\n", h0.blocks, h1.blocks), bad = 1;
        dext.m[5][DM_DECAY] = -40;
        h1 = lane_hit(0, 5);
        if (!(h1.blocks < h0.blocks)) printf("macro DECAY -40: %u -> %u blocks\n", h0.blocks, h1.blocks), bad = 1;
        dext.m[5][DM_DECAY] = 0;
        s0 = lane_hit(0, 2);                                   /* the snare: NOISE off leaves the body */
        dext.m[2][DM_NOISE] = -40;
        s1 = lane_hit(0, 2);
        if (!(s1.zc * 5u < s0.zc * 4u && s1.pl > 0)) printf("macro NOISE -40: crossings %u -> %u\n", s0.zc, s1.zc), bad = 1;
        dext.m[2][DM_NOISE] = 0, dext.m[2][DM_BRIGHT] = -40;
        s1 = lane_hit(0, 2);
        if (!(s1.e != s0.e)) printf("macro BRIGHT: no change\n"), bad = 1;
        dext.m[2][DM_BRIGHT] = 0;
        /* CHOKE: the open hat rings on under a closed hat once its group is off */
        memset(&drums, 0, sizeof drums);
        drum_on(LANE_NOTE[5], 110);
        run(20, 0, 0);
        drum_on(LANE_NOTE[4], 110);
        o0.blocks = active_n();
        dext.m[5][DM_CHOKE] = 1;
        memset(&drums, 0, sizeof drums);
        drum_on(LANE_NOTE[5], 110);
        run(20, 0, 0);
        drum_on(LANE_NOTE[4], 110);
        if (!(o0.blocks == 1u && active_n() == 2u)) printf("macro CHOKE off: %u / %u voices\n", o0.blocks, active_n()), bad = 1;
        dext.m[5][DM_CHOKE] = 0;
        /* a lock: TUNE +12 on the kick of this step only */
        drum_lock = dlock_make(0, 1, 12, 0, 0);
        k1 = lane_hit(0, 0);
        drum_lock = 0;
        h1 = lane_hit(0, 0);
        if (!(k1.zc > k0.zc * 3u / 2u && h1.zc == k0.zc && dlock_tune(dlock_make(3, 1, -7, 1, -21)) == -7 &&
              dlock_decay(dlock_make(3, 1, -7, 1, -21)) == -21 && dlock_lane(dlock_make(3, 1, -7, 1, -21)) == 3u))
            printf("lock: tune %u / %u / %u\n", k1.zc, h1.zc, k0.zc), bad = 1;
        memset(&dext, 0, sizeof dext);
        printf("drum lane macros (TUNE DECAY SWEEP BRIGHT NOISE LEVEL PAN CHOKE REV) and locks: %s\n", (uint32_t)bad != fails0 ? "FAIL" : "ok");
    }

    {   /* MY KIT: the dice (the same kit for the same seed; 24 seeds x 16 lanes bounded, audible, ending), the bake
         * (kit + macros -> MY KIT plays the same), the .syx (the 4096 voice bytes there and back, exactly) */
        static uint8_t a[DRUM_LANES][156], syx[4096];
        static ukit_img_t i1;
        uint32_t seed, ok = 1, l, fails0 = (uint32_t)bad;
        hitst_t x1;
        dice_kit(4711);
        memcpy(a, ukit.v, sizeof a);
        dice_kit(4712);
        ok &= memcmp(a, ukit.v, sizeof a) != 0;
        dice_kit(4711);
        ok &= !memcmp(a, ukit.v, sizeof a) && ukit.seed == 4711u;
        if (!ok) printf("dice: not repeatable / not different\n"), bad = 1;
        for (seed = 1; seed <= 24u; seed++) {
            dice_kit(seed * 977u);
            for (l = 0; l < DRUM_LANES; l++) {
                hitst_t h = lane_hit(KIT_USER, l);
                if (!(h.pl > 300 && h.pl < 120000 && h.blocks < FS * 6u / CTL)) {
                    printf("dice %u lane %s: peak %d, %u blocks\n", seed * 977u, LANE_SHORT[l], h.pl, h.blocks);
                    bad = 1;
                }
            }
        }
        memset(&dext, 0, sizeof dext);                       /* the bake: 808 FM with macros = MY KIT without */
        dext.m[0][DM_TUNE] = 5, dext.m[0][DM_DECAY] = 20, dext.m[2][DM_NOISE] = -10, dext.m[5][DM_PAN] = 30;
        dext.m[5][DM_LEVEL] = -6, dext.m[0][DM_SWEEP] = 10, dext.m[2][DM_REV] = 20, dext.m[3][DM_BRIGHT] = -8;
        {
            static hitst_t want[DRUM_LANES];
            for (l = 0; l < DRUM_LANES; l++)
                want[l] = lane_hit(1, l);
            ukit_bake(1);
            for (l = 0; l < DRUM_LANES; l++) {
                if (dext.m[l][DM_TUNE] || dext.m[l][DM_PAN]) ok = 0;
                x1 = lane_hit(KIT_USER, l);
                if (!(x1.zc == want[l].zc && x1.blocks == want[l].blocks && (double)x1.e > 0.97 * (double)want[l].e &&
                      (double)x1.e < 1.03 * (double)want[l].e && abs(x1.pr - want[l].pr) * 30 <= want[l].pr + 30)) {
                    printf("bake lane %s: crossings %u/%u blocks %u/%u energy %llu/%llu\n", LANE_SHORT[l], x1.zc,
                           want[l].zc, x1.blocks, want[l].blocks, (unsigned long long)x1.e, (unsigned long long)want[l].e);
                    bad = 1;
                }
            }
        }
        dice_kit(321);                                       /* the .syx: there and back */
        ukit_to_img(&i1);
        ukit_syx_put(syx);
        ukit_from(0);
        if (!ukit_syx_is_kit(syx) || ukit_syx_get(syx) || ukit.seed != 321u)
            printf("kit syx: not read back\n"), bad = 1;
        ukit_to_img(&ukit_img);
        if (memcmp(&i1, &ukit_img, sizeof i1))
            printf("kit syx: changed on the way\n"), bad = 1;
        {   /* the reference for the web editor (web/test_web.mjs: its .syx <-> image conversion, the same bytes) */
            FILE *f = fopen("build/host/mykit.img", "wb");
            uint32_t sum = 0, q;
            if (f) { fwrite(&i1, 1, sizeof i1, f); fclose(f); }
            f = fopen("build/host/mykit.syx", "wb");
            if (f) {
                static const uint8_t H[6] = {0xF0, 0x43, 0x00, 0x09, 0x20, 0x00};
                for (q = 0; q < 4096u; q++) sum += syx[q];
                fwrite(H, 1, 6, f); fwrite(syx, 1, 4096, f);
                fputc((int)((128u - (sum & 127u)) & 127u), f); fputc(0xF7, f);
                fclose(f);
            }
        }
        syx[(DRUM_LANES + 3u) * 128u + 120u] = 'X';
        if (ukit_syx_get(syx) != 1)
            printf("kit syx: a bank that is not a kit is taken\n"), bad = 1;
        if (!ok) printf("bake: macros not back to 0\n"), bad = 1;
        memset(&dext, 0, sizeof dext);
        ukit_from(0);
        printf("MY KIT: dice repeatable, 24 seeds x 16 lanes, bake, .syx: %s\n", (uint32_t)bad != fails0 ? "FAIL" : "ok");
    }

    /* demo: each kit plays two bars of a beat at 120 BPM */
    if (argc > 1) {
        static const uint8_t BEAT[16] = {0x31, 0x10, 0x10, 0x10, 0x1C, 0x10, 0x31, 0x20,
                                         0x10, 0x11, 0x10, 0x10, 0x1C, 0x10, 0x30, 0x30};
        uint32_t step = FS * 60u / 120u / 4u / CTL, s, b;
        FILE *w = fopen(argv[1], "wb");
        int32_t l[CTL], r[CTL], rv[CTL], dl[CTL] = {0};
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
                    drums_render(l, r, rv, dl, CTL);
                    for (j = 0; j < CTL; j++)
                        wav_put(w, l[j] / 2, r[j] / 2);   /* (the raw drum bus: no master stage here) */
                }
            }
        }
        fclose(w);
    }
    printf("FM drum kits: %u kits x %u lanes bounded, audible, end; choke, burst, voice reuse, click: %s\n",
           DRUM_KITS, DRUM_LANES, bad ? "FAIL" : "PASS");
    return bad;
}
