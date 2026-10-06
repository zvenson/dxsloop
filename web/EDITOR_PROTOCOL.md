# sloopDX editor protocol (SysEx over USB-MIDI)

The firmware side is `firmware/src/editor.c` (sloopDX is based on SLOOP, which is based on Felucca: the
frames keep its "FL" header). Commands 16-26 (user presets and live sync) form protocol v2; commands
27-30 (tracks) form protocol v3; commands 31-32 (any track's parameters) form protocol v4; command 33 and
the extra step, `INFO` and `TRACK` bytes form protocol v5 (SLOOP 2.0); commands 34-36 (backup) form protocol v6 (SLOOP 2.3); commands 37-41 (the DX7 user bank) form
protocol v7 (sloopDX); commands 42-45 (voice editing) form protocol v8 (sloopDX).

**v7 (sloopDX):** one engine, `DX7` (NENGINES = 1; the engine byte of the drum track is 1). Its `P_E0`
(VOICE) is an enum of 49 names: the 17 factory voices, then U01..U32, the user bank. A DX7 32-voice bulk
dump (.syx, 4104 bytes) is sent in pieces with `BANK_BEGIN` / `BANK_WRITE` / `BANK_END`; the device
checks the checksum, keeps the bank in flash and renames U01..U32 after the voices (re-read `DESC` of
`P_E0`). The sample-slot commands 11-15 stay in the numbering but there are no slots (`SMP_INFO` answers
0 slots, the others rc 7). `INFO` ends with 8 (7 before voice editing). SLOOP 2.3's backup (`BK_LIST` / `BK_GET` / `BK_PUT`,
v6) works as there, with object 8 = the DX7 user bank (4096 voice bytes, length 0 = none) in place of the
sample slots 32..34.

**v3 (four tracks):** the device has four tracks: 1..3 are synth parts, 4 is the drum track. One
of them is *selected* (the TRACKS page on the device, or `TRACK`). Every v1 / v2 command acts on the
selected track (its parameters, engine, preset, steps, the user presets it stores or loads); `TRACK`,
`TRACK_MIX`, `TRACK_DUMP` and `TRACK_STEP` reach any track. Command numbers 1-26 are unchanged.

