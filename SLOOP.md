<p align="center"><img src="assets/logo/sloopdx-logo.png" alt="sloopDX" width="440"></p>

# sloopDX 2.4

**SLOOP as a pure DX7 FM synth, for the M-VAVE FM-1.** Four tracks — three synth parts and a drum machine with 16 sounds on the white keys — one engine: a six-operator DX7 voice (Dexed's msfa core, ported to integer C and within 1 LSB of Dexed against Dexed), 20 factory voices and your own DX7 banks (.syx, 32 voices), a low-pass behind each voice, four FM drum kits made with the same engine and one of your own (every sound with eight macros, a noise operator, step locks, the dice; drive and compression on the drum bus), ghost notes and ratchets, note repeat, one-key chords, 16 punch-in effects, a vinyl / sidechain / DJ-filter master, and a teenage-engineering-style screen that always shows what your hands can do next. No factory patterns, nothing to load: everything you hear, you play.

sloopDX is free and open source (GPL-3.0), a fork of [SLOOP](https://github.com/isod89/sloop-fm1) by isod89, which is based on [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hügelton Instruments. This manual is based on SLOOP's manual: the workflow is SLOOP's, the sound is the DX7's.

> **Status:** 2.4, a usable beta: it builds, the host tests pass, and it is installed and played on a real FM-1; the web editor against the device and the CPU with every voice sounding are still being checked. Install at your own risk.

### From SLOOP 2.3

sloopDX carries what SLOOP 2.3 added to the FM-1 itself (none of it tried on a device in sloopDX yet):

- **A MIDI keyboard on the MIDI IN jack.** The FM-1's 3.5 mm TRS MIDI input works: channels 1–3 play the synth tracks, 10 the drums, 4–16 the selected track. The input reads its buffer by content, so no note is left hanging (fix from Felucca [Salt], by ChanceTheMaker and keremimo). See [MIDI keyboards](#midi-keyboards).
- **MIDI clock in.** GLO → SYSTEM → **SYNC**: USB or TRS, a setting of the FM-1 that stays when you load a project. sloopDX follows the master's tempo, START, CONTINUE and STOP, pulse by pulse, so it never drifts (after Felucca 1.0, from contributions by ChanceTheMaker and keremimo). See [MIDI keyboards](#midi-keyboards).
- **USB audio: record the FM-1 on a computer.** On USB the FM-1 is also an audio input (*Felucca*, 44.1 kHz stereo, no driver): record its master output in your DAW or Audacity, MIDI and the editor still working on the same cable (after Felucca 1.0). HOME menu → **USB AUDIO**: the level follows the MASTER knob, or **FULL**, a fixed full level. See [USB audio](#usb-audio-record-on-a-computer).
- **Choose how REC records.** On the REC screen: KNOB 1 **mode** — *free* (the free take: the tempo follows you) or *tempo* (record at the tempo you set) — KNOB 2 the **length** (1, 2 or 4 bars), KNOB 3 the **start** — your first note, or a one-bar **count-in** after PLAY. See [Recording](#recording).
- **Lights for playing in the dark.** Hold HOME → **LIGHTS**: every button glows (LOW, MID, HIGH), so the labels are readable on a black FM-1; the active ones stay at full light. **KEYS**: the C keys, or every white key, glow too. **NOTES**: the notes playing light their keys, on every page and in every layer (by @renebohne). The glow is a short pulse on every scan, as in Felucca 1.0.1: no flicker, and the dim marks read as dim. See [Lights](#lights).
- **Backup and restore.** The editor saves everything on the FM-1 in one file — the music you are working on, the projects, the user presets, the settings and, on sloopDX, your DX7 bank — and puts it all back. See [The web editor](#the-web-editor).
- **Steadier.** The knobs answer every click: no more dead moments, double clicks or jumps. A note-off sent from a computer is never dropped any more when a lot of MIDI arrives at once (a hanging note), and a malformed MIDI message is ignored. When the processor is overloaded, sloopDX fades out one voice at a time, after two late halves in a row and never the bass or the lead, instead of cutting the oldest note. Keys play about a millisecond sooner. A key let go just after a change of VOICE (POLY, MONO…) no longer leaves its note stuck. The settings, projects and presets are checked more strictly when they are read back from flash. (All after Felucca 1.0.)

### What changed from SLOOP

- **One engine: DX7.** Six operators, 32 algorithms, operator envelopes with rate and level scaling, pitch envelope, LFO with pitch and amp modulation, feedback. The same output as Dexed for the same patch. See [The DX7 engine](#the-dx7-engine).
- **20 factory voices** in the DX7 tradition — designed here, not copied, among them three modern basses (DEEP SUB, 808 SUB, REESE) — and **your own banks**: a standard 32-voice bulk dump (.syx) loads as U01–U32. See [Your own DX7 bank](#your-own-dx7-bank-syx).
- **Macro knobs on every voice:** BRITE, ATK, DEC, REL, FDBK — the knobs a DX7 never had.
- **FM drums.** The drum track plays 16 FM drums built with the same engine, in four kits: DX KIT, 808 FM, ELECTRO and METAL. See [Drum kits](#drum-kits).
- **Gone:** the sample engines, the sample sets, the user sample slots, the other eight engines and the 37 sample and drum-synth kits. Everything else of SLOOP — layers, free takes, swing, song mode, DUST, DUCK, undo, projects, user presets — is as it was.

---

## Contents

1. [Install](#install)
2. [Sixty seconds to a beat](#sixty-seconds-to-a-beat)
3. [The colour code](#the-colour-code)
4. [The panel: tap, hold, layers](#the-panel-tap-hold-layers)
5. [The drum track](#the-drum-track)
6. [Recording](#recording)
7. [Layers in detail](#layers-in-detail)
8. [Undo, clear, save, autosave](#undo-clear-save-autosave)
9. [Master: DUST, DUCK, FILT](#master-dust-duck-filt)
10. [Punch-in effects](#punch-in-effects)
11. [Screens](#screens)
12. [The DX7 engine](#the-dx7-engine)
13. [The factory voices](#the-factory-voices)
14. [Your own DX7 bank (.syx)](#your-own-dx7-bank-syx)
15. [Drum kits](#drum-kits)
16. [Song mode](#song-mode)
17. [The web editor](#the-web-editor)
18. [Sound design pages](#sound-design-pages)
19. [MIDI keyboards](#midi-keyboards)
20. [USB audio: record on a computer](#usb-audio-record-on-a-computer)
21. [Lights](#lights)
22. [Specifications](#specifications)
23. [Rescue, going back, credits](#rescue-going-back-credits)

---

## Install

1. Double-click **`INSTALL-SLOOPDX.bat`** in the sloopDX folder. It builds the firmware and opens the installer at `http://localhost:8766/webapp/installer/`.
2. In **Chrome or Edge**, connect the FM-1 to the computer by USB (a data cable, directly — no hub).
3. Press **INSTALL**, allow MIDI access, and wait for *Done*. Keep the black window open until then.

The FM-1 restarts on the sloopDX logo. The editor is at `http://localhost:8766/webapp/editor/` (or **`OPEN-EDITOR.bat`**). On Linux and macOS, see [BUILDING.md](BUILDING.md) (`./build.sh`, then `python3 tools/fm1_install.py build/felucca.fwsc`).

## Sixty seconds to a beat

1. **ALGORITHM** to track **4** (beige, drums). The white keys play 16 sounds: **F3 kick**, G3 kick 2, A3 snare, B3 clap, **C4 hat**, D4 open hat… **PRESETS** picks a kit: try *808 FM* or *METAL*.
2. Press **REC**: *rec ready*. **Play a beat freely, at your own tempo** — no click, no count-in. Hold **OCT−** while you hit for ghost notes, **OCT+** for hard ones.
3. **Press REC on the "1" after your last bar.** The loop closes: its length sets the tempo, the hits snap to the grid, the loop plays at once.
4. **REC** again while it plays: you record on top (overdub). Hold **ARP** and hold the hat key: a 1/16 hat roll, recorded as ratchets.
5. Turn **ALGORITHM** to track **1** (cyan, *FM BASS*), **REC**, play a bass line. Hold **SCL** and press the key of your song (e.g. D); on track 2 (*EPIANO 1*) hold SCL and turn **KNOB 1** to *7TH*: every white key is now a chord of the key.
6. Hold **FX** and press a white key for a punch-in effect; still holding FX, turn **KNOB 2** for DUST, **KNOB 3** for DUCK.
7. Made a mistake? Hold **EDIT** and press **OCT−**: undo.

## The colour code

| Colour | Track | Knob |
| --- | --- | --- |
| **cyan** | 1 · synth | KNOB 1 |
| **light blue** | 2 · synth | KNOB 2 |
| **pink** | 3 · synth | KNOB 3 |
| **beige** | 4 · drums | KNOB 4 |

The four dials at the bottom of the screen show what KNOB 1–4 do now. White always means *what you are touching*. Red always means *recording*.

## The panel: tap, hold, layers

Every function button has two lives. **Tap** it (press and let go, touching nothing else): its pages open, as on any FM-1 firmware. **Hold** it: a **layer** — the 16 white keys and KNOB 1–4 change job while it is held, and after 0.14 s the screen shows the 16 keys as tiles and the knobs as dials. Let go: back to playing.

The tiles are four rows of four, keys 1–4, 5–8, 9–12, 13–16. To find them without looking at the screen, the first key of each row (1, 5, 9, 13) glows dimly while a layer is held, and on the drum track; the keys at full light are what is on.

**Lock a layer:** hold its button and tap **HOME** — the layer stays open when you let the button go, both hands free for the keys and the knobs (*LOCK* on the screen, the button blinks). Any other button lets it go (HOME, the layer's own button, ENV…) and does only that; PLAY, REC and OCT− / OCT+ keep working inside it.

| Hold | Keys | KNOB 1 · 2 · 3 · 4 | Tap |
| --- | --- | --- | --- |
| **FX** — *punch* | a punch-in effect while the key is held | FILTER · DUST · DUCK · — | FX pages |
| **EDIT** — *erase* | erase that sound / note from the pattern | SHIFT · LENGTH ×2 / ½ · TRANSPOSE · — | EDIT pages (drums: grid / kit) |
| **ARP** — *roll* | note repeat on the grid | RATE · — · — · — | ARP pages |
| **SEQ** — *steps* | steps 1–16 of the page | SOUND / NOTE · DIV · SWING · LENGTH | SEQ pages (drums: grid / kit) |
| **SCL** — *key* | the key of the song | CHORD · SCALE · KEYS · TRANSPOSE | SCL pages |
| **GLO** — *mix* | 1–4 mute · 5–8 solo · 16 tap tempo | level of tracks 1 · 2 · 3 · 4 | GLO pages |
| **SAVE** — *song* | 1–4 play section A–D (next bar) · 5–8 save the loop into A–D · 13 loop / song · 14 SONG REC · 16 the chain | — | TRACKS: the SONG screen · else the SAVE pages |

Other controls:

| Control | Action |
| --- | --- |
| **PLAY** | start / stop all four tracks (works inside any layer) |
| **REC** | playing: record now / stop · stopped: arm (the first note starts) · free take: close the loop |
| hold **REC** | clear the selected track (a ring fills: keep holding ~2 s; let go before and nothing happens) |
| **SAVE** | on TRACKS: the SONG screen · elsewhere: the SAVE pages |
| **EDIT + OCT− / OCT+** | undo / redo |
| **ALGORITHM** | select the track (on every page) |
| **PRESETS** | the selected track's voice, or the drum kit |
| **SELECT** | tempo (always, even inside a layer) |
| **OCT− / OCT+** | synth tracks: octave (both: back to 0) · drum track, held: ghost / hard hits |
| **HOME** | the TRACKS screen · hold: menu (colour, low cut, zoom, lights, keys, notes, USB audio, calibration, about) · tapped while a layer is held: lock it open |
| **ENV / LFO** | their pages |

## The drum track

Track 4 plays **16 sounds, one per white key** from the lowest F to the highest G; a black key plays the sound of the white key on its left (two fingers on one sound, for fast rolls). Every sound is a DX7 voice of its own, rendered by the same engine as the synth tracks.

| Key | Sound | Key | Sound | Key | Sound | Key | Sound |
| --- | --- | --- | --- | --- | --- | --- | --- |
| F3 | kick | C4 | hat | G4 | snare 2 | D5 | ride |
| G3 | kick 2 | D4 | open hat | A4 | low tom | E5 | shaker |
| A3 | snare | E4 | pedal hat | B4 | hi tom | F5 | conga |
| B3 | clap | F4 | rim | C5 | crash | G5 | cowbell |

**Levels:** every hit has one of four levels — **GHOST**, **SOFT**, **NORM** (as played), **HARD**. Hold **OCT−** while you hit for ghost notes, **OCT+** for hard hits; they are recorded so. **Ratchets:** a hit can repeat x1–x4 inside its step (ARP rolls record them; SEQ + a step + KNOB 3 sets them). The closed and pedal hats choke the open one. The kicks and toms have a pitch sweep at the hit, the clap is four hits in a row.

## Recording

sloopDX records live and layers every pass on top of the last (overdub). Notes go to the nearest step **as you heard it**: the time between a key and its sound (~12 ms) is taken back, so what you play on the beat lands on the beat. Chords are kept on the synth tracks (up to 4 notes a step); held notes become ties.

| When | REC does | Then |
| --- | --- | --- |
| **Playing** | records the selected track **at once** | REC again stops recording, the loop plays on |
| **Stopped, project with notes** | arms (*rec ready*, the REC light blinks) | **your first note starts the loop and is step 1**; PLAY starts it too |
| **Stopped, empty project** | arms (*rec ready* · *play freely*) | a **free take** (see below), or MODE *tempo*: as with notes |

**The REC screen sets how it records** (armed, before the first note), with the knobs:

| Knob | Dial | Choices |
| --- | --- | --- |
| KNOB 1 | **mode** (empty project only) | **free**: a free take, the tempo follows you · **tempo**: record at the tempo set (SELECT) |
| KNOB 2 | **length** | the loop of the selected track: **1, 2 or 4 bars** |
| KNOB 3 | **start** | **note**: your first note starts the loop · **count**: press **PLAY**, one bar of clicks (4, 3, 2, 1 on the screen), then the loop starts recording; notes played before only sound |

MODE and START are settings of the FM-1: they stay as you left them. In a project with notes there is no MODE: it always records at the tempo set (a free take would change the tempo of what is already there). During the count-in, REC cancels it and PLAY goes back to *rec ready*.

**Free take — the loop follows you.** On an empty project there is no tempo yet, so you set it by playing:

1. REC, then play freely. The screen shows *free take*, the seconds, and the loop it would make right now (*2 bars · 92 bpm*).
2. **Press REC on the "1" after your last bar.** The time from your first note to that press is the loop: sloopDX picks 1, 2 or 4 bars at the tempo nearest the one set (within 3 % the set tempo is kept), writes your notes into it with their lengths and levels, and plays it at once. All four tracks take that length.
3. **PLAY** during a free take drops it. A take closes by itself after 24 s.

- While recording the REC light is solid and the track shows a red *rec*. Turn ALGORITHM and the take moves to the next track without stopping.
- The **PLAY light flashes on every beat**: a visual metronome. An audible click: GLO → GLOBAL → **CLICK** (`OFF`, `REC`, `ON`); it is never recorded.
- **Swing** is MPC swing: 50 % straight to 75 % (GLO → GLOBAL → SWING for all tracks, SEQ + KNOB 3 per track). The swing of a track adds to the global one.

## Layers in detail

### FX — punch

The 16 white keys are the [punch-in effects](#punch-in-effects); they run while the key is held. The knobs drive the [master](#master-dust-duck-filt): **KNOB 1 FILTER** (turn left: low-pass, right: high-pass, centre: off), **KNOB 2 DUST**, **KNOB 3 DUCK**. Keys pressed while FX is held never play or record notes.

### EDIT — erase

Hold EDIT and press a key: that sound (drums) or that note (synths; with CHORD on, the notes of its chord) leaves the selected track's pattern — **while playing**, from every step the playhead passes while you hold the key (MPC style: hold the hat key for one bar and the hats of that bar are gone); **stopped**, from the whole pattern at once. *ERASED* flashes. The knobs reshape the whole pattern:

- **KNOB 1 SHIFT** — every step one later / earlier (turns the groove around).
- **KNOB 2 LENGTH** — right: ×2 (the pattern copied after itself, up to 128 steps, as far as the pool allows); left: ½.
- **KNOB 3 TRANSPOSE** — every note a semitone up / down (synth tracks).
- **OCT− undo · OCT+ redo** (the knob turns of one hold count as one change).

### ARP — roll (note repeat)

Hold ARP and hold a key: it repeats on the grid at the **RATE** of KNOB 1 — 1/8, 1/16, 1/32, 32T, 1/64 — locked to the tempo and the swing, so it always lands in time. On the drum track OCT− / OCT+ make it ghost / hard. While recording, a roll is written as ratchets (a 1/32 roll on a 1/16 track: x2 on each step). Rolls end with their key.

### SEQ — steps (step sequencer)

The 16 white keys are the 16 steps of the page; the lit ones play. **OCT− / OCT+** page through the track (up to 8 pages: steps 1–16 … 113–128, as far as its LENGTH goes); the first eight black keys (F#3 … A#4) pick page 1–8 directly.

- **An empty step:** press its key — it is set at once. Drums: with the sound shown (KNOB 1 picks it, or the last pad you hit); synths: with the note or chord you played last.
- **A set step:** press and let go — it is cleared. Hold it and turn a knob instead — it is edited, and kept: **KNOB 1** sound (drums) / note (synths), **KNOB 2 LEVEL** (ghost, soft, norm, hard), **KNOB 3 RATCHET** (x1–x4). Hold several step keys to edit them together.
- **Drum locks (2.1):** with a drum step held, **KNOB 4** locks the sound's **TUNE** (−16…+15 semitones) and **PRESETS** its **DECAY** for that hit only; the fourth dial shows *tune / decay*, the step a ★. One sound per step can be locked; clearing the hit clears its lock.
- **No step held:** KNOB 1 the sound / note to set · KNOB 2 **DIV** (1/4 … 1/32, triplets) · KNOB 3 **SWING** of the track · KNOB 4 **LENGTH** (1–128 steps; each track loops on its own length, polymeters stay in phase). The four tracks share **256 steps**: a track can have up to 128, and the others what is left (turn further and the screen says *STEPS FULL*). Only the steps inside a track's LENGTH are saved with the project.

### SCL — key and chords

- **Any key** sets the **key of the song**: the root of all three synth tracks (*KEY D*).
- **KNOB 1 CHORD** (selected synth track): OFF, TRIAD, 7TH, 9TH (1-3-7-9, the lo-fi / R&B voicing), SUS4, POWER. With a chord on, **the white keys walk the scale from C4** — C4 is the chord of the key's I, D4 the II, E4 the III… — and one finger plays the whole chord, recorded as a chord. With SCALE on CHR, the chords come from the minor scale.
- **KNOB 2 SCALE** for all synth tracks (16 scales: major, minor, dorian, mixolydian, pentatonics, harmonic, blues…).
- **KNOB 3 KEYS**: OFF (all keys chromatic), SNAP (every key rounded to the scale), WHITE (the white keys walk the scale, the black keys are silent).
- **KNOB 4 TRANSPOSE** the selected track, ±24 semitones.

Changing a voice (PRESETS, a user preset) never changes the key, the chord mode, the pattern or the mix of its track.

### GLO — mix

- White keys **1–4 mute** tracks 1–4 (a muted track fades out in a few ms and plays no new notes; its pattern runs on in time), keys **5–8 solo** them (several solos add up). The tiles show what is heard.
- The last white key (**G5**): **tap tempo** (two taps or more).
- **KNOB 1–4: the levels** of tracks 1–4.

## Undo, clear, save, autosave

- **Undo / redo:** hold EDIT, press OCT− / OCT+. One level: the last recording pass, erase, clear, step or pattern edit; redo takes it back again.
- **Clear a track:** hold REC. After 0.7 s the press is cancelled and a ring fills; keep holding ~1.3 s more and the selected track is cleared (*TRACK 2 CLEARED*). Let go before: nothing. Undo brings it back.
- **Save:** SAVE + keys 5–8 save the loop into section / project A–D (= SLOT 1–4); SAVE → PROJECT has SLOT, LOAD, SAVE too.
- **Autosave:** when the transport is stopped and you have not touched anything for 2.5 s (at most every 20 s), the working project is kept in flash; at power-on sloopDX comes back exactly as you left it.
- **New project:** SAVE → TOOLS → NEW (turn to GO): the four tracks back to their power-on sounds, empty patterns (undoable).

## Master: DUST, DUCK, FILT

On the whole mix, after the tracks' sends (FX + KNOB 1–3, or GLO → MASTER):

- **DUST** 0–100 %: the mix through an old sampler and a record — drive into a soft clip, a lower sample rate (down to ~11 kHz), fewer bits (down to 8), a low-pass closing to ~3 kHz, and while the transport plays a little hiss and crackle (a stopped sloopDX is silent).
- **DUCK** 0–100 %: every kick pumps the synth tracks down and back over an 1/8 note — the sidechain sound, in time at any tempo.
- **FILT**: a DJ filter. Left of centre a low-pass closing, right a high-pass opening, centre OFF. It glides (no zipper noise).
- **ROLL** (GLO → MASTER): the note-repeat rate of ARP + key.

## Punch-in effects

Hold **FX**, then hold a white key — the 16 white keys from the lowest F to the highest G. The effect runs on the whole mix while the key is held and lets go cleanly when you release it. Loops and the gate are locked to the tempo and start on the grid.

| Key | Effect | Key | Effect |
| --- | --- | --- | --- |
| 1 | loop 1/4 | 9 | low-pass sweep |
| 2 | loop 1/8 | 10 | high-pass sweep |
| 3 | loop 1/16 | 11 | phone |
| 4 | loop 1/32 | 12 | bit crush |
| 5 | stutter (1/16 triplets) | 13 | alias (sample-rate drop) |
| 6 | reverse | 14 | gate 1/16 |
| 7 | tape stop | 15 | echo (dotted 1/8) |
| 8 | half speed | 16 | tape wobble |

## Screens

- **TRACKS** (HOME) — the performance view: tempo, swing, transport, bar.beat; each track with its voice, its steps, the playhead, mute / solo / rec badges and its level. Dials: *swing · level · steps · pan* (KNOB 2 on a muted track unmutes it).
- **Layers** — while a layer button is held: 16 tiles (the white keys) and the knobs' dials, in the layer's colour.
- **DRUMS** (EDIT or SEQ tapped on TRACKS with the drum track) — **grid**: the 16 sounds × 16 steps, levels as shades, ratchets as notches; dials *sound · step · hit · level*. **kit**: 16 pads that flash on every hit; dials *kit · level · drive · comp* (DRIVE and COMP on the whole drum bus; reverb and pan are on HOME). **lane** (EDIT on kit): the macros of the sound last played, its page in the corner (1/3 … 3/3; see [Drum kits](#drum-kits)). SEQ tapped switches grid ↔ kit, EDIT kit → lane → kit, SAVE twice stores MY KIT.
- **EDIT** (tapped on a synth track) — the DX7 pages **VOICE** and **SHAPE**: the voice and its macro knobs. See [The DX7 engine](#the-dx7-engine).
- **REC READY / FREE TAKE** — while REC is armed: the tracks, then **mode**, **length** and **start** on KNOB 1–3 (4-3-2-1 during a count-in); during a free take: the seconds and the loop it makes.
- **Holds** — the ring of REC (clear) while held.
- **SONG** — the section chain.
- **Sound pages** (ENV, LFO, FX, SCL, EDIT, ARP, SEQ, GLO, SAVE) — the full synth, colour-coded.

## The DX7 engine

Every synth track is a six-operator FM synthesizer: the DX7 voice of Dexed's msfa core, ported to integer arithmetic for the FM-1's chip and checked against Dexed (99 % of samples identical, the rest within 1-2 LSB), so a patch sounds here as it does there. 32 algorithms, six operators each with a rate / level envelope, keyboard rate and level scaling, velocity sensitivity and amplitude modulation, a pitch envelope, an LFO with pitch and amp modulation and key sync, operator feedback, transpose. The three parts share **8 voices**; each part plays the whole keyboard (no split).

### Voice edit (DX7)

Tap **EDIT** on a synth track: the voice opens as a **list**, one row per setting, the way the Baud Girl FM-1+VA firmware edits FM (its idea, rebuilt here). The name is on the left, the value on the right, a thin bar shows where the value sits in its range.

| Control | On the voice list |
| --- | --- |
| **SELECT** | moves the highlight (on this page only; tempo again everywhere else) |
| **ALGORITHM** | changes the highlighted value (on this page only; the track again everywhere else) |
| **EDIT** (tap) | opens a group (OP1–OP6, Pitch Env, LFO, Name) or runs an action (tap twice for Copy To, Init Voice, Store) |
| **HOME** | back to the top of the list; on the top, back to TRACKS |
| **SAVE** | stores your bank in the FM-1's memory |
| **PRESETS** | jumps straight to the next or previous operator (OP1 … OP6), on the same row |
| **KNOB 1–4** | **CUT**, **RESO** (the low-pass), **REL**, **FDBK**; the value in the message bar while you turn. Two small dials top right show CUT and RESO all the time: lit when set (CUT below 127, RESO above 0), grey when the filter is out of the way |
| keys, **OCT− / OCT+**, **PLAY** | play as always, so you hear every change |

The top of the list:

| Row | What it is |
| --- | --- |
| **Voice** | the voice the track plays: 01–20 the factory voices, 21–52 U01–U32 of the bank in use |
| **Bank** | which of the 8 banks U01–U32 come from (1–8); the edits of the bank you leave are stored first |
| **Algorithm** | 1–32. The first turn only draws the algorithm: six boxes, the carriers in your track colour along the bottom, each modulator above the operator it feeds, feedback as a loop. Turn again to change it |
| **Feedback** | 0–7 |
| **Osc Sync** | every operator starts its wave together on each note |
| **Transpose** | ±24 semitones |
| **OP1 › … OP6 ›** | one operator: **On** (switch it off to hear the others; not stored, every operator is on again with another voice), Output Level, Coarse (shown as the ratio, or the fixed range), Fine, Detune ±7, Osc Mode (ratio / fixed), Rate 1–4, Level 1–4, Key Velocity, Amp Mod Sens, Break Point, Left / Right Depth, Left / Right Curve, Rate Scaling |
| **Pitch Env ›** | Rate 1–4, Level 1–4 (0 = no shift) |
| **LFO ›** | Wave, Speed, Delay, Pitch Mod Depth, Amp Mod Depth, Pitch Mod Sens, Key Sync |
| **Name ›** | ten characters, one row each |
| **Copy To** | copies the voice into the slot you pick with ALGORITHM, and plays it from there |
| **Init Voice** | a plain sine voice in the slot |
| **Store** | as SAVE |
| **More Pages ›** | the quick knobs (VOICE, SHAPE) and the track's voice mode |

**Where the edit goes.** As on a DX7, you edit a voice in one of your 32 slots. Change anything on a factory voice and sloopDX first copies it into the first slot called INIT VOICE (with no bank loaded: U01), switches the track to that slot and says *COPIED TO U01*. With no free slot it says so: pick one with **Copy To**. The next note plays every change; notes already sounding keep theirs. A dot after the name at the bottom means the bank has changes that are not stored: **SAVE** keeps them, power-off forgets them.

**In the web editor** the **Voice** tab shows the same voice on one page: the algorithm drawn, every operator as a column of sliders with its envelope, the pitch envelope and the LFO. Every slider plays on the FM-1 at once. Pick the slot, copy in a factory voice, import or export a single voice (.syx, the DX7's one-voice format) or one voice of a bank, give it a name, **Store bank**. The FM-1's list and the editor work on the same slots.

### The quick knobs

**More Pages** (or tapping EDIT on the pages after it) opens two pages of macro knobs:

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| **VOICE** | **VOICE** — the patch: the 20 factory voices, then U01–U32 of your bank | **BRITE** −40…+40 | **ATK** −40…+40 | **DEC** −40…+40 |
| **SHAPE** | **REL** −40…+40 | **FDBK** −7…+7 | **CUT** 30 Hz…16 kHz (127 = open) | **RESO** 0…100 % |

The macro knobs are the panel a DX7 never had. They move the patch's own values, so 0 is always *the voice as programmed*, and they work the same on every voice, factory or yours:

- **CUT** and **RESO** — a low-pass after the voice (also on KNOB 1 and 2 in the voice list, with the value shown at the bottom while you turn). At CUT 127 it is out of the way and the voice is bit for bit the DX7 patch; ENV / LFO → FLT move it.
- **BRITE** — the output levels of the modulators (the operators that are not carriers): left dull, right bright. On an electric piano it is the tine; on a bass, the growl.
- **ATK** — the first rate of the carrier envelopes: right faster, left a slower swell.
- **DEC** — the second and third rates of every operator: right shorter and more percussive, left longer.
- **REL** — the fourth (release) rate of the carriers: right shorter, left a longer tail.
- **FDBK** — the feedback of the algorithm's feedback operator, added to the patch's own (0–7): more is harsher, from a sine to a saw.

They act at once, also on notes that are already sounding (as Dexed does): turn BRITE on a held chord and you hear it open up.

**ENV** on a synth track shapes the carriers' envelopes on top of the voice: **ATK** slower attack, **DEC** longer decay, **REL** longer release (0 = as the voice is programmed, up = slower / longer), **SUS** lowers the sustain (127 = as programmed). On **ENV DEST** and **LFO DEST**, **FLT** moves the cutoff of the low-pass behind the voice (**CUT** and **RESO**, KNOB 1 and 2 in the voice list; CUT 127 = open, the voice as programmed) and **SHP** the modulators' level (brightness); **PIT** bends the pitch, **AMP** (LFO) the volume. **PRESETS** on a synth track goes through the 20 factory voices with their sends (chorus, delay, reverb) set to suit them; your own settings save as user presets (32), as in SLOOP. At power-on, on a new project: **90 BPM**, *FM BASS* on track 1, *EPIANO 1* on track 2, *STRINGS* on track 3 and the DX KIT on track 4.

## The factory voices

17 voices in the DX7 tradition, designed for sloopDX (no Yamaha data is in the firmware):

| Voice | | Voice | |
| --- | --- | --- | --- |
| EPIANO 1 | the tine electric piano | ORGAN | organ (all six operators as carriers) |
| EPIANO 2 | softer, more bell in it | CLAV | clav |
| FM BASS | the FM bass | PLUCK | short pluck |
| SLAP BASS | slap bass | FLUTE | flute |
| SUB BASS | sine sub | SAW LEAD | lead |
| BRASS | brass | KOTO | plucked string |
| STRINGS | strings | INIT VOICE | the DX7's blank patch: one sine |
| GLASS PAD | glassy pad | | |
| BELLS | bells | | |
| MARIMBA | marimba | | |

## Your own DX7 banks (.syx)

sloopDX loads standard **DX7 32-voice bulk dumps** — `.syx` files of 4104 bytes, the format every DX7 editor, librarian and the DX7 itself write. It keeps **8 banks** in flash, 256 voices; one of them is *in use*, like the cartridge in a DX7. Its 32 voices are **U01–U32**, numbered **21–52** after the 20 factory voices and named as in the bank. The header, length and checksum are checked and every parameter is clamped to its DX7 range, so a strange file cannot crash the engine. The banks and the one in use stay after power-off.

- **Play them:** turn **PRESETS** on HOME past 17: 18 = U01 … 49 = U32, then your user presets. Or EDIT → **Voice**.
- **Switch banks:** EDIT → **Bank** (1–8) with ALGORITHM. Unstored edits of the bank you leave are stored first.
- **Load a bank:** in the **web editor**, tab **Library**, pick *Bank in use* 1–8 and drop the `.syx`; or `python3 tools/fm1_bank_upload.py bank.syx --bank 3` (FM-1 on USB; needs `pip3 install mido python-rtmidi`).
- **Where to find banks:** the original DX7 ROM cartridges at yamahablackboxes.com, hundreds of banks at bobbyblues.recup.ch (*DX7 All The Web*), soundarchive.co. sloopDX ships none of them: you load what you own.

While a bank uploads, U01–U32 play INIT VOICE; when it is accepted they switch over. A bank that fails its checksum is refused and that slot stays empty. DX7II "dump all" files and single-voice dumps (163 bytes) are not banks; a single voice goes in through the editor's **Voice** tab (Import .syx).

## Drum kits

Five kits — **PRESETS** on the drum track, KNOB 1 on the kit page, or the editor. The four factory kits each have their own 16 FM voices (other algorithms, frequencies, envelopes and pitch sweeps), all made for sloopDX; the fifth is **MY KIT**, yours.

| # | Kit | Style | What it does |
| --- | --- | --- | --- |
| 1 | DX KIT | classic | the voices as designed: an 808-style kick, a punch kick, snare, a four-hit clap, closed / open / pedal hat, rim, tight snare, two toms, crash, ride, shaker, conga, cowbell |
| 2 | 808 FM | round | an analogue drum machine in FM: sine kicks and toms with long pitch drops, a tonal snare, metallic hats from six detuned partials, a two-tone cowbell |
| 3 | ELECTRO | punchy | short and clicky: kicks with a fast sweep and a click, a bright snare, tight hats, a snap, a zap on the conga key, a blip on the cowbell key |
| 4 | METAL | industrial | inharmonic: a clanging snare, anvil rim and snare, bell toms, a gong on the crash key, a bell ride, a chain shaker, a pipe, a bell |
| 5 | MY KIT | yours | a copy of DX KIT until you save a kit, load a kit .syx or roll the dice |

**Noise.** Drum voices can have operators that play **noise** instead of a sine (sample and hold of a 32-bit generator, a new value twice per period of the operator's frequency: low = rumble, high = hiss; the level and the envelope as a sine's). Snares, claps, hats, cymbals and shakers use it. It is a drums-only mode: no synth voice and no .syx voice ever turns it on.

**Edit a sound (the lane page).** On the kit page tap **EDIT**: the sound you last played by hand (a key, MIDI; also while the pattern plays, whose own hits never move it) and its macros; the top right says *lane* and the page (1/3). KNOB 1–4, three pages on **PRESETS**; the message bar shows the sound, the macro and its value while you turn (*SNARE DECAY +8*). Each macro is an offset on the kit (0 = as the kit), saved with the project and its song sections:

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| 1 | **TUNE** ±24 semitones (the note, and fixed-frequency operators with it) | **DECAY** ±40 (the falling rates of every operator; + longer) | **SWEEP** ±40 (the pitch drop: deeper and longer, or less) | **BRIGHT** ±40 (the modulators' level) |
| 2 | **NOISE** ±40 (the noise operators' level; −40 off) | **LEVEL** −20…+10 dB | **PAN** L64…R63 | **CHOKE** kit / off / A / B / C |
| 3 | **REV** 0…127 (the sound's reverb send; 64 = the drum REV as it is) | | **dice kit** (turn right twice: a new MY KIT) | **dice sound** (turn right twice: only this sound new, the rest of the kit kept) |

**MY KIT.** **SAVE twice** on the kit (or lane) page bakes the kit you are on, with every sound's macros, into MY KIT: the macros are now in its voices and table, back to 0, and the drum track plays MY KIT, kept in flash. The **dice** (lane page 3, KNOB 3, turned right once to arm and once more to roll) makes a new MY KIT from rules per sound (kicks and toms: a body, a modulator, maybe a click; snares: body, tone and noise; the three hats share their metal and choke each other; …), the seed in the top right (#4711). KNOB 4 there rolls only the sound you edit; a factory kit becomes MY KIT first (with your macros in it), the other sounds stay as they were. The same seed gives the same kit: the web editor (Library → MY KIT) rolls a seed you type, loads a kit **.syx** and downloads MY KIT as one (a DX7 32-voice bank: voices 1–16 the sounds, 17–32 their table, so any DX7 librarian keeps it). A dice kit stays in RAM until you SAVE it.

**The drum bus.** Each sound at its level and pan → the sum → **DRIVE** → **COMP** (the kit page's KNOB 3 / 4) → each sound's reverb send. The drum track has **6 voices** of its own, so a busy pattern never steals from the synths. The kit is saved with projects and song sections. MIDI notes in on the drum channel (10) play the nearest of the 16 sounds (GM drum map).

## Song mode

A song is up to 16 steps of 4 sections, **A–D** (each holds the four tracks: voices, patterns, kit). Make it live, by playing:

1. Make a loop (the verse). Hold **SAVE** and press the **5th white key** (*save A*). Change the loop (the chorus) and save it into **B** with the 6th key, a bridge into **C**, an end into **D**. Saving over a used section asks for the key again within 3 s.
2. **Play the sections live:** hold SAVE and press white key **1–4**. Playing, the section starts on the next bar, every track from its first step, always in time; stopped, it becomes the loop at once.
3. **Record the song as you play it:** SAVE + key **14** (*rec*): from the next bar, every section you play and how many bars it plays are written into the song. Press it again, or STOP, to end: *SONG PARTS 5*. It is saved by itself once you stop.
4. **Play it back:** SAVE + key **13** switches *loop* / *song*; in song mode **PLAY** plays the whole song and stops at the end (your loop is back afterwards).

The **SONG screen** (SAVE tapped on TRACKS, or SAVE + key 16) shows the chain and edits it by hand: **KNOB 1** the step, **KNOB 2** its section, **KNOB 3** its bars, **KNOB 4** the number of steps; **REC** stores the loop into the step's section; **SAVE** (tap) saves the chain; **OCT−** loop / song; **OCT+ twice** loads a section. The four sections are the four project slots.

## The web editor

Open it from the installer page, or with **`OPEN-EDITOR.bat`** (`http://localhost:8766/webapp/editor/`), in Chrome or Edge with the FM-1 on USB, and press **Connect**. It follows the device live (turn a knob on the FM-1, the editor moves).

- **Sound** — every parameter of the selected track: the voice and the macro knobs, envelope, LFO, arp, sends.
- **Sequencer** — the pattern settings and the steps. On the **drum track**: a grid of the 16 sounds × the steps, with the **kit**. Choose a **level** (GHOST, SOFT, NORM, HARD) and a **roll** (x1–x4), then click: a hit; click it again (same level and roll): cleared; Shift+click: one level louder.
- **Tracks** — the four channel strips (level, pan, mute; SOLO and REC shown as on the device).
- **Library** — the user presets, and your **DX7 bank**: open or drop a `.syx`, see U01–U32 by name, erase the bank.
- **Projects**, **Settings** (GLOBAL, **MASTER**: DUST, DUCK, FILT, ROLL; DRUMS).
- **Backup** (Projects tab): **Save a backup** writes everything on the FM-1 to one file (SLOOP-backup-DATE.json): the music you are working on, the projects 1–4 (the song sections A–D), the 32 user presets, the settings (colours, calibration, the song order, the lights, SYNC) and the DX7 user bank. **Restore from a file** puts it all back — what is on the FM-1 is replaced. A damaged file is refused before anything is written, every object is checked as a load checks it, and each one is written as a save writes it (a cut-off restore never leaves half an object). Stop the song (PLAY) before restoring.

The protocol is documented in [web/EDITOR_PROTOCOL.md](web/EDITOR_PROTOCOL.md) (v7: SLOOP 2.3's backup plus the bank upload).

## Sound design pages

Underneath, SLOOP's synth voice is still there around the DX7: envelope and LFO pages (the ENV page is a gate for the DX7 voice; the track's LFO adds pitch modulation on top of the voice's own), arpeggiator, scales and chords, glide and voice modes (POLY, MONO, LEGATO, UNISON), per-track drive and slicer, chorus / delay / reverb sends (a stereo chorus, a tempo delay, a stereo reverb built as a feedback delay network: dense, no metallic ring), 32 user presets, 4 projects.

## MIDI keyboards

sloopDX takes MIDI from two places at once:

- **The MIDI IN jack** (3.5 mm TRS, on the FM-1): a keyboard or a pad controller with a MIDI output, through a TRS-to-DIN MIDI adapter. If nothing plays, try the other type of adapter (type A / type B).
- **USB**, from a computer or a phone (a DAW, a MIDI routing app) or a USB MIDI host box. A USB keyboard plugged straight into the FM-1 cannot work: both are USB devices, and a USB link needs a host.

| MIDI channel | Plays |
| --- | --- |
| 1, 2, 3 | synth tracks 1, 2, 3 |
| 10 | the drum track (the nearest of its 16 sounds; GLO → DRUMS → CH changes the channel) |
| 4–16 | the selected track: set your keyboard to channel 4 and it follows ALGORITHM |

**MIDI clock in:** GLO → SYSTEM → **SYNC** = **USB** or **TRS** (INT: sloopDX's own tempo). START plays from the top, CONTINUE carries on where it stopped, STOP stops; the tempo (BPM) follows the master, and the steps follow its 24 pulses a beat, so sloopDX cannot drift away from it. When the clock stops for half a second, PLAY on the FM-1 plays at its own tempo again. SYNC is a setting of the FM-1: it stays when you load a project.

Bluetooth MIDI is not supported: sloopDX, like Felucca, never switches the radio on.

## USB audio: record on a computer

On USB the FM-1 is also an audio input, named **Felucca**: 44.1 kHz, 16-bit stereo, class compliant, so no driver is needed. In your DAW or in Audacity, choose that input and record: you get the master output, exactly what the headphones play (after DUST, DUCK and FILT; the click and the count-in too, if they are on).

**Its level: HOME menu → USB AUDIO.**

- **MASTER** (default): the recording follows the MASTER knob, as the headphones do. Keep MASTER well up while you record.
- **FULL**: a fixed level, as with MASTER all the way up, kept from clipping by the output limiter, whatever the knob. MASTER then only sets the headphones: the right choice for an audio interface or a computer input with no level control of its own.

USB AUDIO is a setting of the FM-1: it stays as you left it. MIDI, the web editor and the installer keep working on the same cable while the computer records.

- The first time, the computer sees the FM-1 as a slightly different device (MIDI + audio) and sets it up again; the MIDI port keeps its name.
- The audio input comes from Felucca 1.0 (Leo Kuroshita): the same code, adapted to SLOOP 2.3 and carried into sloopDX.

## Lights

Hold **HOME** for the menu: **LIGHTS**, **KEYS** and **NOTES** are together there (with **USB AUDIO**, the level of the USB audio input: see [USB audio](#usb-audio-record-on-a-computer)). PRESETS moves, **KNOB 1** sets, OCT+ steps round, OCT− closes. They are saved with the settings of the FM-1, not with a project: loading a project or NEW PROJECT does not change them.

- **LIGHTS** — OFF, LOW, MID, HIGH: every button glows at that level, so its label can be read in the dark (on a black FM-1 the labels are unreadable unlit). What is on — the page, PLAY, REC, an octave — stays at full light and still blinks as before.
- **KEYS** — OFF, C KEYS, WHITE KEYS: the Cs, or every white key, glow at the LIGHTS level too (KEYS turns LIGHTS on at LOW if it was off). Played keys and the layer landmarks keep their own light.
- **NOTES** — ON: on a synth track, the notes sounding light their keys, played live or by the sequencer (the drum track always does). By @renebohne. It works on every page and in every layer: where the keys play or erase notes (EDIT, ARP, SAVE, SCL) the notes are lit — in SCL the scale and on the drum track in EDIT the sounds of the pattern then glow dimly underneath; where the keys are tiles (FX effects, SEQ steps, GLO mute / solo) the notes glow dimly and the tiles keep their full light.

The glow is a short pulse on every scan of the panel (about 900 times a second): no flicker. LOW, MID and HIGH are 0.5, 1 and 2 µs a scan; the landmarks (keys 1, 5, 9, 13 while a layer is held) and the notes under the tiles glow at 4 µs, a lit LED is about 95 µs.

## Specifications

| | |
| --- | --- |
| Tracks | 3 synth parts (8 DX7 voices shared) + drums (16 sounds, 6 DX7 voices) |
| Engine | DX7: 6 operators, 32 algorithms, rate / level envelopes with scaling, pitch envelope, LFO, feedback; integer port of Dexed's msfa (99 % of samples identical to Dexed, the rest within 1-2 LSB) |
| Sounds | 20 factory voices + 32 from your .syx bank (U01–U32); macros BRITE, ATK, DEC, REL, FDBK; a low-pass CUT / RESO; 32 user presets |
| Sequencer | up to 128 steps per track from one pool of 256 for all four, own length and division each; chords with a level and ratchet per note; drums with a level and ratchet per sound; ties, slide; MPC swing 50–75 %; one sample-accurate clock for steps, arp, rolls, slicer and song (no drift) |
| Performance | layers (hold a button: keys and knobs change job): punch-in FX, erase, note repeat, step entry, key / chords, mute / solo / tap tempo |
| Drum kits | 4 FM kits (DX KIT, 808 FM, ELECTRO, METAL), 16 sounds each |
| Effects | 16 punch-in effects; master DUST, DUCK, DJ filter; per track drive, slicer, sends to a stereo chorus, a tempo delay and a stereo reverb; master limiter |
| Recording | live, quantised as heard (latency-compensated), overdub; records at once while playing; free take sets loop length and tempo, or the tempo set, from the first note or a one-bar count-in |
| Memory | undo / redo, 4 projects, 32 user presets, one DX7 bank, autosave of the working project, song of 4 sections × 16 steps × 1–64 bars; full backup / restore from the editor |
| Audio | 44.1 kHz, fixed-point DSP (no floating point on the chip); USB audio input (the master output, 16-bit stereo, class compliant) |
| MIDI | USB class-compliant in / out; TRS MIDI IN (3.5 mm jack); channels 1–3 the synths, 10 the drums, 4–16 the selected track; MIDI clock in (USB or TRS); DX7 bulk dumps via the editor protocol |
| Update | over USB from the browser (package SHA-256 and CRC checked) |

## Rescue, going back, credits

- **USB rescue:** hold **OCT−** alone while switching on (*USB RESCUE*), then install again.
- **Interrupted install:** the FM-1 stays in update mode; press Install again and it finishes. A damaged package is refused, and the FM-1 keeps waiting for a good one.
- **Back to the official firmware:** save a backup with the editor first. Then M-VAVE's own updater, M-UPGRADE, with the FM-1 V15 firmware from m-vave.com (close every other app that uses MIDI first); or, on the installer page, **Return to the official firmware (V15)**: select M-VAVE's unmodified FM-1.fwsc (only that exact file is accepted) and install it. To come back to sloopDX, install it again and restore your backup.
- **Credits:** sloopDX (the DX7 engine port, the FM drums, the fork) is by Sven Trogus. It is a fork of SLOOP by isod89, which is based on Felucca by Leo Kuroshita (@kurogedelic), Hügelton Instruments — sequencer, UI, effects, storage, editor and installer. From SLOOP 2.3: played-note key lights by @renebohne (pull request #11); the TRS MIDI input buffer fix from Felucca [Salt] by ChanceTheMaker, found by keremimo; knob reading, MIDI input, overload shedding, LED glow, key debounce, MIDI clock, the USB audio input and the return to the official firmware after Felucca 1.0 / 1.0.1. The DX7 core is the msfa engine of Dexed (Apache-2.0; © 2012 Google Inc., 2016–2025 Pascal Gauthier). Font: Terminus (SIL OFL 1.1). Interface ideas after teenage engineering's pocket operators and EP-133, Elektron's step entry and Akai's MPC (swing, note repeat, erase) — sloopDX is not affiliated with any of them.
- **Licence:** GPL-3.0, no warranty (see [LICENSING.md](LICENSING.md)). M-VAVE and FM-1 are trademarks of their owners; DX7 is a trademark of Yamaha. sloopDX is not affiliated with any of them, and contains no Yamaha or M-VAVE voice data.
