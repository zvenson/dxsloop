/* SPDX-License-Identifier: GPL-3.0-only */
/* ARP MODE OMNI (the chord harp) and FLW (a pattern following its chord), through the real keyboard,
 * recording and sequencer paths (seq.c omni_*). */
#include <assert.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

/* the black keys (F3 + k): F# G# A# C# D# F# G# A# C# D# F# -> F C G Dm Am Em G7 E7 D7 Bb A7 */
static const uint8_t BLACK[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};

static int sounding(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].gate && t->v[i].note == note)
            return 1;
    return 0;
}

static int gates(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].gate != 0;
    return n;
}

static void press(uint32_t k) { fm1_in.notes |= 1u << k; keyboard_block(); }
static void lift(uint32_t k) { fm1_in.notes &= ~(1u << k); keyboard_block(); }

static void reset(void)
{
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    kb_prev = 0;
    fm1_in.notes = 0;
    usb.config = 1;
    mo_w = mo_r = 0;
    omni_ch = 0xFFu;
    omni_new = 0;
}

static void keys_test(void)
{
    track_t *t = &trk[0];
    char nm[8];
    uint32_t k, c;
    reset();
    t->p[P_AMODE] = AM_OMNI;
    assert(!ARP_RUNS(t));
    for (k = c = 0; k < 27u; k++)                       /* 11 black keys, 16 white */
        c += OM_BLACK(k);
    assert(c == 11u);
    for (c = 0; c < 11u; c++)
        assert(OM_BLACK(BLACK[c]) && omni_idx(BLACK[c], 1) == c);
    assert(omni_idx(26, 0) == 15u);

    press(BLACK[0]);                                    /* F: F3 A3 C4, sounding and sent */
    assert(omni_ch == 0 && omni_new && sounding(t, 53) && sounding(t, 57) && sounding(t, 60));
    assert(mo_w == 3);
    omni_name(nm);
    assert(!strcmp(nm, "F"));
    assert(t->nheld == 0 && t->arp_phys == 0);          /* not the arpeggiator */
    lift(BLACK[0]);
    assert(gates(t) == 0 && mo_w == 6);

    press(0);                                           /* the strings of F from G3: A3 C4 F4 */
    press(2);
    press(4);
    assert(sounding(t, 57) && sounding(t, 60) && sounding(t, 65));
    press(BLACK[4]);                                    /* Am while the strings ring */
    omni_name(nm);
    assert(omni_ch == 4 && !strcmp(nm, "Am"));
    assert(sounding(t, 57) && sounding(t, 60) && sounding(t, 64));
    lift(0);                                            /* a string lets go of its own note */
    lift(2);
    lift(4);
    lift(BLACK[4]);
    assert(gates(t) == 0);

    press(BLACK[6]);                                    /* G7: G3 B3 F4; its strings G3 B3 D4 F4 */
    assert(sounding(t, 55) && sounding(t, 59) && sounding(t, 65));
    lift(BLACK[6]);
    assert(omni_string(t, 0) == 55 && omni_string(t, 1) == 59 && omni_string(t, 2) == 62 &&
           omni_string(t, 3) == 65 && omni_string(t, 4) == 67);
    for (k = 0; k < 16u; k++)                           /* rising, in range */
        assert(omni_string(t, k) < 128u && (!k || omni_string(t, k) > omni_string(t, k - 1)));

    t->p[P_ROOT] = 5;                                   /* ROOT does not move the buttons */
    press(BLACK[0]);
    omni_name(nm);
    assert(!strcmp(nm, "F") && sounding(t, 53) && sounding(t, 57) && sounding(t, 60));
    lift(BLACK[0]);
    t->p[P_TRANS] = 2;                                  /* TRN +2: the F button is G */
    press(BLACK[0]);
    omni_name(nm);
    assert(!strcmp(nm, "G") && sounding(t, 55) && sounding(t, 59) && sounding(t, 62));
    lift(BLACK[0]);
    song.octave = 1;                                    /* OCT+ moves both up */
    press(BLACK[1]);
    assert(sounding(t, 74) && omni_string(t, 0) == 69);   /* D5 F#5 A5 played, the strings from G4: A4 */
    lift(BLACK[1]);
    assert(gates(t) == 0);
    song.octave = 0;                                    /* a MONO track: still the whole chord */
    t->p[P_TRANS] = 0;
    t->p[P_VOICE] = V_MONO;
    press(BLACK[4]);
    assert(gates(t) == 3 && sounding(t, 57) && sounding(t, 60) && sounding(t, 64));
    press(6);                                           /* the 4th string (B3 key): A4, beside the chord */
    assert(gates(t) == 4 && sounding(t, 69));
    lift(6);
    lift(BLACK[4]);
    assert(gates(t) == 0);
    t->p[P_VOICE] = V_POLY;
    puts("omni: 11 chord keys, 16 strings, chord change under held strings, fixed buttons, TRN, octave, MONO, MIDI out ok");
}

static void record_test(void)
{
    track_t *t = &trk[0];
    step_t *s;
    reset();
    t->p[P_AMODE] = AM_OMNI;
    song.playing = 1;
    song.rec = 1;
    press(BLACK[4]);                                    /* Am recorded as a chord */
    lift(BLACK[4]);
    press(0);                                           /* a string: live only */
    lift(0);
    s = &t->step[0];
    assert(s->n == 3);
    omni_ch = 0;                                        /* played back: the step sets the chord again */
    omni_new = 0;
    seq_step(t, s, 1000, 0);
    assert(omni_ch == 4 && omni_new);
    seq_release(t);
    puts("omni: chords recorded, strings not, a chord step sets the chord on playback ok");
}

static void follow_test(void)
{
    track_t *b = &trk[1];
    step_t s;
    reset();
    trk[0].p[P_AMODE] = AM_OMNI;
    b->p[P_AMODE] = AM_FLW;
    assert(!ARP_RUNS(b));
    memset(&s, 0, sizeof s);
    s.n = 1;
    s.note[0] = 36;
    s.time = ST_NOTE;
    seq_step(b, &s, 1000, 0);                           /* no chord yet: as written */
    assert(sounding(b, 36));
    seq_release(b);
    omni_set(&trk[0], 2);                               /* G: down a fourth (the nearer way) */
    seq_step(b, &s, 1000, 0);
    assert(sounding(b, 31));
    omni_set(&trk[0], 0);                               /* F while it sounds: its release still ends it */
    seq_release(b);
    assert(gates(b) == 0);
    seq_step(b, &s, 1000, 0);                           /* F: up a fourth */
    assert(sounding(b, 41));
    seq_release(b);
    omni_set(&trk[0], 4);                               /* Am: down a minor third */
    seq_step(b, &s, 1000, 0);
    assert(sounding(b, 33));
    seq_release(b);
    trk[0].p[P_TRANS] = 2;                              /* TRN +2: the Am button is Bm, the bass B */
    omni_set(&trk[0], 4);
    seq_step(b, &s, 1000, 0);
    assert(sounding(b, 35));
    seq_release(b);
    trk[0].p[P_TRANS] = 0;
    trk[2].p[P_AMODE] = 0;                              /* a plain track does not follow */
    seq_step(&trk[2], &s, 1000, 0);
    assert(sounding(&trk[2], 36));
    seq_release(&trk[2]);
    puts("omni: FLW moves the pattern by the chord's root, releases what it played, others stay ok");
}

int main(void)
{
    host_tracks_init();
    keys_test();
    record_test();
    follow_test();
    return 0;
}