**v4 (any track's parameters):** `TRACK_PARAM` gets or sets a parameter of any track without changing
the selection, and the `TRACK_CHANGED` push follows level, pan and mute of the tracks that are not
selected. The v1-v3 commands are byte for byte as before; v4 is asked for with bit 1 of `WATCH`.

**v5 (SLOOP 2.0):** the drum track has 16 lanes (one sound per white key) with a level and a ratchet per
hit; `DRUM_STEP` reads and writes them. Synth steps carry a level and a ratchet per note. `INFO` ends with
the protocol version (5) and `TRACK` with the solo mask. Every addition is a byte appended at the end of
a reply or a request, so v1-v4 editors keep working (they see the drum lanes as GM notes, below).

## Framing

A request is `F0 7D 46 4C <cmd> <args...> F7`:

- `7D` is the non-commercial SysEx ID.
- `46 4C` is "FL".

Every request gets exactly one reply, with the same header and the same `<cmd>`. Requests
the device does not understand get no reply. Every data byte is 7 bit. While the editor
watches (v2, `WATCH`), the device also sends push frames (cmds 23, 24, 26) at any time.

| Item | Encoding |
| --- | --- |
| value (v14) | 2 bytes, LSB first, holding value + 8192, so the range is -8192..8191. `[lo, hi]`: value = (lo \| hi << 7) − 8192 |
| string | ASCII bytes, ended by a 0 byte |
| scope | 0 = parameter of the selected track (`P_*`, 0..P_COUNT−1); 1 = global parameter (`G_*`, 0..G_COUNT−1) |
| track | 0..3: tracks 1..3 (synth parts), 3 = the drum track |
| engine byte | 0..NENGINES−1; NENGINES = the drum track (it has no engine and no presets) |

The engine parameters are `P_E0..P_E7`: P_COUNT−8 .. P_COUNT−1, and `INFO` gives `P_E0`.
Their meaning, range and names depend on the current engine, so re-read `DESC` for them
after an engine change.

## Commands

| cmd | Request args | Reply args |
| --- | --- | --- |
| 1 INFO | — | version string, NENGINES, P_COUNT, G_COUNT, NSTEP, P_E0, then NENGINES engine-name strings, then (v3) NTRK (4), then (v5) the protocol version (8 on sloopDX, 7 on sloopDX 1.0 before voice editing, 6 on SLOOP 2.3, 5 on SLOOP 2.0-2.2); older firmware ends after the names / NTRK |
| 2 GET | scope, id | scope, id, v14 |
| 3 SET | scope, id, v14 | scope, id, v14 (the value after clamping). Setting global `G_ENGSEL` (id from DESC label "ENG") changes the engine with its defaults |
| 4 DUMP | — | engine, preset, then P_COUNT × v14 (the selected track), then G_COUNT × v14 (globals) |
| 5 DESC | scope, id | scope, id, fmt, min v14, max v14, def v14, label string, unit string, then for an enum (fmt 8) one name string per value (at most 64; firmware before SLOOP sent at most 16) |
| 6 STEP_GET | index 0..NSTEP−1 | index, n (0..4 notes), note0..note3, time (0 NOTE, 1 TIE, 2 REST), flags (1 accent, 2 slide), vel, then (v5) lvl, hi, rat |
| 7 STEP_SET | index, n, note0..3, time, flags, vel [, lvl, hi, rat (v5)] | same as STEP_GET (after the write). Without the v5 bytes the step's levels and ratchets become 0 |
| 8 PRESET | engine, preset | engine, preset (applies the preset: sound, sends, arp; never the pattern, the mix or the key: `LEVEL PAN MUTE`, `LEN DIV SWG GATE`, `ROOT SCL QNT CHORD` stay) |
| 9 PROJECT | op (0 load, 1 save, 2 query), slot 0..3 | op, slot, used (1/0). Save writes flash: allow ~2 s |

The local Studio build may append status `1` to a PROJECT reply when playback
prevents a load/save. No operation occurred; stop playback and try again.
An absent status byte retains the original reply format.
| 10 NAMES | engine | engine, count, count preset-name strings, then the two edit-page titles |
| 11 SMP_BEGIN | slot | slot, rc 7 (sloopDX has no sample slots) |
| 12 SMP_WRITE | slot, offset (3 bytes), data | slot, offset, rc 7 |
| 13 SMP_END | slot, data | slot, rc 7 |
| 14 SMP_ERASE | slot | slot, rc 7 |
| 15 SMP_INFO | — | 0, 0 (no slots) |
| 16 UP_LIST | start, count (1..16) | start, count, total slots, then per slot: used (0/1), engine, name string ("" if unused) |
| 17 UP_GET | slot | slot, used, engine, name, P_COUNT × v14, 16 × (note, flags) |
| 18 UP_PUT | slot, engine, name, P_COUNT × v14, 16 × (note, flags) | slot, rc (0 ok, 1 args, 2 flash). Writes flash: allow 1 s |
| 19 UP_STORE | slot, name | slot, rc. Stores the current sound: engine, parameters, the first 16 sequencer steps as the pattern (TIE steps → flag 4) |
| 20 UP_LOAD | slot | slot, rc (0 ok, 1 empty/invalid). Applies it |
| 21 UP_ERASE | slot | slot, rc |
| 22 WATCH | on (0/1; v4: 3 = also `TRACK_CHANGED`) | on (0/1; v4 firmware: 3 when 3 was asked for). While on, the device pushes cmds 23, 24, 26 (and 32 with bit 1) |
| 23 CHANGED (push) | — | scope, id, v14 |
| 24 RELOAD (push) | — | engine, preset, then (v3) the selected track |
| 25 PING | — | 0 |
| 26 STEP_CHANGED (push) | — | index, then (v3) the selected track |

| cmd (v3) | Request args | Reply args |
| --- | --- | --- |
| 27 TRACK | — (query), or track (select it) | selected track, NTRK, then per track: engine byte, preset, level v14, mute (0/1), armed (0/1, live recording) |
| 28 TRACK_MIX | track (get), or track, level v14 (0..127), mute (set) | track, level v14, mute. The drum track's level is global `G_DRLVL` (GLO > DRUMS LEVEL); mute is the track's `P_MUTE` |
| 29 TRACK_DUMP | track | track, engine byte, preset, P_COUNT × v14 (that track's parameters; no globals) |
| 30 TRACK_STEP | track, index (get), or track, index, n, note0..3, time, flags, vel [, lvl, hi, rat] (set) | track, index, n, note0..3, time, flags, vel, then (v5) lvl, hi, rat |

| cmd (v4) | Request args | Reply args |
| --- | --- | --- |
| 31 TRACK_PARAM | track, id (get), or track, id, v14 (set); id = `P_*` (0..P_COUNT−1) | track, id, v14 (the value after clamping, as `SET`). The selection does not change; no push about the editor's own write |
| 32 TRACK_CHANGED (push) | — | track, id, v14: `P_LEVEL`, `P_PAN` or `P_MUTE` of a track that is not selected changed on the device (only while `WATCH` was sent with bit 1) |

| cmd (v5) | Request args | Reply args |
| --- | --- | --- |
| 33 DRUM_STEP | index (get), or index, on (3 bytes), lvl (5 bytes), rat (5 bytes) (set) | index, on (3 bytes), lvl (5 bytes), rat (5 bytes): the drum track's step, whichever track is selected |

| cmd (v7, sloopDX) | Request args | Reply args |
| --- | --- | --- |
| 37 BANK_BEGIN | — | rc (0 ok). Starts an upload: the user bank is empty from now on (U01..U32 play INIT VOICE, the USER kit the DX KIT) until `BANK_END` accepts it |
| 38 BANK_WRITE | offset (2 × 7 bit, LSB first, 0..4095), data (1..512 bytes: the voice bytes of the dump, bytes 6..4101 of the .syx, each 7 bit, as they are) | offset (2 bytes), rc: 0 ok, 1 arguments (no `BANK_BEGIN`, offset + length > 4096, no data) |
| 39 BANK_END | checksum (byte 4102 of the .syx) | rc: 0 ok (the bank plays and is in flash), 1 checksum (the bank stays empty), 2 flash (the bank plays until power-off) |
| 40 BANK_INFO | — | ok (0 = no bank, 1 = a bank), then 32 name strings (U01..U32; empty strings without a bank) |
| 41 BANK_ERASE | — | rc: 0 ok, 2 flash. The user bank is empty again |

| cmd (v8, sloopDX voice editing) | Request args | Reply args |
| --- | --- | --- |
| 42 VOICE_GET | index 0..48 (the VOICE enum: 0..16 factory, 17..48 = U01..U32) | index, 128 bytes: the voice in the DX7's packed bulk format (7-bit, as in a .syx). Without a bank a user slot reads as INIT VOICE |
| 43 VOICE_PUT | slot 0..31, 128 packed bytes | slot, rc (0 ok, 1 arguments). Replaces U(slot+1) in RAM: the next note on a part whose VOICE is that slot plays it; the name comes from bytes 118..127. Without a bank this makes one (every other slot INIT VOICE). Not in flash until BANK_SAVE |
| 44 VOICE_PARAM | slot 0..31, idx 0..154 as 2 × 7 bit LSB first (get), or slot, idx (2), value (set) | slot, idx (2), value (clamped to the DX7 range of idx). idx is the position in the unpacked 155-byte voice (VCED order): op block k = idx 21k..21k+20 for OP6 (k 0) .. OP1 (k 5): R1 R2 R3 R4 L1 L2 L3 L4 BP LD RD LC RC RS AMS VEL LVL MODE COARSE FINE DET; 126..133 pitch EG R1-4 L1-4; 134 ALG 0..31; 135 FB; 136 OSC SYNC; 137 LFO SPEED; 138 DELAY; 139 PMD; 140 AMD; 141 LFO SYNC; 142 LFO WAVE; 143 PMS; 144 TRANSPOSE; 145..154 the name. Without a bank the first set makes one |
| 45 BANK_SAVE | — | rc (0 ok, 1 no bank, 2 flash): STORE, the whole user bank to flash (~1 s) |

**pack7:** groups of up to 7 bytes, each preceded by one byte holding their top bits
(bit j = bit 7 of byte j).

**DX7 user bank** (v7): a DX7 32-voice bulk dump is `F0 43 0n 09 20 00`, 4096 voice bytes (32 × 128,
the DX7's packed voice format), a checksum byte and `F7`: 4104 bytes. The checksum is
`(128 - (sum of the 4096 voice bytes) mod 128) mod 128`. The uploader checks the header, the length and the
checksum itself, then sends `BANK_BEGIN`, eight `BANK_WRITE` of 512 bytes (offsets 0, 512, .. 3584) and
`BANK_END` with the checksum byte. One request at a time, as always; a `BANK_WRITE` answers in a few ms,
`BANK_END` writes flash (allow ~1 s). After rc 0 re-read `DESC` of `P_E0` (the voice names) and, if
wanted, `BANK_INFO`. The reference uploader is `tools/fm1_bank_upload.py`; the firmware keeps the bank in
`firmware/src/eng_dx7.c` (`dx_user`, two storage objects at 0xA0000..0xA3FFF) and loads it at boot.

`fmt` values (`firmware/src/core.h`):

| Value | Name | Value | Name | Value | Name |
| --- | --- | --- | --- | --- | --- |
| 0 | INT | 5 | CUTOFF | 10 | NOTE |
| 1 | PCT | 6 | DB | 11 | ONOFF |
| 2 | BIPCT | 7 | SEMI | 12 | OCT |
| 3 | TIME | 8 | ENUM | 13 | STEPS |
| 4 | LFOHZ | 9 | BPM | 14 | SWING |
| | | | | 15 | FILT |

v5 formats: **SWING** 0..100 shown as the MPC swing, 50 % (straight) + value / 4 (so 75 % at 100);
**FILT** −64..63: 0 OFF, below 0 a low-pass closing (LP 1..100 %), above 0 a high-pass (HP 1..100 %).
**PCT** is the share of the range: value × 100 / max (rounded).

The editor should show the value with the unit; formatting it exactly like the device does
is not required.

## v2: user presets

A user preset = engine (0..NENGINES−1), name (1..12 chars, ASCII 32..126; the device shows it upper
case), all P_COUNT instrument parameters (v14 each, the same order as `DUMP`), and a 16-step pattern:
16 × (note 0..127 (0 = rest), flags: 1 accent, 2 slide, 4 tie). Loading one applies the engine and
the parameters of the sound; the pattern stored with it is never loaded (changing a sound never changes
the sequence), and the mix, pattern and key parameters stay (as `PRESET`). The slots are numbered 0..31
(the device shows U01..U32).

- `UP_LIST`: count is cut at 16 and at the last slot (start ≥ 32: count 0, no entries).
- `UP_GET` of an empty slot has the same shape with used 0, engine 0, name "" and all values 0.
  Values come back in the current parameter order, inside their ranges.
- `UP_PUT`: rc 1 for a slot ≥ 32, an engine ≥ NENGINES, a name that is empty, longer than 12 or has
  bytes outside 32..126, or a frame that is too short. Values are clamped to their ranges for that
  engine. A note with flag 4 is stored as a tie (note 0); flags on a rest are dropped.
- `UP_STORE`: name "" stores with the automatic name the device uses (engine name + slot number,
  "ANALOG 07"). rc 1 for a bad slot or name.
- rc 2 = the flash write failed or there is no flash; the slot is still changed in RAM until power-off.
- Frames stay below 640 bytes (`UP_PUT` is 5 + 1 + 1 + 13 + 2 × P_COUNT + 32 + 1).

**On the device:** SAVE > USER page: KNOB 1 picks the slot, KNOB 2 LOAD, KNOB 3 ERASE, KNOB 4 SAVE
(one detent arms, a second one within ~1.5 s acts, as PROJECT LOAD / SAVE). SAVE uses the automatic
name. SELECT and the SAVE > PRESETS browser continue past the factory presets into the used user
presets.

**Flash** (`firmware/src/upreset.c`): two storage objects (`OBJ_UPRESET0/1`, A/B sector pairs at
0xDC000..0xDFFFF), 16 records of 192 bytes each, behind a bank header (magic "UPB1", record size,
slot count; a mismatch reads as an empty bank). A record keeps its layout version (mismatch: empty)
and the P_COUNT it was stored with; another count is mapped by count (last 8 values = P_E0..P_E7, the
first ones = P_LEVEL.. in order, missing ones = defaults). P_COUNT was 53 (P_E0 45) until the SLICER
parameters (SLCR, PAT, RATE, DEPTH: ids 45..48) went in just before P_E0: P_COUNT 57, P_E0 49; SLOOP 2.0
added CHORD (id 49): P_COUNT 58, P_E0 50 (and G_COUNT 32: DUST, DUCK, FILT, ROLL, NEW at 27..31). An
editor takes them from `INFO`; older records load with the SLICER off and CHORD off.

## v2: live sync

- `WATCH 1` starts the pushes. Watching ends by itself 3 s after the last request of any kind (send
  `PING` about every 1 s), on a USB reset, and when the host goes away; `WATCH 0` ends it at once.
- **CHANGED** (scope, id, v14): a parameter changed on the device (knob, menu, sequencer edit of a
  `P_*`), not by the editor's own `SET`. Coalesced: each (scope, id) at most every 20 ms, with the
  latest value.
- **RELOAD** (engine, preset): the engine, a preset, a user preset or a project was loaded; re-read
  `DESC` of the engine parameters, `DUMP` and the steps. It is also sent after loads the editor asked
  for (`SET` of G_ENGSEL, `PRESET`, `PROJECT` load, `UP_LOAD`).
- **STEP_CHANGED** (index): a sequencer step changed on the device (record, clear, step edit,
  pattern load); not after the editor's own `STEP_SET`.
- Push frames have the normal header. Accept them at any time, also while waiting for a reply:
  match replies by cmd (23, 24 and 26 are never replies). The device sends at most a few per
  ~5 ms pass, and only when its USB send queue has room, so a push never delays a reply.

## v3: tracks

- The drum track: `DUMP` / `RELOAD` / `TRACK` give the engine byte NENGINES. Its `P_*` values exist
  (the pattern parameters `LEN DIV SWG GATE`, `PAN`, `MUTE` and `P_E0`, the kit, are used; the rest is
  ignored). `PRESET`, `SET` of `G_ENGSEL` and `UP_LOAD` do nothing there (`UP_LOAD` and `UP_STORE` answer
  rc 1). `DESC` of `P_E0` is the enum `KIT`; `P_E1..P_E7` describe engine 0. Its steps hold the 16 drum
  lanes (v5, `DRUM_STEP`); the v1-v4 step commands see them as GM notes (up to 4 per step).
- Selecting a track with `TRACK` does not push `RELOAD` (the editor re-reads `DUMP`, the steps and the
  engine `DESC` itself); selecting one on the device does (`RELOAD` with the new track).
- Pushes are about the selected track only: `CHANGED` (scope 0) and `STEP_CHANGED` refer to it, and
  changes to other tracks (live recording from MIDI into another track, `TRACK_*` writes) push nothing.
- Level and mute are also `P_LEVEL` / `P_MUTE` of the selected track (`SET`); `TRACK_MIX` reaches the
  others. Presets and user presets change a part's sound but keep its mix (`P_LEVEL`, `P_PAN`,
  `P_MUTE`), its pattern parameters (`LEN DIV SWG GATE`) and its key (`ROOT SCL QNT`, `CHORD`).
- Projects (`PROJECT`) save and load all four tracks and the selection (SLOOP 2.0: project format 4,
  "FUN4", with the drum lanes, levels and ratchets; formats 3, 2 and 1 from older firmware are converted
  when loaded, a format 1 project into track 1).
- Older firmware (no NTRK in `INFO`): one instrument; skip the track UI.

## v4: any track's parameters

- **Finding out:** send `WATCH 3`. v4 firmware answers 3; v3 (0.8) firmware answers 1, does not know
  cmds 31 / 32 (no reply) and never pushes `TRACK_CHANGED`. `WATCH 1` behaves exactly as in v2 / v3
  (reply 1, no `TRACK_CHANGED`). Match the `WATCH` reply by bit 0.
- `TRACK_PARAM` clamps like `SET` scope 0: to the range of that parameter; the engine parameters
  `P_E0..P_E7` to the ranges of that track's engine (the drum track: `P_E0` the kit, the others engine 0,
  as `DESC`). A parameter
  with a fixed range (min = max) keeps its value. A track ≥ NTRK or an id ≥ P_COUNT gets no reply.
  For the selected track it is the same as `SET` scope 0. The drum track's level is still `G_DRLVL`
  (`SET` scope 1 or `TRACK_MIX`); its `P_LEVEL` is not used.
- `TRACK_CHANGED` is never about the selected track (its changes stay `CHANGED` scope 0). Coalesced like
  `CHANGED` (each track and id at most every 20 ms, latest value), and not sent for the editor's own
  `TRACK_PARAM` / `TRACK_MIX` writes. After a selection change (`RELOAD`, or the editor's `TRACK`) the
  device takes the current values as known.

## v5: drum lanes, levels, ratchets (SLOOP 2.0)

- **Finding out:** `INFO` ends with 5. Older firmware ends after NTRK (or the engine names): use the
  v1-v4 commands only.
- **Drum lanes** (`firmware/src/drums.c` `LANE_NOTE`), one per white key from F3: 0 kick (36), 1 kick 2
  (35), 2 snare (38), 3 clap (39), 4 hat (42), 5 open hat (46), 6 pedal (44), 7 rim (37), 8 snare 2 (40),
  9 low tom (43), 10 hi tom (48), 11 crash (49), 12 ride (51), 13 shaker (70), 14 conga (63), 15 cowbell
  (56). A black key plays the lane of the white key left of it.
- **Levels** (2 bits): 0 NORM (as played), 1 GHOST, 2 SOFT, 3 HARD. **Ratchets** (2 bits): 0..3 = x1..x4
  hits in the step.
- **`DRUM_STEP`:** `on` is 16 bits (bit l = lane l), sent as 3 × 7 bits LSB first; `lvl` and `rat` are
  32 bits each (lane l in bits 2l..2l+1), sent as 5 × 7 bits LSB first. A set replaces the whole step;
  the lanes that are off read back with level and ratchet 0. An index ≥ NSTEP gets no reply. Not a push:
  `STEP_CHANGED` with the drum track selected means "re-read `DRUM_STEP` of that index".
- **The drum track through the old commands:** `STEP_GET` / `TRACK_STEP` give its first 4 lanes that are
  on as GM notes (lane order), time NOTE (REST if none), flag 1 (accent) if one of them is HARD, vel 100,
  and lvl / hi / rat 0. A `STEP_SET` / `TRACK_STEP` write puts each note on its nearest lane (GM 35..81,
  `LANE_OF_GM`), level HARD if the accent flag is set, else NORM, ratchet x1; time TIE / REST clears the
  step.
- **Synth steps:** `lvl` holds 2 bits per note (note k in bits 2k..2k+1, the same levels), `rat` 2 bits per
  note (x1..x4); both 8 bits, sent as 7-bit bytes with their top bits in `hi` (bit 0: lvl bit 7, bit 1:
  rat bit 7).
- **`TRACK`** ends with the solo mask (bit per track; GLO + key on the device). A soloed track plays,
  the others are faded out unless soloed too; mute and solo do not change `P_MUTE` of other tracks.
- **The kit** is the drum track's `P_E0`: `DESC` of `P_E0` with the drum track selected is the enum
  `KIT` (sloopDX: 5 FM kits, DX KIT, 808 FM, ELECTRO, METAL, USER; SLOOP 2.x had 34 sample kits). `DESC` of
  `P_E1..P_E7` there still describes engine 0 (unused).

## v6: backup / restore (SLOOP 2.3)

`INFO` ends with 6. Objects: **0** the working project (a `project_t`, as the autosave), **1** the settings
(`persist_t`: colours, low cut, zoom, the panel calibration, the song order, the lights and SYNC word),
**2..5** the projects 1..4 (song sections A..D; length 0 = empty), **6..7** the user preset banks (`up_bank_t`,
16 records each; 0 = empty), **8** the DX7 user bank (sloopDX: 4096 voice bytes as in a .syx; 0 = none; SLOOP 2.3 has the user
sample slots as 32..34 instead). Numbers are 5 × 7 bit (u35, LSB first); data is pack7.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 34 BK_LIST | — | rc (0 ok, 4 no flash), count, then per object: id, length u35, CRC-32 u35 (zlib). Takes a snapshot of the working project and the settings for GET |
| 35 BK_GET | id, offset u35, count (2 × 7 bit, 1..256) | id, rc (0 ok, 1 arguments, 5 the snapshot is gone: LIST again), offset u35, count, pack7 data |
| 36 BK_PUT | op 0 begin: id 0..8, length u35, CRC-32 u35 · op 1 data: id, offset u35, pack7 (≤ 256 bytes, in order) · op 2 commit: id · op 3 abort: id | op, id, rc: 0 ok, 1 arguments, 2 not a valid object (CRC, magic, sizes, ranges), 3 stop the song first (projects), 4 flash, 5 no begin for this object (or more than 15 s ago) |

A restore stages one object in RAM (the project load buffer), checks it at the commit as a load checks it
(projects: magic, size and sum, older formats converted; banks: magic, record size, slot count; settings:
magic, palette, a permutation of the buttons and knobs, a valid song order) and writes it through the usual
A/B commit; the working project is loaded at once (the song must be stopped). Samples are restored with
`SMP_BEGIN` / `SMP_WRITE` / `SMP_END` (the header is the first 480 bytes of the object, the data from byte
512), an empty slot with `SMP_ERASE`. The editor's file is JSON: `{format: "sloop-backup", version: 1,
firmware, date, objects: [{id, len, crc, data (base64)}]}`; it is checked (lengths, CRCs) before anything is
written.

## Notes for the editor

- **One request at a time.** Wait for the reply, about 10–50 ms, before sending the next.
  The device holds only one incoming SysEx frame.
- **Following the device.** With v2 firmware, `WATCH` and `PING` (above). Older firmware pushes
  nothing (no reply to `PING`): poll `DUMP` about every 300–500 ms while the page is visible.
- **Port.** The device's MIDI port is named "Felucca" (USB 1209:0001; SLOOP keeps the name so editors
  and installers find it). Updates use the same
  port with other SysEx (the `F0 22 24 35 …` keys, `00 59 …` frames); never send those
  from the editor. Since SLOOP 2.3 (after Felucca 1.0) the same USB device also has an audio input
  ("Felucca", 44.1 kHz stereo; bcdDevice 3.11): the MIDI port and this protocol are unchanged, and both
  work while the computer records.
- **Voice editing (v8).** A user slot is the edit buffer, as on a DX7: `VOICE_PARAM` changes it live (RAM),
  `BANK_SAVE` is STORE. The device's own EDIT pages edit the same slots; the editor re-reads a slot with
  `VOICE_GET` after a `RELOAD` or when the user asks. The VOICE enum names change with the slot names: re-read
  `DESC` of `P_E0`.
- **Safety.** Only `PROJECT` save, `BANK_END` / `BANK_ERASE` / `BANK_SAVE`, `BK_PUT` and `UP_PUT` / `UP_STORE` / `UP_ERASE` write flash, and only in
  Felucca's own storage; never the app or the update area.
