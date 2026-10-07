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

## 1a. Notizen vom Gerätetest (2026-10-06), erledigt in 2.0

- [x] Cutoff / Resonanz auf den Knobs im EDIT-Modus, mit Anzeige beim Drehen: Tiefpass (tsvf, dsp.c) pro Stimme hinter
      dem DX7-Ausgang (`eng_dx7.c dx7_render`), CUT = `P_E6` (127 = offen: bitgleich wie ohne), RESO = `P_E7`; ENV/LFO-DEST
      FLT bewegen den Cutoff (SHP weiter die Helligkeit). In der Voice-Liste KNOB 1 CUT, KNOB 2 RESO, Wert unten im Bild
      (`ui_say`). Cheat Sheets korrigiert. Alte Projekte / User-Presets / Editor-Library laden CUT offen
      (`core.h DX_CUT_OPEN`, `project.c dxv`, `UP_VER 2`, editor `dxvOf` / `dxMigrate`).
- [x] Drums zu leise: Drum-Bus von −3 dB auf 0 dB (`drums.c`, `lvl * 200` statt 142); im 4-Spur-Mix −20.5 LUFS
      (Bass −16.8), Peaks −2.9 dBFS. Am Gerät nachhören.
- [x] Drum-Effekte: DRIVE (Soft-Clip, `P_DIST` der Drum-Spur) und COMP (Peak-Follower, 4:1, Make-up bis +5.4 dB,
      `P_CHOR` der Drum-Spur) auf dem Drum-Bus vor Pan und Hall-Send (`drums.c drums_bus`). Bedienung: Kit-Seite KNOB 3 / 4
      (Hall und Pan bleiben auf HOME). Test: `tests/drumkit_test.c`.
- [x] Drei moderne Bässe: DEEP SUB, 808 SUB, REESE (vor INIT VOICE; Werk 01–20, Bank 21–52). Alte Spielstände werden
      beim Laden umnummeriert (`core.h DX_VOICE_FROM_V1`). Am Gerät nachhören (808-Pitch-Drop, Reese-Schwebung).

## 1m. 3.1 / 3.2, erledigt

- [x] Synth statt Groovebox (Nutzer-Feedback „clear visual envelopes like Dexed or Serum“): Operator-Solo; im
      Algorithmusbild Output Level und Live-Pegel je Operator. Web-Editor: Hüllkurven zum Ziehen (↔ Rate, ↕ Level).
      3.1 hatte die Hüllkurve auch als Grafik auf dem FM-1 (KNOB 1–4 = Rate/Level, Live-Punkt): für den Nutzer
      unverständlich und zu viel Platz, in 3.2 wieder raus.

## 1l. 3.0, erledigt

- [x] FX: erste Seite SENDS, die vier Sends als Drehregler in einer Reihe (KNOB 1–4 DIST CHO DLY REV), dann die Effektseiten.
- [x] OMNI: der Akkordname kommt nur noch beim Drücken einer Akkordtaste, nicht bei jedem abgespielten Akkord-Step.

## 1k. 2.9, erledigt

- [x] OMNI: die Saiten (weiße Tasten) werden mit aufgenommen, als die Note, die sie gespielt haben („das ist nämlich so geil“).

## 1j. 2.8, erledigt

- [x] OMNI: aufgenommene Akkorde spielten auf MONO / LEGATO-Spuren (viele Presets) nur einen Ton; die Steps einer OMNI-Spur
      klingen jetzt immer mehrstimmig wie die Tasten (seq.c seq_on, Test omni_test playback_mono_test).

## 1i. 2.7, erledigt

- [x] Effekte: eine Seite je Effekt (vorher FX = vier Sends, DLY, REV/CHO gemischt; „versteht kein Mensch“). DIST: DRIVE TONE
      TYPE (SOFT HARD FUZZ CRUSH) MIX; CHORUS: SEND RATE DEPTH MIX; DELAY: SEND TIME FDBK MIX (COLR ohne Seite, im Editor);
      REVERB: SEND SIZE DAMP PRE (0–90 ms). Projektformat 7 (FUN7), 6 wird nach Anzahl umgesetzt; Editor-Dateien von 2.6 auch.

