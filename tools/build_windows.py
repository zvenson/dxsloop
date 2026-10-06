#!/usr/bin/env python3
"""sloopDX on Windows: generate assets here, then compile with the JieLi toolchain in WSL
(run by build-sloopdx.ps1 / INSTALL-SLOOPDX.bat).

No system packages are installed. --toolchain is an existing Linux path;
--sdk is a Windows directory containing cpu/wl82/tools from the pinned SDK.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--distro', required=True)
    ap.add_argument('--toolchain', required=True)
    ap.add_argument('--sdk', type=Path, required=True)
    ap.add_argument('--skip-generate', action='store_true')
    args = ap.parse_args()
    os.chdir(ROOT)
    import build
    if not args.skip_generate:
        build.generate()
    wsl = ['wsl', '-d', args.distro, '--exec']
    def linux_path(p):
        return subprocess.check_output(wsl + ['wslpath', '-a', p.resolve().as_posix()], text=True).strip()
    src, sdk = linux_path(ROOT), linux_path(args.sdk)
    code = 'import sys;sys.path.insert(0,"tools");import build;build.generate=lambda:None;sys.exit(build.main())'
    subprocess.run(['wsl', '-d', args.distro, '--cd', src, '--exec', 'env',
                    'JIELI_TOOLCHAIN=' + args.toolchain, 'AC79_SDK=' + sdk,
                    'python3', '-c', code], check=True)

if __name__ == '__main__':
    main()
