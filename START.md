# sloopDX: erster Build auf dem eigenen Rechner

Das Repo ist ein Git-Checkout mit voller Historie, Branch `sloopdx` (SLOOP 2.2 ist `main`).
Vorher noch nie auf dem FM-1 gelaufen; RAM und CPU auf dem Chip sind offen.

Voraussetzung: Linux x86-64 oder WSL2 (die JieLi-Toolchain gibt es nur dafür), Python 3, gcc.

```sh
# 1. Toolchain und SDK (einmalig)
tools/get_toolchain.sh                      # -> ~/.jieli/toolchain
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
    https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK

# 2. Bauen
./build.sh                                  # -> build/felucca.fwsc (+ RAM-Check: .data+.bss <= 96 KB)

# 3. Host-Tests (ohne Hardware)
sh tests/run_tests.sh

# 4. Erst Emulator, dann Gerät
#    fm1-emulator (Rust) mit build/felucca.fwsc
pip install mido python-rtmidi
python tools/fm1_install.py build/felucca.fwsc     # FM-1 per USB, kein Hub

# Zurück: M-UPGRADE + Original-Firmware. Startet er gar nicht mehr: FM-1-transporter.
```

Wenn `./build.sh` wegen RAM abbricht: Meldung kopieren; dann wandert die User-Bank (4,4 KB)
in den Flash. Mehr in README.md und BUILDING.md.

Remotes: `upstream` zeigt auf SLOOP (isod89/sloop-fm1, nur lesen). Eigenes Repo anlegen und hochladen:

```sh
git remote add origin https://github.com/<account>/sloopdx.git
git push -u origin sloopdx
```