## 1h. 2.6, erledigt

- [x] OMNI-Knöpfe fest beschriftet: spielen ihren Akkord unabhängig von ROOT (am Gerät stand ROOT F, der F-Knopf spielte Bb); nur TRN
      transponiert, FLW folgt dem klingenden Grundton. OMNI immer mehrstimmig, auch auf einer MONO-Spur.

## 1g. 2.5, erledigt

- [x] OMNI: ARP MODE OMNI macht die Tasten zur Akkord-Harfe (nach dem Omnichord, Akkordknöpfe wie FoMni-1): 11 schwarze Tasten = Akkorde
      F C G · Dm Am · Em G7 E7 · D7 Bb · A7 (fest, TRN transponiert), 16 weiße = Saiten über die Akkordtöne ab G3. Akkorde werden
      aufgenommen und setzen beim Abspielen den Akkord wieder, Saiten nur live. ARP MODE FLW: das Pattern einer Spur folgt dem
      Grundton (`seq.c omni_*`, `tests/omni_test.c`).

## 1f. 2.4, erledigt

- [x] Lane-Seite: folgt dem von Hand gespielten Sound (Taste, MIDI; `seq.c drum_input` -> `drum_hand`) auch während der Wiedergabe; die Schläge des Sequencers verschieben sie nicht.

## 1e. 2.3, erledigt

- [x] Voice-Liste: zwei Mini-Regler oben rechts (cut / res), leuchten wenn gesetzt (CUT < 127, RESO > 0), sonst grau.

## 1d. 2.2: 128 Steps, Bugs aus 2.1, erledigt

- [x] Spuren bis 128 Steps, gemeinsamer Pool von 256 (`core.h slen_room`): eine Spur höchstens 128, die anderen den Rest,
      sonst "STEPS FULL". OCT−/OCT+ blättern 8 Seiten, schwarze Tasten F#3..A#4 wählen Seite 1-8. Projektformat 6 (nur die
      Steps innerhalb LEN), Format 5/4/.. laden weiter. Editor-Protokoll 11 (INFO: 0 = 128 Steps). RAM jetzt 93.4 KB von 96.
- [x] Lane-Seite: kein Würfeln per Doppel-Tipp mehr (Prellen / Doppel-Tap landete nie auf der Lane-Seite und überschrieb MY KIT
      ungefragt). Würfeln auf Lane-Seite 3: KNOB 3 Kit, KNOB 4 nur dieser Sound, je zweimal drehen (AGAIN). Layer-Tasten
      entprellt (ein Druck < 40 ms nach dem Loslassen zählt nicht). Toter Tasten-Code in `drum_screen_input` entfernt.
- [x] Lane-Seite zeigt "lane" und die Seite (1/3); Meldungszeile "SNARE DECAY +8".
- [x] Bank-Sounds (21-52) viel leiser als 1-20: PRESETS setzte beim Laden einer Bank-Stimme alle Quick-Knobs auf 0, auch
      CUT (seit 2.0 = Filter zu, 30 Hz). Jetzt die Engine-Defaults (CUT 127). Dazu DX_BANK_TRIM -6 dB -> 0: gemessen an den
      8 ROM-Bänken (Median -29 LUFS mit -6 dB, Werk -23). Alte Projekte / Presets / Editor-Library mit Bank-Stimme + CUT 0
      öffnen beim Laden (`core.h DX_CUT_FIX`, PROJ_DXV 2, UP_VER 3, editor dxv 3).
- [x] Cheat Sheets: HOME-Knobs (Swing · Level · Steps · Pan), EDIT-Seiten (VOICE BRITE ATK DEC / REL FDBK CUT RESO),
      Voice-Liste KNOB 1-4 = CUT RESO REL FDBK.
- [ ] Am Gerät: Version prüfen (HOME halten -> ABOUT), Lane-Seite, Würfel, 128-Step-Spur, CPU.

## 1c. Ausbau FM-Drum-Engine (2.1), erledigt

