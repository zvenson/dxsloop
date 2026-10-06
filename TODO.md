# sloopDX: was noch zu tun ist

Stand 2026-10-06 (abends, nach dem Merge von SLOOP 2.3). Reihenfolge = Priorität. Host-Tests (`sh tests/run_tests.sh`) sind grün, der
Firmware-Build läuft (`./build.sh`: RAM 85 KB von 96 KB); auf dem FM-1 lief noch nichts.

## 1. Gerätetest (siehe START.md)

- [x] Erster Build mit der JieLi-Toolchain: RAM .data + .bss 84636 B von 98304 (nach dem 2.3-Merge: USB-Audio) (die User-Bank bleibt im
      RAM; 13 KB Luft). `tests/target_budget.txt` kennt jetzt die DX7-Schleifen (dx7_render, dx_compute,
      dx_op, dx_env_advance, mix_block).
- [ ] fm1-emulator, dann Gerät. CPU: 3 Parts + Drums gleichzeitig spielen, auf Aussetzer hören.
      14 DX7-Stimmen (8 + 6 Drums) auf dem pi32v2 sind ungetestet; Host-Kosten sind wie DIGITAL.
- [ ] Pegel am Gerät: Drums gegen Synth-Parts, Klick (Metronom), DUCK, USB-Audio. Der DX7-Ausgang ist 6 dB
      lauter als zuerst (eng_dx7.c, `>> 10`), die Preset-Trims sind gemessen (siehe 4). Achtung: der DX7 hat
      einen höheren Crest-Faktor als SLOOPs Engines; 8 E-Piano-Stimmen bei Velocity 110 treiben den Master-
      Limiter (SLOOPs USB-AUDIO-FULL-Test läuft deshalb mit 3 STRINGS-Noten). Klingt es am Gerät zu
      gedrückt: zurück auf `>> 11`, `preset_trim.h` auf 0 und `tools/level_presets.py` zweimal laufen lassen.
- [ ] .syx-Bank am Gerät laden (Editor oder `tools/fm1_bank_upload.py`), Neustart, Bank noch da?

## 1b. Gefunden beim Bank-Upload über den ALSA-Sequencer (2026-10-06)

- [ ] `usb.c ota_wire_send`: eine lange Antwort (BANK_INFO ~330 Bytes, DESC mit 49 Namen) wird nach 200 ms
      ohne Platz in der Senderingschlange mitten im SysEx abgebrochen, ohne F7. Linux (ALSA seq) wartet dann
      auf das Ende und verschluckt alles danach. Fix: nie mitten im Frame aufgeben (ganzen Frame vorher auf
      Platz prüfen, sonst gar nicht senden), oder bei Abbruch ein F7 nachschieben. Über Chrome / WebMIDI
      tritt es nicht auf.
- [ ] `seq_bank_upload.py` (in ~/sloopDX/banks, außerhalb des Repos): Bank-Upload ohne python-rtmidi über
      aseqsend / aseqdump; ins Repo als tools/, wenn der Fix oben drin ist.

## 2. .syx-Bank aufs Gerät

- [x] Editor-Befehle 34..38 (BANK_BEGIN / WRITE / END / INFO / ERASE, Protokoll v6 in
      `web/EDITOR_PROTOCOL.md`): die Bank kommt in 8 Stücken zu 512 Bytes, Prüfsumme am Ende.
- [x] Flash: zwei Storage-Objekte `OBJ_DXBANK0/1` bei 0xA0000..0xA3FFF (project.c `dx_bank_boot` /
      `dx_bank_store` / `dx_bank_erase`), beim Start geladen.
- [x] Namen U01..U32 aus der Bank; das USER-Drumkit spielt die ersten 16 Stimmen.
- [x] CLI: `python3 tools/fm1_bank_upload.py bank.syx` (`--names`, `--erase`, `--check`);
      Test `tests/bank_upload_test.py` gegen ein simuliertes Gerät.
- [ ] Am Gerät prüfen (siehe 1).

## 3. Web-Editor und Installer-Seite (`web/`, `docs/`)

- [x] Editor ohne Sample-Seiten, KIT-Liste = 5 FM-Kits, DX7-Parameter, .syx-Upload im Library-Tab;
      `node web/test_web.mjs` läuft wieder in `tests/run_tests.sh`.
