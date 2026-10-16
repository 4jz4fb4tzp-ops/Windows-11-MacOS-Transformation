@echo off
rem One-click setup: installs Windhawk, all mods and settings.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup\Setup.ps1" %*