- [x] Drum-Editor pro Lane: TUNE DECAY SWEEP BRIGHT NOISE LEVEL PAN CHOKE (+ REV), Kit-Seite EDIT -> Lane-Seite, PRESETS
      blättert, Wert in der Meldungszeile; im Projekt (Format 5 "FUN5", FUN4 lädt neutral). `drums.c dext`, `drum_voice`.
- [x] Noise-Operator (`dx7_core.c dx_op_noise`, nur Drums, Maske in `dx_drum_t.noise`); Werks-Kits nachgezogen
      (Snare, Clap, Hats, Becken, Shaker; FM-Feedback-Rauschen -> echtes Rauschen). Test in `drumkit_test.c`.
- [x] Parameter-Locks: 2 Bytes pro Step (eine Lane: TUNE ±16, DECAY ±48), SEQ-Layer: Step halten + KNOB 4 / PRESETS, ★.
- [x] MY KIT: Kit 5, SAVE 2× auf der Kit-Seite (Makros eingebacken), Flash `OBJ_DXKIT` 0xC2000; .syx (Stimmen 1-16 +
      Tabelle 17-32 "KIT DATA") über Editor (Backup-Objekt 9, Umwandlung im Browser, gegen C getestet).
- [x] Würfel-Kit: EDIT 2×, Regeln pro Lane, Seed oben rechts (#4711), im Editor mit Seed wiederholbar (KIT_DICE 47).
- [x] Drum-Bus: Lane-Gain -> Summe (Stereo, Lane-Pan) -> Drive -> Kompressor -> Reverb-Send pro Lane.
- [ ] Am Gerät nachhören: Noise-Snare / Hats, Würfel-Kits, Locks, CPU mit 6 Drum-Stimmen + 8 Synth-Stimmen.

## 1b. Gefunden beim Bank-Upload über den ALSA-Sequencer (2026-10-06)

- [x] (1.9, Release) `usb.c ota_wire_send`: eine lange Antwort (BANK_INFO ~330 Bytes, DESC mit 49 Namen) wird nach 200 ms
      ohne Platz in der Senderingschlange mitten im SysEx abgebrochen, ohne F7. Linux (ALSA seq) wartet dann
      auf das Ende und verschluckt alles danach. Fix: nie mitten im Frame aufgeben (ganzen Frame vorher auf
      Platz prüfen, sonst gar nicht senden), oder bei Abbruch ein F7 nachschieben. Über Chrome / WebMIDI
      tritt es nicht auf. Gelöst: Wartezeit 500 ms, ein abgebrochener Frame wird beim nächsten Senden mit F7
      beendet.
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
      `python3 web/make_site.py build/felucca.fwsc 1.9 docs`); `docs/firmware/sloopdx-1.9.fwsc`.
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

- [x] `SLOOP.md`, `BUILDING.md`, `LICENSING.md` auf DX7 + FM-Drums; LICENSING
      mit Dexed/msfa (Apache-2.0), Sample-Lizenzen raus.
- [x] ABOUT-Seite (`ui_menu.c`): `GITHUB.COM/ZVENSON/DXSLOOP`.
- [ ] isod89 per Issue über den Fork informieren (guter Ton, keine Pflicht).
- [ ] Versionierung: `FELUCCA_VERSION "sloopDX 1.9"` (`ui.c`), Paket FM-1_900; das erste Release mit
      `./build.sh --release 1.9` (Identität FM-1_919) und Tag `v1.9`.

## Nicht vergessen

- `tests/dx7ref/` prüft den DX7-Core gegen Dexed (Stand 2026-10-06: 98,97 % der Samples identisch, Rest 1-2 LSB); braucht Dexed-Quellen:
  `MSFA=<dexed>/Source/msfa sh tests/dx7ref/build.sh && build/host/dx7ref/dx7_exact_test 300`.
- Nach jeder Klangänderung: `GOLDEN_UPDATE=1 sh tests/run_tests.sh`, Diff von `tests/golden.txt`
  ansehen, mit committen. Nach Änderungen an den Render-Schleifen: `BUDGET_UPDATE=1`.
- `tools/level_presets.py` braucht numpy und scipy (`pip install numpy scipy`).
- Zurück zur Original-Firmware: M-UPGRADE. Totalausfall: FM-1-transporter (XIAO RP2040).
