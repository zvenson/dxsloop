@echo off
title sloopDX installer
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-sloopdx.ps1"
pause
