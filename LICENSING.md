# sloopDX licensing

sloopDX is free software: a fork of SLOOP, which is based on Felucca. Its **code** is licensed
under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in `LICENSE`).
The **assets** inherited from Felucca are not part of that licence: the icon atlas
`assets/icons.png` is Copyright (C) 2026 Hügelton Instruments, all rights reserved; its licence
terms will be published later. sloopDX contains no sample data at all: the drum sounds made by
Felucca's `tools/gen_waves.py` (the Hügelton Sample Pack) are not built into its firmware, and
the CC0 sample sets of SLOOP are gone with the sample engines.

Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments (Felucca)
Copyright (C) 2026 isod89 (SLOOP)
sloopDX (C) 2026 Sven Trogus

## What is code (GPL-3.0-only)

Every file in this tree that carries an `SPDX-License-Identifier: GPL-3.0-only` header:

- the firmware: `firmware/` (app, HAL, update loader)
- the build script and tools: `build.sh`, `tools/`
- the web pages (installer, editor) and their tests: `web/` (not the Fukiai font, below)
- the host tests: `tests/`

You may use, study, change and share it under the GPL. If you distribute sloopDX, or
firmware derived from it, you must also give your recipients its complete corresponding
source under the same licence. That includes devices that ship with modified sloopDX
inside.

## Additional permission (GPL-3.0 section 7)

As an additional permission under GPL-3.0 section 7, you may combine Felucca, or a work
based on it, with the Felucca Assets (above), and convey the combination.
This is allowed even though the Felucca Assets are not licensed under the GPL, provided
that:

- you follow the GPL for every part that is not a Felucca Asset; and
- you follow the terms published for the assets.

The Felucca Assets are data (wavetables, icons, sample data). They are not program
code. A firmware image built from the GPL sources with replacement assets, or with no
assets, is entirely governed by the GPL.

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| DX7 core: the msfa engine of Dexed, Copyright 2012 Google Inc., 2016-2025 Pascal Gauthier, ported to integer C (the port and its SLOOP integration are GPL-3.0-only, Copyright (C) 2026 Sven Trogus); the tables are computed on the host with msfa's expressions | Apache-2.0 (msfa) | `firmware/src/dx7_core.c` (`SPDX-License-Identifier: Apache-2.0 AND GPL-3.0-only`), `tools/gen_dx7_tables.c` → `firmware/src/dx7_tables.h`; <https://github.com/asb2m10/dexed> |
| Terminus font 8x16 (ter-u16n) | SIL OFL 1.1 | `assets/fonts/ter-u16n.bdf`, `assets/fonts/Terminus-LICENSE.txt` |
| Fukiai icon font (Hügelton Instruments), web editor only | MIT | `web/fukiai.ttf`, `web/FUKIAI-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

The factory voices (`firmware/src/dx7_bank.h`) and the FM drums are designed in
`tools/gen_dx7_bank.py` and are GPL-3.0-only like the rest of the code. No Yamaha voice data
and no M-VAVE data is in this tree; banks you load (.syx) stay on your device.

## Contributions

Contributions are welcome under GPL-3.0-only. By submitting one, you agree that it may be
combined with the Felucca Assets under the section 7 permission above. Do not submit Yamaha
or M-VAVE voice data.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments. "SLOOP" is the name of
isod89's firmware; sloopDX is a fork of it and is not endorsed by its author.

"DX7" is a trademark of Yamaha Corporation. sloopDX is an independent implementation of the
DX7's synthesis (by way of Dexed) and contains no Yamaha voice data, firmware or ROM content.
It is not affiliated with, endorsed by or supported by Yamaha.

"M-VAVE" and "FM-1" are trademarks of their respective owners. sloopDX is independent
firmware that runs on FM-1 hardware. It is not affiliated with, endorsed by or supported
by those owners, and contains no M-VAVE voice data.

## Radio

sloopDX never enables the Bluetooth / Wi-Fi radio of the hardware.
