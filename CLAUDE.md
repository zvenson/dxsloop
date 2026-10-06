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

- `firmware/src/dx7_core.c`: the DX7 voice (`dxv_t`, `dx_init`, `dx_compute`, envelopes, LFO).
- `firmware/src/eng_dx7.c`: the engine (`ENG_DX7`): factory voices, user bank `dx_user` (packed .syx,
  `dx_bank_begin/write/end` for the editor's pieces, `dx_bank_load`, `dx_unpack`, `dx_sanitize`), macros
  BRITE/ATK/DEC/REL/FDBK, DC blocker. Flash side of the bank: `project.c` (`dx_bank_boot/store/erase`,
  storage objects `OBJ_DXBANK0/1` at 0xA0000). Editor cmds 34-38 in `editor.c` (protocol v6).
- `firmware/src/drums.c`: FM drum track, 16 lanes, kits in `FM_KITS` (DX KIT, 808 FM, ELECTRO, METAL: each its own voices, `DX_KIT_VOICE`; USER).
- `firmware/src/dx7_bank.h`: generated voices + `DX_DRUM[]` table (`tools/gen_dx7_bank.py`).
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
