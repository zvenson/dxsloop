@echo off
title SLOOP installer
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-sloop.ps1"
pause
