# sloopDX: build the firmware, then open the browser installer.
# Needs a WSL distribution with the JieLi toolchain and the SDK files in build/deps/ac79 (BUILDING.md).
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$Distro = if ($env:SLOOPDX_WSL_DISTRO) { $env:SLOOPDX_WSL_DISTRO } else { 'Ubuntu' }
$Toolchain = if ($env:SLOOPDX_TOOLCHAIN) { $env:SLOOPDX_TOOLCHAIN } else { '/root/.jieli/toolchain' }
Write-Host ""
Write-Host "  s l o o p D X   1.1" -ForegroundColor White
Write-Host "  ---- ---- ---- ----" -ForegroundColor DarkGray
Write-Host "== Building the firmware (WSL $Distro)" -ForegroundColor Cyan
python tools/build_windows.py --distro $Distro --toolchain $Toolchain --sdk build/deps/ac79
if ($LASTEXITCODE -ne 0) { throw "The build failed" }
Write-Host "== Making the installer site" -ForegroundColor Cyan
python web/make_site.py build/felucca.fwsc 1.1 build/sloopdx-site
if ($LASTEXITCODE -ne 0) { throw "make_site failed" }
Write-Host ""
Write-Host "Installer: http://localhost:8766/webapp/installer/  (Chrome or Edge, FM-1 on USB)" -ForegroundColor Green
Write-Host "Editor:    http://localhost:8766/webapp/editor/" -ForegroundColor Green
Write-Host "Keep this window open during the install. Ctrl+C stops the server."
Start-Process "http://localhost:8766/webapp/installer/"
python -m http.server 8766 --bind 127.0.0.1 --directory build/sloopdx-site
