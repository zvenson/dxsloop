# sloopDX

SLOOP (fork of Felucca) for the M-VAVE FM-1, reduced to one engine: a DX7 (Dexed's msfa core ported
to integer C, within 1 LSB of Dexed) plus FM drums made with the same core. GPL-3.0. Branch `sloopdx`; `main` is
SLOOP 2.2 as forked. Open work: TODO.md. First build on a machine: START.md.

## Hard rules

- No Yamaha or M-VAVE voice data in the repo, ever: all factory voices are designed in
  `tools/gen_dx7_bank.py`. Users load their own `.syx` banks.
- Keep the SLOOP / Felucca / Dexed credits (about page, README, LICENSING).
- Target: JieLi AC791N, pi32v2, no FPU, 44.1 kHz, CTL = 32-sample blocks. Integer DSP only; tables
  are generated on the host (`tools/gen_dx7_tables.c`, committed as `firmware/src/dx7_tables.h`).
- RAM: .data + .bss ≤ 96 KB (checked by `tools/build.py`). Voice budget: 8 voices over 3 parts,
  plus NDRUM 6 drum voices.

## Where things are

- `firmware/src/dx7_core.c`: the DX7 voice (`dxv_t`, `dx_init`, `dx_compute`, envelopes, LFO). Drums only: a noise
  operator (`dxv_t.noise`, `dx_op_noise`), set by drums.c after `dx_init`; synth voices never (bit-exact as Dexed).
- `firmware/src/eng_dx7.c`: the engine (`ENG_DX7`): factory voices (20: 01–20, the bank 21–52; 1.9 had 17, older saves
  are renumbered on load: `core.h DX_VOICE_FROM_V1`), a low-pass per voice (CUT `P_E6`, RESO `P_E7`), user bank `dx_user` (packed .syx,
  `dx_bank_begin/write/end` for the editor's pieces, `dx_bank_load`, `dx_unpack`, `dx_sanitize`), macros
  BRITE/ATK/DEC/REL/FDBK, DC blocker. Flash side of the bank: `project.c` (`dx_bank_boot/store/erase`,
  storage objects `OBJ_DXBANK0/1` at 0xA0000). Editor cmds 34-38 in `editor.c` (protocol v6).
- `firmware/src/drums.c`: FM drum track, 16 lanes, kits in `FM_KITS` (DX KIT, 808 FM, ELECTRO, METAL: each its own voices, `DX_KIT_VOICE`; the USER kit went in 1.9).
  The voices sum into a stereo bus (lane pan) -> `drums_bus` (DRIVE = the drum track's `P_DIST`, COMP = its `P_CHOR`;
  kit page KNOB 3 / 4) -> each lane's reverb send. Per lane 8 macros + REV (`dext.m`, `core.h drum_ext_t`, saved in the
  project: format 5 "FUN5") and per step a TUNE / DECAY lock of one lane (`dext.lock`, `dlock_*`). MY KIT = kit 5
  (`ukit`, flash object `OBJ_DXKIT`, backup object 9, .syx via `ukit_syx_*`), the dice `dice_kit(seed)`. UI: kit
  screen pages grid / kit / lane (`ui_studio.c`), locks in the SEQ layer (`ui_layers.c`). Editor protocol v10.
- `firmware/src/dx7_bank.h`: generated voices + `DX_DRUM[]` table (`tools/gen_dx7_bank.py`).
- Steps (2.2): `NSTEP` 128 a track, one pool of `STEP_POOL` 256 for the four (`core.h slen_room / slen_set /
  slen_fit_all`: every LEN change goes through them); projects (format 7 since 2.7, format 6 read) store each track's LEN steps in order
  (`project.c proj_capture / proj_apply`, older formats through format 5: `proj_from_v5`). INFO sends 128 as 0.
- OMNI (2.5): ARP MODE 6 = chord harp (black keys chords, white keys strings), 7 = FLW (a pattern follows the
  chord's root): `seq.c omni_*`, `core.h AM_OMNI / AM_FLW / ARP_RUNS`, test `tests/omni_test.c`.
- Effect pages (2.7): one page per effect (`params.c PAGES`, scope `SC_MIX`: a slot with `PG_G` is a global). New
  track params `P_DTONE / P_DTYPE / P_DMIX` (DIST in `fx.c track_dist`), globals `G_CMIX`, `G_RPRE` (`fx_buses`).
  New common params go before P_E0 and need a new project format (`project.c`: freeze the old one, map by count).
- `firmware/src/engines.c`: `ENGINES[] = {&ENG_DX7}`, NENGINES 1 (`core.h`). Old engine numbers in
  projects / presets map through `% NENGINES`.
- `tools/gen_logo.py`: logo and boot splash (`build/gen/sloopdx_logo.h`).

## Build and test

- Firmware: `./build.sh` (needs the JieLi toolchain `tools/get_toolchain.sh` and the AC79 SDK, see
  BUILDING.md). Builds: RAM about 85 KB of 96. Syntax-only without the toolchain:
  `cc -fsyntax-only -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal -DFELUCCA_ID='"FM-1_909"' firmware/src/felucca.c`
- Generated headers: `python3 tools/build.py` step `generate()`, or by hand
  `python3 tools/gen_logo.py build/gen/sloopdx_logo.h` etc. (`build/gen/` is needed by the host tests).
- Host tests: `sh tests/run_tests.sh`. Regression alone:
  `cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/regress tests/regress.c -lm && build/host/regress tests/golden.txt tests/cpu_baseline.txt`
  After an intended sound change: `GOLDEN_UPDATE=1`, review `tests/golden.txt`, commit it.
- Drums: `tests/drumkit_test.c` (every kit × lane, choke, burst, click, WAV demo).
- .syx import: `tests/dx7_syx_test.c` (self-contained; pass a .syx path to test a real dump).
- Against Dexed: `tests/dx7ref/` (needs Dexed's `Source/msfa` plus its tuning-library and MTS-ESP headers;
  2026-10-06 vs Dexed master: 98.97 % of samples identical, the rest 1-2 LSB, worst -62 dB with AMS).
- Web editor tests: `node web/test_web.mjs` (run by `run_tests.sh` when node exists). Bank upload CLI:
  `tests/bank_upload_test.py`.

## Style

Match the existing SLOOP code: C99, 4 spaces, short comments that say why, one compilation unit
(`felucca.c` includes the rest). Commit messages: one line what changed, body only if needed.