- [x] Installer-Seite auf sloopDX (`web/index_pkg.html` Vorlage, `docs/` erzeugt mit
      `python3 web/make_site.py build/felucca.fwsc 1.5 docs`); `docs/firmware/sloopdx-1.5.fwsc`.
- [x] GitHub Action `.github/workflows/build.yml` (Build, Tests, Artefakt, Release bei `v*`-Tag).
      Ob der Runner pkgman.jieliapp.com und gitee erreicht, zeigt der erste Lauf.
- [x] `INSTALL-SLOOPDX.bat`, `build-sloopdx.ps1`, `OPEN-EDITOR.bat`, `tools/build_windows.py`.
- [x] Repo auf GitHub: github.com/zvenson/dxsloop (Fork von isod89/sloop-fm1), SLOOP 2.3 ist gemergt
      (USB-Audio, MIDI-IN-Buchse, MIDI-Clock, REC-Modi, Lichter, Backup; Backup-Objekt 8 = DX7-Bank).
      `main` folgt `sloopdx` per Fast-Forward (`git push origin sloopdx:main`).
- [x] Webseite: https://dx7.designburgapps.com (Installer, `/webapp/editor/`, `?mock=1` ohne Gerät). Läuft
      auf dem Pi in `~/docker/sloopdx-site` (nginx + eigener cloudflared-Tunnel `sloopdx-site`,
      `deploy/pi/`). Neuer Stand: pushen, dann auf dem Pi `~/docker/sloopdx-site/update.sh`.

## 4. Klang

- [x] Pegel-Trims der 17 Presets gemessen (`tools/level_presets.py`, BS.1770, Ziel -15 LUFS) und in
      `firmware/src/preset_trim.h`. FM BASS, SLAP BASS, SUB BASS, MARIMBA, CLAV, KOTO liegen am
      Anschlag (+19..+23 = bis +11,5 dB): diese Stimmen sind in `tools/gen_dx7_bank.py` zu leise
      (kurze Hüllkurven, niedrige Carrier-Level). Dort anheben, dann `preset_trim.h` auf 0 setzen und
      das Tool zweimal laufen lassen (Anleitung im Skript).
- [ ] Werksstimmen (`tools/gen_dx7_bank.py` → `firmware/src/dx7_bank.h`) nach Gehör näher an den
      klassischen DX7-Charakter ziehen. Eigene Patches bleiben Pflicht: keine Yamaha- oder M-VAVE-Bytes.
- [ ] FM-Drums: Lanes liegen in ~10 dB (`build/host/drum-kits.txt`); Feinabgleich nach Gehör.
- [ ] Weitere Macro-Ideen: LFO-Tiefe, Algorithmus-Umschaltung, Operator-Mute.

## 5. Doku und Namen

- [x] `SLOOP.md`, `BUILDING.md`, `LICENSING.md`, `DEMARRAGE-RAPIDE-FR.md` auf DX7 + FM-Drums; LICENSING
      mit Dexed/msfa (Apache-2.0), Sample-Lizenzen raus.
- [x] ABOUT-Seite (`ui_menu.c`): `GITHUB.COM/ZVENSON/DXSLOOP`.
- [ ] isod89 per Issue über den Fork informieren (guter Ton, keine Pflicht).
- [ ] Versionierung: `FELUCCA_VERSION "sloopDX 1.5"` (`ui.c`), Paket FM-1_900; das erste Release mit
      `./build.sh --release 1.5` (Identität FM-1_915) und Tag `v1.5`.

## Nicht vergessen

- `tests/dx7ref/` prüft den DX7-Core gegen Dexed (Stand 2026-10-06: 98,97 % der Samples identisch, Rest 1-2 LSB); braucht Dexed-Quellen:
  `MSFA=<dexed>/Source/msfa sh tests/dx7ref/build.sh && build/host/dx7ref/dx7_exact_test 300`.
- Nach jeder Klangänderung: `GOLDEN_UPDATE=1 sh tests/run_tests.sh`, Diff von `tests/golden.txt`
  ansehen, mit committen. Nach Änderungen an den Render-Schleifen: `BUDGET_UPDATE=1`.
- `tools/level_presets.py` braucht numpy und scipy (`pip install numpy scipy`).
- Zurück zur Original-Firmware: M-UPGRADE. Totalausfall: FM-1-transporter (XIAO RP2040).
