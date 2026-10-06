# sloopDX: was noch zu tun ist

Stand 2026-10-06. Reihenfolge = Priorität. Host-Tests (`sh tests/run_tests.sh`) sind grün;
auf dem FM-1 lief noch nichts.

## 1. Erster echter Build und Gerätetest (siehe START.md)

- [ ] `./build.sh` mit der JieLi-Toolchain. Der RAM-Check in `tools/build.py` (.data + .bss ≤ 96 KB)
      ist die erste Hürde: der DX7 bringt etwa 18 KB mit (3 Parts × 8 Stimmen 9,4 KB, Drums 3,9 KB,
      User-Bank `dx_user` 4,4 KB). Falls zu viel: User-Bank in den Flash (die alten Sample-Slots sind
      frei), `dxv_t` (392 B/Stimme) verkleinern, oder NVOICE pro Part prüfen.
- [ ] Nach dem ersten Build `BUDGET_UPDATE=1 sh tests/run_tests.sh`: `tests/target_budget.txt`
      (Instruktionen der Render-Schleifen aus `build/felucca.dis`) kennt die DX7-Schleifen noch nicht.
- [ ] fm1-emulator, dann Gerät. CPU: 3 Parts + Drums gleichzeitig spielen, auf Aussetzer hören.
      14 DX7-Stimmen (8 + 6 Drums) auf dem pi32v2 sind ungetestet; Host-Kosten sind wie DIGITAL.
- [ ] Pegel am Gerät: Drums gegen Synth-Parts, Klick (Metronom), DUCK.

## 2. .syx-Bank aufs Gerät

- [ ] Editor-Befehl (`firmware/src/editor.c`, Protokoll in `web/EDITOR_PROTOCOL.md`): 4104 Bytes
      Bulk-Dump empfangen → `dx_bank_load()` (`eng_dx7.c`, fertig und getestet: `tests/dx7_syx_test.c`).
- [ ] Im Flash ablegen (project.c / storage), beim Start laden; `dx_user_ok` setzen.
- [ ] Namen U01..U32 aus der Bank (`dx_user_name`) kommen schon aus dem Dump.
- [ ] Das USER-Drumkit spielt dann die ersten 16 Stimmen der Bank als Lanes (`drums.c`).
- [ ] Alternativ/zusätzlich: `tools/fm1_install.py`-artiges CLI-Skript, das die .syx per USB schickt.

## 3. Web-Editor und Installer-Seite (`web/`, `docs/`)

- [ ] Sample-Seiten raus, KIT-Liste = die 5 FM-Kits, Edit-Parameter des DX7 (VOICE, BRITE, ATK, DEC,
      REL, FDBK). Dann `node web/test_web.mjs` wieder in `tests/run_tests.sh` einschalten.
- [ ] `.syx`-Upload im Editor (hängt an 2).
- [ ] Installer-Seite (`docs/index.html` → `docs/webapp/installer/`, lädt `docs/firmware/sloop-2.2.fwsc`):
      auf sloopDX umstellen, eigenes `.fwsc` ablegen, Branding. GitHub Pages aus `docs/`.
- [ ] GitHub Action: Build bei Push (Toolchain von pkgman.jieliapp.com; ob GitHub-Runner den Server
      erreichen, zeigt der erste Lauf), `.fwsc` als Release-Asset und nach `docs/firmware/`.
- [ ] `INSTALL-SLOOP.bat`, `build-sloop.ps1`, `OPEN-EDITOR.bat`, `tools/build_windows.py` umbenennen.

## 4. Klang

- [ ] Werksstimmen (`tools/gen_dx7_bank.py` → `firmware/src/dx7_bank.h`) nach Gehör näher an den
      klassischen DX7-Charakter ziehen. Eigene Patches bleiben Pflicht: keine Yamaha- oder M-VAVE-Bytes.
- [ ] Pegel-Trims der 17 Presets (`firmware/src/preset_trim.h`, alle 0; `tools/level_presets.py`).
- [ ] FM-Drums: Lanes liegen in ~10 dB (`build/host/drum-kits.txt`); Feinabgleich nach Gehör,
      Pedal-Hat und Ride waren zu leise und sind schon angehoben.
- [ ] Weitere Macro-Ideen: LFO-Tiefe, Algorithmus-Umschaltung, Operator-Mute.

## 5. Doku und Namen

- [ ] `SLOOP.md` (Handbuch), `BUILDING.md`, `LICENSING.md`, `DEMARRAGE-RAPIDE-FR.md` beschreiben
      noch Samples und die alten Engines. Kürzen auf DX7 + FM-Drums; LICENSING: Dexed/msfa (Apache-2.0)
      aufnehmen, Sample-Lizenzen raus.
- [ ] ABOUT-Seite (`ui_menu.c`): GitHub-Zeile zeigt noch `ISOD89/SLOOP-FM1`; auf das eigene Repo ändern,
      sobald es existiert. SLOOP/Felucca-Credits bleiben (GPL).
- [ ] isod89 per Issue über den Fork informieren (guter Ton, keine Pflicht).
- [ ] Projekt-Versionierung: `FELUCCA_VERSION "sloopDX 1.0"` in `ui.c`, Paket-Identität `PRODUCT`
      in `tools/build.py` (FM-1_9XY).

## Nicht vergessen

- `tests/dx7ref/` prüft den DX7-Core bit-genau gegen Dexed; braucht Dexed-Quellen:
  `MSFA=<dexed>/Source/msfa sh tests/dx7ref/build.sh && build/host/dx7ref/dx7_exact_test 300`.
- Nach jeder Klangänderung: `GOLDEN_UPDATE=1 sh tests/run_tests.sh`, Diff von `tests/golden.txt`
  ansehen, mit committen.
- Zurück zur Original-Firmware: M-UPGRADE. Totalausfall: FM-1-transporter (XIAO RP2040).
