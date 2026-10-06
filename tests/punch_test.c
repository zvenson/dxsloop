/* SPDX-License-Identifier: GPL-3.0-only */
/* PUNCH-IN FX (punch.c): each of the 16 effects changes the mix while held, stays bounded,
 * and the mix is exactly the dry one again once its key is up (after the 64-sample fade).
 * The FX-held keyboard picks effects with the white keys and never plays or records a note.
 * argv[1]: a WAV demo (a beat, then every effect for one beat). */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
#define BLOCKS (FS * 3u / CTL)                        /* 3 s at 120 BPM */
static int32_t got[CTL * 2u];

static void beat_setup(void)
{
    static const uint8_t B[16][2] = {{36, 42}, {42}, {42}, {42}, {38, 42}, {42}, {42}, {36, 46},
                                     {36, 42}, {42}, {36}, {42}, {38, 42}, {42}, {42}, {46}};
    uint32_t j, q;
    host_tracks_init();
    memset(&drums, 0, sizeof drums);
    
    TDRUM->p[P_E0] = 0;                               /* the sampled kit: no random noise */
    song.g[G_BPM] = 120;
    for (j = 0; j < NSTEP; j++) {
        step_t *s = &TDRUM->step[j];
        memset(s, 0, sizeof *s);
        s->time = ST_REST;
        if (j < 16u)
            for (q = 0; q < 2u && B[j][q]; q++) {
                s->note[s->n++] = B[j][q];
                s->time = ST_NOTE;
                s->vel = 100;
            }
    }
    TDRUM->p[P_SLEN] = 16;
    punch.req = -1;
    punch.cur = -1;
    punch.g = 0;
    punch.hold = 0;
    clk_beat = 0;
    clk_pos = 0;
    transport_req = 1;
}

/* a deterministic "mix": a decaying 110 Hz tone every quarter + noise bursts every 1/16 */
static void test_mix(uint32_t b, int32_t *l, int32_t *r)
{
    static uint32_t seed = 1;
    uint32_t i;
    if (b == 0) seed = 1;
    for (i = 0; i < CTL; i++) {
        uint32_t t = b * CTL + i, q = t % (FS / 2u), s16 = t % (FS / 8u);
        int32_t tone = (sine_i((uint32_t)t * 10712093u) * (int32_t)(FS / 2u - q) / (int32_t)(FS / 2u)) >> 1;
        int32_t nz = s16 < 2000u ? (int32_t)((seed = seed * 1664525u + 1013904223u) >> 18) - 8192 : 0;
        l[i] = (tone + nz) * 3;
        r[i] = (tone - nz) * 3;
    }
}

int main(int argc, char **argv)
{
    uint32_t fx, i, b, on = FS / CTL, off = on + FS / 2u / CTL;   /* held from 1 s to 1.5 s */
    FILE *f = argc > 1 ? fopen(argv[1], "wb") : 0;
    for (fx = 0; fx < PUNCH_NFX; fx++) {
        uint64_t diff = 0;
        beat_setup();
        song.playing = 1;
        clk_pos = 0;
        for (b = 0; b < BLOCKS; b++) {
            int32_t l[CTL], r[CTL], l0[CTL], r0[CTL];
            test_mix(b, l, r);
            memcpy(l0, l, sizeof l), memcpy(r0, r, sizeof r);
            if (b == on) punch.req = (int8_t)fx;
            if (b == off) punch.req = -1;
            punch_process(l, r, CTL);
            clk_pos = (clk_pos + CTL * (uint32_t)song.g[G_BPM]) % BEAT_U;   /* (the transport clock) */
            for (i = 0; i < CTL; i++) {
                assert(l[i] > -(1 << 22) && l[i] < (1 << 22) && r[i] > -(1 << 22) && r[i] < (1 << 22));
                if (b < on || b >= off + 3u)
                    assert(l[i] == l0[i] && r[i] == r0[i]);   /* dry before, dry again after */
                else
                    diff += (uint64_t)(l[i] > l0[i] ? l[i] - l0[i] : l0[i] - l[i]);
            }
        }
        if (diff <= 2000u * (off - on))
            printf("fx %u (%s): diff %llu\n", fx, PUNCH_NAME[fx], (unsigned long long)diff);
        assert(diff > 2000u * (off - on));            /* it did something */
        assert(punch.cur == -1);
        song.playing = 0;
    }
    {   /* the keyboard while FX is held: white keys pick effects; nothing sounds, nothing records */
        uint32_t k;
        beat_setup();
        song.sel = 0;
        song.rec = 1u;
        mix_block(got, CTL);
        ly_bit[LY_FX] = 1u << 2;                      /* FX held (seq.c layer_now) */
        fm1_in.buttons = ly_bit[LY_FX];
        for (k = 0; k < 27u; k++) {
            int32_t w = punch_key(k);
            fm1_in.notes = 1u << k;
            events_block(CTL);
            assert(w < 0 ? 1 : punch.req == w);
            assert(!trk[0].step[0].n && !trk[0].v[0].active);
            fm1_in.notes = 0;
            events_block(CTL);
            assert(punch.req == -1);
        }
        assert(punch_key(0) == 0 && punch_key(26) == 15);
        fm1_in.buttons = 0;
        song.rec = 0;
        transport_req = 2;
        events_block(CTL);
    }
    if (f) {   /* demo: one beat dry, then each effect for one beat with a beat dry between */
        uint32_t beat = FS / 2u / CTL, b, total = beat * (1u + 2u * PUNCH_NFX);
        static int32_t blk[CTL * 2u];
        wav_hdr(f, total * CTL);
        beat_setup();
        TDRUM->p[P_E0] = 1u;                         /* 808 FM */
        for (b = 0; b < total; b++) {
            uint32_t q;
            int32_t e = b < beat ? -1 : (int32_t)((b - beat) / beat);
            punch.req = (int8_t)(e >= 0 && e % 2 == 0 ? e / 2 : -1);
            mix_block(blk, CTL);
            for (q = 0; q < CTL; q++) wav_put(f, blk[2 * q], blk[2 * q + 1]);
        }
        fclose(f);
    }
    printf("punch-in FX: %u effects change the mix while held, bounded, exact dry mix after release; "
           "FX-held keys pick effects, play and record nothing PASS\n", PUNCH_NFX);
    return 0;
}
