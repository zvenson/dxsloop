# Building sloopDX

The build makes three files in `build/`:

| File | What |
| --- | --- |
| `felucca.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `felucca.fwsc` | the installable package (app + loader) |

The DX7 tables (`firmware/src/dx7_tables.h`) and the factory voices (`firmware/src/dx7_bank.h`)
are generated on the host and committed, so the target never needs floating point; regenerate
them with `tools/gen_dx7_tables.c` and `tools/gen_dx7_bank.py` only when you change them.

## Windows (WSL)

`INSTALL-SLOOPDX.bat` builds in a WSL distribution and opens the installer on
`http://localhost:8766/webapp/installer/`. It needs Python 3 with Pillow on Windows, a WSL
distribution with the JieLi toolchain, and the three SDK files (below) in `build/deps/ac79`.
Set `SLOOPDX_WSL_DISTRO` (default `Ubuntu`) and `SLOOPDX_TOOLCHAIN` (a Linux path, default
`/root/.jieli/toolchain`) if yours differ. The site it serves is built into `build/sloopdx-site`.

## Prerequisites (macOS)

- Python 3 with Pillow: `pip3 install Pillow`
- Docker Desktop. The JieLi toolchain is Linux x86-64 only; the build runs each tool in a
  `linux/amd64` `debian:bookworm-slim` container (Rosetta on Apple silicon). Keep the source
  tree in a folder Docker can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files
  (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); they are not part of this tree.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- Node.js (optional, for the web tests).

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build

```
./build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations
(`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`). The link step checks the RAM budget
(.data + .bss ≤ 96 KB; the DX7 state is about 18 KB of it).

`./build.sh --release 1.5` makes a release build: the package identity becomes
`FM-1_915` and the version string `1.5 BETA` (`tools/build.py` adds BETA unless the release
name has it); the package is `build/felucca-1.5.fwsc`. A plain build has the identity `FM-1_900`.

Build options (environment, `0` or `1`; defaults in `firmware/src/felucca.c`):

| Flag | Default | |
| --- | --- | --- |
| `FELUCCA_FLASH` | 1 | settings, presets, projects and the DX7 user bank in flash |
| `FELUCCA_OTA` | 1 | update entry (needs `FELUCCA_FLASH`) |
| `FELUCCA_CDC` | 1 | USB serial console |
| `FELUCCA_UAC` | 1 | USB audio input: the master output, 44.1 kHz stereo (after Felucca 1.0) |
| `FELUCCA_UART` | 1 | TRS MIDI IN (the 3.5 mm jack) |

## Tests

```
sh tests/run_tests.sh
```

Runs the host tests (flash storage, user presets, MIDI parser, update entry, update
loader, a DSP render, the 4-track mix, project formats, the SLICER, the FM drum kits
(`tests/drumkit_test.c`: every kit × lane, choke, burst, click, a WAV demo), the .syx import
(`tests/dx7_syx_test.c`), the regression suite, the command-line installer) and, when Node.js
is installed, the web page tests (`node web/test_web.mjs`). Run it after `./build.sh` (it uses
`build/` and needs `AC79_SDK` set as for the build).

The regression suite (`tests/regress.c`) renders every voice and preset and compares a
hash of each render with `tests/golden.txt`; it also checks levels, voices and the CPU
cost (`tests/cpu_baseline.txt`, `tests/target_budget.txt`). After an intended change of
the sound, `GOLDEN_UPDATE=1 sh tests/run_tests.sh` rewrites the hashes; `BUDGET_UPDATE=1`
does the same for the cost files.

The DX7 core is checked against Dexed with `tests/dx7ref/` (99 % of samples identical, the rest within 1-2 LSB), which needs a
checkout of Dexed's `Source/msfa`:

```
MSFA=<dexed>/Source/msfa sh tests/dx7ref/build.sh && build/host/dx7ref/dx7_exact_test 300
```

## Install

On Windows, `INSTALL-SLOOPDX.bat` builds and opens the web installer (Chrome or Edge). The
`.fwsc` of each release is on the GitHub releases page, and the installer page on GitHub
Pages (`docs/`) carries the current one.

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/felucca.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Or, to install your own build from the web installer, make a local copy of the site and open it from `localhost`
(Web MIDI needs a secure context):

```
python3 web/make_site.py build/felucca.fwsc dev /tmp/sloopdx-site
cd /tmp/sloopdx-site && python3 -m http.server 8000
# open http://localhost:8000/webapp/installer/
```

A DX7 bank (.syx, 32 voices) goes to the device from the editor's Library tab or with
`python3 tools/fm1_bank_upload.py bank.syx`.

Installing firmware is at your own risk. If an install fails and the FM-1 no longer
starts, recovery needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).
